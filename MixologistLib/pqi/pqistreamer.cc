/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-6, Robert Fernie
 *
 *  This file is part of the Mixologist.
 *
 *  The Mixologist is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  The Mixologist is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the Mixologist; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 ****************************************************************/

#include <iostream>
#include <fstream>
#include <sstream>
#include "util/debug.h"

#include "pqi/pqistreamer.h"
#include "pqi/pqinotify.h"

#include "serialiser/serial.h"
#include "serialiser/baseitems.h"  /***** For FileData *****/

#include "pqi/friendsConnectivityManager.h" //For updating last heard from stats

const int PQISTREAM_ABS_MAX = 900000000; /* ~900 MB/sec (actually per loop) */

/* This removes the print statements (which hammer pqidebug) */
/***
#define NeTITEM_DEBUG 1
 ***/


pqistreamer::pqistreamer(Serialiser *rss, std::string id, unsigned int librarymixer_id, BinInterface *bio_in, int bio_flags_in)
    :PQInterface(id, librarymixer_id), serialiser(rss), bio(bio_in), bio_flags(bio_flags_in),
     pkt_wpending(NULL),
     totalRead(0), totalSent(0),
     currRead(0), currSent(0),
     avgReadCount(0), avgSentCount(0) {
    avgLastUpdate = currReadTS = currSentTS = time(NULL);

    /* allocated once */
    pkt_rpend_size = getPktMaxSize();
    pkt_rpending = malloc(pkt_rpend_size);
    reading_state = reading_state_initial;

    // avoid uninitialized (and random) memory read.
    memset(pkt_rpending,0,pkt_rpend_size);

    // 100 B/s (minimal)
    setMaxRate(true, 0.1);
    setMaxRate(false, 0.1);
    setRate(true, 0);
    setRate(false, 0);

    {
        std::ostringstream out;
        out << "pqistreamer::pqistreamer()";
        out << " Initialisation!" << std::endl;
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    if (!bio_in) {
        std::ostringstream out;
        out << "pqistreamer::pqistreamer()";
        out << " NULL bio, FATAL ERROR!" << std::endl;
        pqioutput(PQL_ALERT, PQISTREAMERZONE, out.str().c_str());
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Fatal error", "Unknown total failure of network interface.");

        exit(1);
    }

    failed_read_attempts = 0;                      // reset failed read, as no packet is still read.

    return;
}

pqistreamer::~pqistreamer() {
        QMutexLocker stack(&streamerMtx);

    {
        std::ostringstream out;
        out << "pqistreamer::~pqistreamer()";
        out << " Destruction!" << std::endl;
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    if (bio_flags & BIN_FLAGS_NO_CLOSE) {
        std::ostringstream out;
        out << "pqistreamer::~pqistreamer()";
        out << " Not Closing BinInterface!" << std::endl;
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    } else if (bio) {
        std::ostringstream out;
        out << "pqistreamer::~pqistreamer()";
        out << " Deleting BinInterface!" << std::endl;
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());

        delete bio;
    }

    /* clean up serialiser */
    if (serialiser)
        delete serialiser;

    // clean up outgoing. (cntrl packets)
    while (out_pkt.size() > 0) {
        void *pkt = out_pkt.front();
        out_pkt.pop_front();
        free(pkt);
    }

    // clean up outgoing (data packets)
    while (out_data.size() > 0) {
        void *pkt = out_data.front();
        out_data.pop_front();
        free(pkt);
    }

    if (pkt_wpending) {
        free(pkt_wpending);
        pkt_wpending = NULL;
    }

    free(pkt_rpending);

    // clean up outgoing.
    while (incoming.size() > 0) {
        NetItem *i = incoming.front();
        incoming.pop_front();
        delete i;
    }
    return;
}

// Get/Send Items.
int pqistreamer::SendItem(NetItem *si) {
    {
        std::ostringstream out;
        out << "pqistreamer::SendItem():" << std::endl;
        si->print(out);
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    // This is called by different threads, and by threads that are not the handleoutgoing thread,
    // so it should be protected by a mutex
    QMutexLocker stack(&streamerMtx);

    {
        std::ostringstream out;
        out << "pqistreamer::SendItem()";
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    /* decide which type of packet it is */
    FileData *data = dynamic_cast<FileData *>(si);
    bool isControl = (data == NULL);

    uint32_t pktsize = serialiser->size(si);
    void *ptr = malloc(pktsize);

    if (serialiser->serialise(si, ptr, &pktsize)) {
        if (isControl) {
            out_pkt.push_back(ptr);
        } else {
            out_data.push_back(ptr);
        }
    } else {
        /* cleanup serialiser */
        free(ptr);

        std::ostringstream out;
        out << "pqistreamer::SendItem() Null Pkt generated!";
        out << std::endl;
        out << "Caused By: " << std::endl;
        si->print(out);
        pqioutput(PQL_ALERT, PQISTREAMERZONE, out.str().c_str());
    }

    if (!(bio_flags & BIN_FLAGS_NO_DELETE)) {
        delete si;
    }

    return 1;
}

NetItem *pqistreamer::GetItem() {
    {
        std::ostringstream out;
        out << "pqistreamer::GetItem()";
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    std::list<NetItem *>::iterator it;

    it = incoming.begin();
    if (it == incoming.end()) {
        return NULL;
    }

    NetItem *osr = (*it);
    incoming.erase(it);
    return osr;
}

int pqistreamer::tick() {
    {
        std::ostringstream out;
        out << "pqistreamer::tick()";
        out << std::endl;
        out << PeerId() << ": currRead/Sent: " << currRead << "/" << currSent;
        out << std::endl;

        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    bio->tick();

    if (!(bio->isactive())) {
        return 0;
    }


    /* must do both, as outgoing will catch some bad sockets,
     * that incoming will not */
    handleincoming();
    handleoutgoing();

    /* give details of the packets */
    {
        std::list<void *>::iterator it;

        std::ostringstream out;
        out << "pqistreamer::tick() Queued Data:";
        out << " for " << PeerId();

        if (bio->isactive()) {
            out << " (active)";
        } else {
            out << " (waiting)";
        }
        out << std::endl;

        {
            QMutexLocker stack(&streamerMtx);
            int total = 0;

            for (it = out_pkt.begin(); it != out_pkt.end(); it++) {
                total += getNetItemSize(*it);
            }

            out << "\t Out Packets [" << out_pkt.size() << "] => " << total;
            out << " bytes" << std::endl;

            total = 0;
            for (it = out_data.begin(); it != out_data.end(); it++) {
                total += getNetItemSize(*it);
            }

            out << "\t Out Data    [" << out_data.size() << "] => " << total;
            out << " bytes" << std::endl;

            out << "\t Incoming    [" << incoming.size() << "]";
            out << std::endl;
        }

        pqioutput(PQL_DEBUG_BASIC, PQISTREAMERZONE, out.str().c_str());
    }

    /* if there is more stuff in the queues */
    if ((incoming.size() > 0) || (out_pkt.size() > 0) || (out_data.size() > 0)) {
        return 1;
    }
    return 0;
}

/**************** HANDLE OUTGOING TRANSLATION + TRANSMISSION ******/

int pqistreamer::handleoutgoing() {
    QMutexLocker stack(&streamerMtx);

    {
        std::ostringstream out;
        out << "pqistreamer::handleoutgoing()";
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    int sentbytes = 0;

    std::list<void *>::iterator it;

    if (!(bio->isactive())) {
        /* if we are not active - clear anything in the queues. */
        for (it = out_pkt.begin(); it != out_pkt.end(); ) {
            free(*it);
            it = out_pkt.erase(it);

            std::ostringstream out;
            out << "pqistreamer::handleoutgoing() Not active->Clearing Pkt!";
            pqioutput(PQL_DEBUG_BASIC, PQISTREAMERZONE, out.str().c_str());
        }
        for (it = out_data.begin(); it != out_data.end(); ) {
            free(*it);
            it = out_data.erase(it);

            std::ostringstream out;
            out << "pqistreamer::handleoutgoing() Not active->Clearing DPkt!";
            pqioutput(PQL_DEBUG_BASIC, PQISTREAMERZONE, out.str().c_str());
        }

        /* also remove the pending packets */
        if (pkt_wpending) {
            free(pkt_wpending);
            pkt_wpending = NULL;
        }

        outSentBytes(sentbytes);
        return 0;
    }

    int maxbytes = outAllowedBytes();
    bool allSent = true;
    // a very simple round robin
    while (allSent) {
        allSent = false;

        if (!bio->cansend()) {
            outSentBytes(sentbytes);
            pqioutput(PQL_DEBUG_ALERT, PQISTREAMERZONE, "pqistreamer::handleoutgoing() Bio not ready for sending");
            return 0;
        }
        if (maxbytes < sentbytes) {
            outSentBytes(sentbytes);
            pqioutput(PQL_DEBUG_ALERT, PQISTREAMERZONE, "pqistreamer::handleoutgoing() Max bytes sent, max is: " + QString::number(maxbytes));
            return 0;
        }

        // send a out_pkt, else send out_data. unless
        // there is a pending packet.
        if (!pkt_wpending) {
            if (out_pkt.size() > 0) {
                pkt_wpending = *(out_pkt.begin());
                out_pkt.pop_front();
            } else if (out_data.size() > 0) {
                pkt_wpending = *(out_data.begin());
                out_data.pop_front();
            }
        }

        if (pkt_wpending) {
            // write packet.
            int bytes_to_send = getNetItemSize(pkt_wpending);
            int bytes_sent;

            if (bytes_to_send != (bytes_sent = bio->senddata(pkt_wpending, bytes_to_send))) {
                std::ostringstream out;
                out << "Problems with Send Data! (only " << bytes_sent << " bytes sent" << ", total pkt size=" << bytes_to_send;
                pqioutput(PQL_DEBUG_BASIC, PQISTREAMERZONE, out.str().c_str());

                outSentBytes(sentbytes);
                // pkt_wpending will kept til next time.
                // ensuring exactly the same data is written (openSSL requirement).
                return -1;
            }

            free(pkt_wpending);
            pkt_wpending = NULL;

            sentbytes += bytes_to_send;
            allSent = true;
        }
    }
    outSentBytes(sentbytes);
    return 1;
}


/*
This long and complicated function is basically broken into two parts.
In the first, labeled start_packet_read, we attempt to read the basic block, which is the minimum packet size.
Once we have that we mark reading_state to started, and can read the full size in the header from the basic block.
Once we have the full size, we proceed to read in and deserialize the packet.
If when we finish deserializing, we still have more available to read and haven't hit our transfer cap yet, we loop back to start_packet_read.
*/
int pqistreamer::handleincoming() {
    int readbytes = 0;
    static const int max_failed_read_attempts = 2000;

    pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, "pqistreamer::handleincoming()");

    if (!(bio->isactive())) {
        reading_state = reading_state_initial;
        inReadBytes(readbytes);
        return 0;
    }

    // enough space to read any packet.
    int maxlen = pkt_rpend_size;
    void *block = pkt_rpending;

    // initial read size: basic packet.
    int baseLength = getPktBaseSize();

    int maxin = inAllowedBytes();

start_packet_read:
    {
        // read the basic block (minimum packet size)
        int amountRead;
        // reset the block, to avoid uninitialized memory reads.
        memset(block,0,baseLength);

        if (baseLength != (amountRead = bio->readdata(block, baseLength))) {
            pqioutput(PQL_DEBUG_BASIC, PQISTREAMERZONE, "pqistreamer::handleincoming() Didn't read BasePkt!");

            inReadBytes(readbytes);

            if (amountRead == 0) {
                pqioutput(PQL_DEBUG_BASIC, PQISTREAMERZONE, "pqistreamer::handleincoming() read blocked");
                return 0;
            } else if (amountRead < 0) {
                // TMost likely it is either nothing to read or a packet is pending but could not be read by pqissl because of stream flow.
                // So we return without an error, and leave the machine state in 'start_read'.
                pqioutput(PQL_DEBUG_BASIC, PQISTREAMERZONE, "pqistreamer::handleincoming() Error in bio read");
                return 0;
            } else {
                // This should never happen as partial reads are handled at a lower layer in the bio.
                std::ostringstream out;
                out << "pqistreamer::handleincoming() Incomplete ";
                out << "(Strange) read of " << amountRead << " bytes";
                pqioutput(PQL_ALERT, PQISTREAMERZONE, out.str().c_str());
                return -1;
            }
        }

        readbytes += baseLength;
        reading_state = reading_state_packet_started;
        failed_read_attempts = 0; // base packet totally read, reset failed read count
    }

    {
        // How much more to read.
        int extraLength = getNetItemSize(block) - baseLength;

        if (extraLength > maxlen - baseLength) {
            pqioutput(PQL_ALERT, PQISTREAMERZONE, "Received a packet larger than the maximum limit allowed!");
            bio->close();
            reading_state = reading_state_initial;
            failed_read_attempts = 0;
            return -1;
        }

        if (extraLength > 0) {
            void *extradata = (void *) (((char *) block) + baseLength);
            int amountRead;
            // reset the block, to avoid uninitialized memory reads.
            memset((void *)( &(((unsigned char *)block)[baseLength])), 0 ,extraLength);
            // for checking later
            memset(extradata, 0, extraLength);

            // we assume readdata() returned either -1 or the complete read size.
            if (extraLength != (amountRead = bio->readdata(extradata, extraLength))) {
                if (++failed_read_attempts > max_failed_read_attempts) {
                    pqioutput(PQL_ALERT, PQISTREAMERZONE, "Unexpected failure to read packet");
                    bio->close();
                    reading_state = reading_state_initial;
                    failed_read_attempts = 0;
                    return -1;
                } else {
                    // this is just a SSL_WANT_READ error. Don't panic, we'll re-try the read soon.
                    return 0;
                }
            }
            failed_read_attempts = 0;
            readbytes += extraLength;
        }

        // create packet, based on header.
        {
            std::ostringstream out;
            out << "Read Data Block->Incoming Pkt(";
            out << baseLength + extraLength << ")" << std::endl;
            pqioutput(PQL_DEBUG_BASIC, PQISTREAMERZONE, out.str().c_str());
        }

        uint32_t pktlen = baseLength+extraLength;

        NetItem *pkt = serialiser->deserialise(block, &pktlen);

        if (pkt != NULL){
            // Use overloaded Contact function
            pkt->LibraryMixerId(LibraryMixerId());

            incoming.push_back(pkt);

            friendsConnectivityManager->heardFrom(LibraryMixerId());

            pqioutput(PQL_DEBUG_BASIC, PQISTREAMERZONE, "Successfully read a packet");
        }
        else pqioutput(PQL_ALERT, PQISTREAMERZONE, "Failed to deserialize a packet!");

        reading_state = reading_state_initial;
        failed_read_attempts = 0;
    }

    if (maxin < readbytes){
        pqioutput(PQL_DEBUG_ALERT, PQISTREAMERZONE, "pqistreamer::handleincoming() Max bytes read");
    } else {
        if (bio->moretoread()) goto start_packet_read;
    }

    inReadBytes(readbytes);
    return 0;
}


/* BandWidth Management Assistance */

int pqistreamer::outAllowedBytes() {
    int currentTime = time(NULL);

    int maxout = (int) (getMaxRate(false) * 1000.0);

    /* allow a lot if not bandwidthLimited */
    if (!bio->bandwidthLimited() || maxout == 0) {
        currSent = 0;
        currSentTS = currentTime;
        return PQISTREAM_ABS_MAX;
    }

    int timeElapsed = currentTime - currSentTS;
    // limiter->for when currSentTs->0.
    if (timeElapsed > 5) timeElapsed = 5;

    currSent -= timeElapsed * maxout;
    if (currSent < 0) {
        currSent = 0;
    }

    currSentTS = currentTime;

    {
        std::ostringstream out;
        out << "pqistreamer::outAllowedBytes() is ";
        out << maxout - currSent << "/";
        out << maxout;
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    return maxout - currSent;
}

int     pqistreamer::inAllowedBytes() {
    int currentTime = time(NULL); // get current timestep.

    int maxin = (int) (getMaxRate(true) * 1000.0);

    /* allow a lot if not bandwidthLimited */
    if (!bio->bandwidthLimited() || maxin == 0) {
        currReadTS = currentTime;
        currRead = 0;
        return PQISTREAM_ABS_MAX;
    }

    int timeElapsed = currentTime - currReadTS;
    // limiter->for when currReadTs->0.
    if (timeElapsed > 5) timeElapsed = 5;

    currRead -= timeElapsed * maxin;
    if (currRead < 0) {
        currRead = 0;
    }

    currReadTS = currentTime;

    {
        std::ostringstream out;
        out << "pqistreamer::inAllowedBytes() is ";
        out << maxin - currRead << "/";
        out << maxin;
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    return maxin - currRead;
}


static const float AVG_PERIOD = 5; // sec
static const float AVG_PAST_WEIGHT = 0.8; // Percentage amount to weight speed by past versus current rate

void    pqistreamer::outSentBytes(int outb) {
    {
        std::ostringstream out;
        out << "pqistreamer::outSentBytes(): ";
        out << outb << "@" << getRate(false) << "kB/s" << std::endl;
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }


    totalSent += outb;
    currSent += outb;
    avgSentCount += outb;

    int currentTime = time(NULL); // get current timestep.
    if (currentTime - avgLastUpdate > AVG_PERIOD) {
        float avgReadpSec = getRate(true);
        float avgSentpSec = getRate(false);

        avgReadpSec *= AVG_PAST_WEIGHT;
        avgReadpSec += (1.0 - AVG_PAST_WEIGHT) * avgReadCount /
                       (1000.0 * (currentTime - avgLastUpdate));

        avgSentpSec *= AVG_PAST_WEIGHT;
        avgSentpSec += (1.0 - AVG_PAST_WEIGHT) * avgSentCount /
                       (1000.0 * (currentTime - avgLastUpdate));


        /* pretend our rate is zero if we are
         * not bandwidthLimited().
         */
        if (bio->bandwidthLimited()) {
            setRate(true, avgReadpSec);
            setRate(false, avgSentpSec);
        } else {
            setRate(true, 0);
            setRate(false, 0);
        }

        avgLastUpdate = currentTime;
        avgReadCount = 0;
        avgSentCount = 0;
    }
    return;
}

void    pqistreamer::inReadBytes(int inb) {
    {
        std::ostringstream out;
        out << "pqistreamer::inReadBytes(): ";
        out << inb << "@" << getRate(true) << "kB/s" << std::endl;
        pqioutput(PQL_DEBUG_ALL, PQISTREAMERZONE, out.str().c_str());
    }

    totalRead += inb;
    currRead += inb;
    avgReadCount += inb;

    return;
}

