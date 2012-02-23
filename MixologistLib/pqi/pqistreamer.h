/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-8, Robert Fernie
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

#ifndef MRK_PQI_STREAMER_HEADER
#define MRK_PQI_STREAMER_HEADER

// Only dependent on the base stuff.
#include "pqi/pqi_base.h"
#include <QMutex>

#include <list>

/*
Each connection method a pqiperson has will have a pqistramer.
A pqistreamer is a PQInterface, and it is the final PQInterface that takes structured data
and converts it into binary data for the BinInterface.
While doing so, it also manages the bandwidth based on limits passed down to it from above.
*/

class pqistreamer: public PQInterface {
public:
    pqistreamer(Serialiser *rss, std::string peerid, unsigned int librarymixer_id, BinInterface *bio_in, int bio_flagsin);
    virtual ~pqistreamer();

    // PQInterface
    //Takes a NetItem, and adds it to the appropriate output queue
    virtual int SendItem(NetItem *);
    //Returns the first NetItem off the incoming queue
    virtual NetItem *GetItem();

    virtual int tick();

private:
    /* Implementation */
    //Called by tick to handle the outbound and inbound queues. Heavyweight functions that do almost all of the work.
    int handleoutgoing();
    int handleincoming();

    // Bandwidth/Streaming Management.
    // On each call, resets the timer on currRead/currSent and returns the rate at which read/send is allowed.
    // Takes into account any overage in the last period.
    int outAllowedBytes();
    int inAllowedBytes();

    // Updates totalRead, currRead, and avgReadCount based on amount sent
    // Also periodically updates the rate at which both read and send have been occurring on this PQInterface
    void outSentBytes(int outb);
    // Updates totalRead, currRead, and avgReadCount based on amount read
    void inReadBytes(int inb);

    // Serialiser - determines which packets can be serialised.
    Serialiser *serialiser;
    // Binary Interface for IO, initialized at startup.
    BinInterface *bio;
    unsigned int bio_flags; // possible are BIN_FLAGS_NO_CLOSE | BIN_FLAGS_NO_DELETE

    void *pkt_wpending; // storage for pending packet to write.
    int pkt_rpend_size; // size of pkt_rpending.
    void *pkt_rpending; // storage for read in pending packets.

    enum {reading_state_packet_started=1,
          reading_state_initial=0
         };

    int reading_state;
    int failed_read_attempts;

    // Temp Storage for transient data.....
    std::list<void *> out_pkt; // Control / Search / Results queue
    std::list<void *> out_data; // FileData - secondary queue.
    //A queue of incoming items of all types, waiting for GetItem to be called to take them off
    std::list<NetItem *> incoming;

    // data for network stats.
    int totalRead;
    int totalSent;

    // these are representative (but not exact)
    int currRead; // Amount read/sent since TS
    int currSent;
    int currReadTS; // TS from which these are measured.
    int currSentTS;

    int avgLastUpdate; // TS from which these are measured.
    float avgReadCount;
    float avgSentCount;

    mutable QMutex streamerMtx;
};


#endif //MRK_PQI_STREAMER_HEADER
