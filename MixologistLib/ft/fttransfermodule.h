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

#include <map>
#include <list>
#include <string>

#include "ft/ftfilecreator.h"
#include "ft/ftdatademultiplex.h"
#include "ft/ftcontroller.h"

#include "util/threads.h"

/*
 * FUNCTION DESCRIPTION
 *
 * Each Transfer Module is paired up with a single File Creator, and responsible for the transfer of one file.
 * The Transfer Module is responsible for sending requests to peers at the correct data rates, and storing the returned data
 * in a FileCreator.
 * There are multiple Transfer Modules in the File Transfer system. Their requests are multiplexed through the Client Module.
 * The Transfer Module contains all the algorithms for sensible Data Requests.
 * It must be able to cope with varying data rates and dropped peers without flooding the system with too many requests.
 *
 */

const uint32_t  PQIPEER_INIT                 = 0x0000;
const uint32_t  PQIPEER_NOT_ONLINE           = 0x0001;
const uint32_t  PQIPEER_DOWNLOADING          = 0x0002;
const uint32_t  PQIPEER_IDLE                 = 0x0004;
const uint32_t  PQIPEER_SUSPEND              = 0x0010;

class peerInfo;

class ftTransferModule {
public:
    enum fileTransferStatus {
        FILE_DOWNLOADING,
        FILE_COMPLETING,
        FILE_COMPLETE,
        FILE_FAIL_CANCEL
    };

    ftTransferModule(ftFileCreator *fc, ftDataDemultiplex *dm, ftController *c);
    ~ftTransferModule(){};

    //Called from ftController
    int tick();
    //Called from ftController, batch sets a list of friends by librarymixer_ids as file sources
    bool setFileSources(QList<int> sourceIds);
    //Called from ftController, adds the friend as a file source
    bool addFileSource(int librarymixer_id);
    //Called from ftController, sets the online state of a friend
    bool setPeerState(int librarymixer_id,uint32_t state);  //state = ONLINE/OFFLINE
    //Called from ftController, returns a list of librarymixer ids of file sources that are known
    bool getFileSources(QList<int> &sourceIds);
    //Called from ftController, gets the online state and target transfer rate for a friend
    bool getPeerState(int librarmixer_id,uint32_t &state,uint32_t &tfRate);

    /*
    TEMPORARILY REMOVED
    bool pauseTransfer();
    bool resumeTransfer();
    */
    //Called from ftController, instructs the transfer module to stop doing anything in preparation for its imminent deletion
    void cancelTransfer();

    //Called from ftDataDemultiplex when data is received, frees the data whether successful or not
    bool recvFileData(std::string cert_id, uint64_t offset, uint32_t chunk_size, void *data);

private:
    //Updates actualRate with the total from all sources
    void updateActualRate();
    //Called to signal when a file transfer is complete
    void completeFileTransfer();
    //Called by tick, handles calculating target rates and then requesting an appropriate amount of data
    bool locked_tickPeerTransfer(peerInfo &info);
    //Called by recvFileData, updates info about our transfer
    //If we are rttActive and calculating a new rate, and we have finished receiving the full chunk, calculates the new rate change
    void locked_recvDataUpdateStats(peerInfo &info, uint64_t offset, uint32_t chunk_size);

    /* These have independent Mutexes / are const locally (no Mutex protection)*/
    ftFileCreator *mFileCreator;
    ftController *mFtController; //So we can let it know when a file completes

    MixMutex tfMtx;

    //List of all sources, with first element the cert_id of the source
    QMap<std::string,peerInfo> mFileSources;

    //Controls the current status of the download
    fileTransferStatus mTransferStatus;

    //Total transfer speed on this file
    double actualRate;
};

class peerInfo {
public:
    peerInfo(int _librarymixer_id, std::string cert_id)
        :librarymixer_id(_librarymixer_id), cert_id(cert_id), state(PQIPEER_NOT_ONLINE), actualRate(0),
        offset(0), chunkSize(0), receivedSize(0), lastRequestTime(0), lastReceiveTime(0), pastTickTransfered(0), nResets(0),
        rtt(0), rttActive(false), rttStart(0), rttOffset(0),mRateChange(1), fastStart(true) {return;}

    int librarymixer_id;
    std::string cert_id;
    uint32_t state;
    double actualRate;

    //current file data request
    uint64_t offset;
    uint32_t chunkSize;

    //already received data size for current request
    uint32_t receivedSize;

    time_t lastRequestTime;
    time_t lastReceiveTime;
    uint32_t pastTickTransfered;
    uint32_t nResets; /* count to disable non-existant files */

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
