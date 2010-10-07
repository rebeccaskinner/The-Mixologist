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
#include "util/threads.h"

#include <list>

/*
A full implementation of the PQInterface that must be supplied
a BinInterface to communicate via.

Implementations include pqistreamer, pqiperson (which presents it as an interface
to underlying pqistreamers), pqiloopback.
*/

// The interface does not handle connection, just communication.
// possible bioflags: BIN_FLAGS_NO_CLOSE | BIN_FLAGS_NO_DELETE

class pqistreamer: public PQInterface {
public:
    pqistreamer(Serialiser *rss, std::string peerid, int librarymixer_id, BinInterface *bio_in, int bio_flagsin);
    virtual ~pqistreamer();

    // PQInterface
    virtual int     SendItem(NetItem *);
    virtual NetItem *GetItem();

    virtual int     tick();
    virtual int     status();

private:
    /* Implementation */

    // to filter functions - detect filecancel/data and act!
    int queue_outpqi(      NetItem *i);
    int     handleincomingitem(NetItem *i);

    // ticked regularly (manages out queues and sending via above interfaces.
    int handleoutgoing();
    int handleincoming();

    // Bandwidth/Streaming Management.
    float   outTimeSlice();

    int outAllowedBytes();
    void    outSentBytes(int );

    int inAllowedBytes();
    void    inReadBytes(int );

    // Serialiser - determines which packets can be serialised.
    Serialiser *serialiser;
    // Binary Interface for IO, initialized at startup.
    BinInterface *bio;
    unsigned int  bio_flags; // BIN_FLAGS_NO_CLOSE | BIN_FLAGS_NO_DELETE

    void *pkt_wpending; // storage for pending packet to write.
    int   pkt_rpend_size; // size of pkt_rpending.
    void *pkt_rpending; // storage for read in pending packets.

    enum {reading_state_packet_started=1,
          reading_state_initial=0
         } ;

    int   reading_state ;
    int   failed_read_attempts ;

    // Temp Storage for transient data.....
    std::list<void *> out_pkt; // Cntrl / Search / Results queue
    std::list<void *> out_data; // FileData - secondary queue.
    //A queue of incoming items of all types, waiting for GetItem to be called to take them off
    std::list<NetItem *> incoming;

    // data for network stats.
    int totalRead;
    int totalSent;

    // these are representative (but not exact)
    int currRead;
    int currSent;
    int currReadTS; // TS from which these are measured.
    int currSentTS;

    int avgLastUpdate; // TS from which these are measured.
    float avgReadCount;
    float avgSentCount;

    MixMutex streamerMtx ;
    //  pthread_t thread_id;
};


#endif //MRK_PQI_STREAMER_HEADER
