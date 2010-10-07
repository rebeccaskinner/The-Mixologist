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

#include <map>
#include <list>
#include <string>

#include "ft/ftfilecreator.h"
#include "ft/ftdatamultiplex.h"
#include "ft/ftcontroller.h"

#include "util/threads.h"

const uint32_t  PQIPEER_INIT                 = 0x0000;
const uint32_t  PQIPEER_NOT_ONLINE           = 0x0001;
const uint32_t  PQIPEER_DOWNLOADING          = 0x0002;
const uint32_t  PQIPEER_IDLE                 = 0x0004;
const uint32_t  PQIPEER_SUSPEND              = 0x0010;

class peerInfo {
public:
    peerInfo(int _librarymixer_id);

    int librarymixer_id;
    std::string cert_id;
    uint32_t state;
    double actualRate;

    //current file data request
    uint64_t offset;
    uint32_t chunkSize;

    //already received data size for current request
    uint32_t receivedSize;

    time_t lastTS; /* last Request */
    time_t recvTS; /* last Recv */
    uint32_t lastTransfers; /* data recvd in last second */
    uint32_t nResets; /* count to disable non-existant files */

    /* rrt rate control */
    uint32_t rtt;       /* last rtt */
    bool     rttActive; /* have we initialised an rtt measurement */
    time_t   rttStart;  /* ts of request */
    uint64_t rttOffset; /* end of request */
    float    mRateIncrease; /* percentage to change current rate. 0 = steady, .5 = 50% more, 1 = double, -1 = stop */
    /* When true, the next request will be for FT_TM_FAST_START_RATE instead of what would have been requested.
       Used for starting out transfers as a minimum to start from, so it's true whenever we're starting out. */
    bool    fastStart;
};

class ftFileStatus {
public:
    enum Status {
        PQIFILE_INIT,
        PQIFILE_NOT_ONLINE,
        PQIFILE_DOWNLOADING,
        PQIFILE_PAUSE,
        PQIFILE_COMPLETE,
        PQIFILE_FAIL,
        PQIFILE_FAIL_CANCEL,
        PQIFILE_FAIL_NOT_AVAIL,
        PQIFILE_FAIL_NOT_OPEN,
        PQIFILE_FAIL_NOT_SEEK,
        PQIFILE_FAIL_NOT_WRITE,
        PQIFILE_FAIL_NOT_READ,
        PQIFILE_FAIL_BAD_PATH
    };

    ftFileStatus():hash(""),stat(PQIFILE_INIT) {}
    ftFileStatus(std::string hash_in):hash(hash_in),stat(PQIFILE_INIT) {}

    std::string hash;
    Status stat;
};

class ftTransferModule {
public:
    ftTransferModule(ftFileCreator *fc, ftDataMultiplex *dm, ftController *c);
    ~ftTransferModule();

    //interface to download controller
    //Batch add a list of friends by librarymixer_ids as file sources
    bool setFileSources(QList<int> sourceIds);
    //Adds the friend as a file source
    bool addFileSource(int librarymixer_id);
    //Sets the online state and target transfer rate for a friend
    bool setPeerState(int librarymixer_id,uint32_t state);  //state = ONLINE/OFFLINE
    //Returns a list of librarymixer ids of file sources that are known about
    bool getFileSources(QList<int> &sourceIds);
    //Gets the online state and target transfer rate for a friend
    bool getPeerState(int librarmixer_id,uint32_t &state,uint32_t &tfRate);
    bool pauseTransfer();
    bool resumeTransfer();
    bool cancelTransfer();

    //interface to multiplex module
    bool recvFileData(std::string cert_id, uint64_t offset,
                      uint32_t chunk_size, void *data);
    void requestData(std::string cert_id, uint64_t offset, uint32_t chunk_size);

    //interface to file creator
    bool getChunk(uint64_t &offset, uint32_t &chunk_size);
    bool storeData(uint64_t offset, uint32_t chunk_size, void *data);

    int tick();

    std::string hash() {
        return mFileCreator->getHash();
    }
    uint64_t    size() {
        return mSize;
    }


private:
    bool queryInactive();
    //Updates actualRate with the total from all sources
    void updateActualRate();
    //Called when a file transfer is complete
    bool completeFileTransfer();
    bool locked_tickPeerTransfer(peerInfo &info);
    bool locked_recvPeerData(peerInfo &info, uint64_t offset,
                             uint32_t chunk_size, void *data);


    /* These have independent Mutexes / are const locally (no Mutex protection)*/
    ftFileCreator *mFileCreator;
    ftDataMultiplex *mMultiplexor;
    ftController *mFtController;

    uint64_t    mSize;

    MixMutex tfMtx; /* below is mutex protected */

    //List of all sources, with first element the cert_id of the source
    std::map<std::string,peerInfo> mFileSources;

    uint16_t     mFlag;  //2:file canceled, 1:transfer complete, 0: not complete
    double actualRate;

    ftFileStatus mFileStatus; //used for pause/resume file transfer
};

#endif  //FT_TRANSFER_MODULE_HEADER
