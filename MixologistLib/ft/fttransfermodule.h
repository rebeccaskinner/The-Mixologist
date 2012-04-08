/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2008, Robert Fernie
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

#ifndef FT_TRANSFER_MODULE_HEADER
#define FT_TRANSFER_MODULE_HEADER

#include "ft/ftfilecreator.h"
#include "ft/ftdatademultiplex.h"
#include "ft/ftcontroller.h"

#include <QMutex>

/*
 * FUNCTION DESCRIPTION
 *
 * Each Transfer Module is paired up with a single FileCreator it contains, and responsible for the download of one file.
 * The Transfer Module is responsible for sending requests to peers at the correct data rates, and storing the returned data
 * in a FileCreator.
 * The Transfer Module contains all the algorithms for sensible data requests.
 * It must be able to cope with varying data rates and dropped peers without flooding the system with too many requests.
 *
 * In practice, the way this works for file transfers is that the ftController thread's loop will have the transferModule send requests for more data.
 * Meanwhile, the incoming data is handled by the ftDataDemultiplex thread.
 * Stats are collected on the incoming data, to try to keep the requests in sync with the speed of the connection to that friend.
 * Reasons to avoid requesting a huge chunk all at once are (1) to avoid allocating too much of a file to one friend (for multi-source when enabled)
 * and (2) to avoid the overhead on the sender side of reading a huge chunk into memory all at once.
 * We do this by setting a target time to complete our requests of 9 seconds.
 * If we have completed our requested amount of data, and the time is less than 9 seconds, we increase the rate.
 * Conversely if we took more time to complete our request we decrease the rate.
 */

class peerInfo;

class ftTransferModule : public QObject {
    Q_OBJECT

public:
    enum fileTransferStatus {
        /* Not done, and not downloading at this moment, but could already be partially downloaded. */
        FILE_WAITING = 0,
        /* Currently downloading. */
        FILE_DOWNLOADING = 1,
        /* Successful completion. */
        FILE_COMPLETE = 2
    };

    ftTransferModule(unsigned int initial_friend_id, uint64_t size, const QString &hash);
    ~ftTransferModule();

    /* Called from ftController, returns 0 normally, returns 1 when file is complete. */
    int tick();

    /* Returns the current status. */
    ftTransferModule::fileTransferStatus transferStatus() const;

    /* Sets the current status. */
    void transferStatus(fileTransferStatus newStatus);

    /* Called from ftController, returns a list of librarymixer ids of file sources that are known. */
    bool getFileSources(QList<unsigned int> &sourceIds);

    /* Called from ftController, gets the online state and target transfer rate for a friend. */
    bool getPeerState(unsigned int librarmixer_id, uint32_t &state, uint32_t &tfRate);

    /* Called from ftDataDemultiplex when data is received, frees the data whether successful or not. */
    bool recvFileData(unsigned int librarymixer_id, uint64_t offset, uint32_t chunk_size, void *data);

    /* Has an independent Mutex, can be accessed directly */
    ftFileCreator* mFileCreator;

    /* Originally I wanted to make these slots and make them directly connect to the friendConnected signal from friendsConnectivityManager.
       However, they never received the signals even though I checked everything a thousand times.
       Therefore, I have put the actual signal reception in ftcontroller and ftofflmlist, where they signals are received fine.
       If anyone can figure out a way to make these directly receive the signals, that'd be a lot cleaner. */
    void friendConnected(unsigned int friend_id);
    void friendDisconnected(unsigned int friend_id);

private:
    /* Called by tick, this is the meat of the outbound data requests.
       Handles calculating target rates and then requesting an appropriate amount of data. */
    bool locked_tickPeerTransfer(peerInfo &currentPeer);

    /* Called by recvFileData, updates info about our transfer so that locked_tickPeerTransfer can know how much to request.
       If we are rttActive and calculating a new rate, and we have finished receiving the full chunk, calculates the new rate change. */
    void locked_recvDataUpdateStats(peerInfo &currentPeer, uint64_t startingByte, uint32_t chunk_size);

    mutable QMutex tfMtx;

    /* Controls the current status of the download. */
    fileTransferStatus mTransferStatus;

    /* List of all sources, with first element the LibraryMixer ID of the source. */
    QMap<unsigned int, peerInfo> mFileSources;

    /* Total transfer speed on this file. */
    double actualRate;
};

/* Used internally to hold information about each friend we have as a file source. */
class peerInfo {
public:
    /* Unused default constructor just so we can use QMap. */
    peerInfo(){}

    peerInfo(unsigned int _librarymixer_id)
        :librarymixer_id(_librarymixer_id), actualRate(0), state(PQIPEER_NOT_ONLINE),
        offset(0), chunkSize(0), receivedSize(0), lastRequestTime(0), lastReceiveTime(0), pastTickTransferred(0), numResets(0),
        rtt(0), rttActive(false), rttStart(0), rttOffset(0),mRateChange(1), fastStart(true) {return;}

    unsigned int librarymixer_id;
    double actualRate;

    enum PeerInfoState {
        PQIPEER_NOT_ONLINE = 1,
        PQIPEER_DOWNLOADING = 2,
        PQIPEER_ONLINE_IDLE = 3
    };
    PeerInfoState state;

    //current file data request
    uint64_t offset;
    uint32_t chunkSize;

    //already received data size for current request
    uint32_t receivedSize;

    time_t lastRequestTime;
    time_t lastReceiveTime;
    uint32_t pastTickTransferred;

    /* Number of times that we have reset trying for this file from this peer.
       Used to disable non-existant files. */
    uint32_t numResets;

    /* rtt rate control
     * Rate control is based on the amount of time it takes for a complete chunk to be received.
     * The target time is FT_TM_STD_RTT, at that speed, rates will neither increase nor decrease.
     * There will be many requests and receipts for a single rtt period.
     */
    uint32_t rtt;       /* amount of time it took for the last rtt */
    bool     rttActive; /* indicates we are currently measuring an rtt cycle */
    time_t   rttStart;  /* time we began measuring request */
    uint64_t rttOffset; /* offset in file when rtt cycle is complete */
    float    mRateChange; /* percentage to change current rate. 0 = steady, .5 = 50% more, 1 = 100% more, -1 = 100% less */
    /* When true, the next request will be for FT_TM_FAST_START_RATE instead of what would have been requested.
       Used for starting out transfers as a minimum to start from, so it's true whenever we're starting out. */
    bool    fastStart;
};

#endif  //FT_TRANSFER_MODULE_HEADER
