/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-8, Robert Fernie
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

/*
 * ftDataMultiplexModule.
 *
 * This multiplexes the data from PQInterface to the ftTransferModules.
 */

#include <cstdlib>
#include "ft/ftdatamultiplex.h"
#include "ft/fttransfermodule.h"
#include "ft/ftfilecreator.h"
#include "ft/ftfileprovider.h"
#include "ft/ftsearch.h"
#include "util/debug.h"
const int ftdatamultiplexzone = 29592;


/* For Thread Behaviour */
const uint32_t DMULTIPLEX_MIN   = 10; /* 1ms sleep */
const uint32_t DMULTIPLEX_MAX   = 1000; /* 1 sec sleep */
const double   DMULTIPLEX_RELAX = 0.5; /* ??? */

/******
 * #define MPLEX_DEBUG 1
 *****/

const uint32_t FT_DATA      = 0x0001;
const uint32_t FT_DATA_REQ  = 0x0002;

ftRequest::ftRequest(uint32_t type, std::string peerId, std::string hash, uint64_t size, uint64_t offset, uint32_t chunk, void *data)
    :mType(type), mPeerId(peerId), mHash(hash), mSize(size),
     mOffset(offset), mChunk(chunk), mData(data) {
    return;
}

ftDataMultiplex::ftDataMultiplex(std::string ownId, ftDataSend *server, ftSearch *search)
    :MixQueueThread(DMULTIPLEX_MIN, DMULTIPLEX_MAX, DMULTIPLEX_RELAX),
     mDataSend(server),  mSearch(search), mOwnId(ownId) {
    return;
}

bool    ftDataMultiplex::addTransferModule(ftTransferModule *mod, ftFileCreator *f) {
    MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
    QMap<std::string, ftClient>::iterator it;
    if (mClients.end() != (it = mClients.find(f->getHash()))) {
        log(LOG_ERROR, ftdatamultiplexzone, "Tried to add a new file transfer but a duplicate exists!");
        return false;
    }
    mClients[f->getHash()] = ftClient(mod, f);

    return true;
}

bool    ftDataMultiplex::removeTransferModule(std::string hash) {
    MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
    QMap<std::string, ftClient>::iterator it;
    if (mClients.end() == (it = mClients.find(hash))) {
        return false;
    }
    mClients.erase(it);
    return true;
}


bool    ftDataMultiplex::FileUploads(QList<FileInfo> &uploads) {
    MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
    QMap<std::string, ftFileProvider *>::const_iterator it;
    for (it = mServers.begin(); it != mServers.end(); it++) {
        FileInfo info;
        FileDetails(it.value()->getHash(), info);
        uploads.push_back(info);
    }
    return true;;
}

bool    ftDataMultiplex::FileDetails(std::string hash, FileInfo &info) {
#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::FileDetails(";
    std::cerr << hash << ", " << hintsflag << ")";
    std::cerr << std::endl;
#endif

    QMap<std::string, ftFileProvider *>::iterator sit;
    sit = mServers.find(hash);
    if (sit != mServers.end()) {

#ifdef MPLEX_DEBUG
        std::cerr << "ftDataMultiplex::FileDetails()";
        std::cerr << " Found ftFileProvider!";
        std::cerr << std::endl;
#endif

        sit.value()->FileDetails(info);
        return true;
    }


#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::FileDetails()";
    std::cerr << " Found nothing";
    std::cerr << std::endl;
#endif

    return false;
}

/* data interface */

/*************** SEND INTERFACE (calls ftDataSend) *******************/

/* Client Send */
bool    ftDataMultiplex::sendDataRequest(std::string peerId,
        std::string hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::sendDataRequest() Client Send";
    std::cerr << std::endl;
#endif
    return mDataSend->sendDataRequest(peerId,hash,size,offset,chunksize);
}

/* Server Send */
bool    ftDataMultiplex::sendData(std::string peerId,
                                  std::string hash, uint64_t size,
                                  uint64_t offset, uint32_t chunksize, void *data) {
#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::sendData() Server Send";
    std::cerr << std::endl;
#endif
    return mDataSend->sendData(peerId,hash,size,offset,chunksize,data);
}


/*************** RECV INTERFACE (provides ftDataRecv) ****************/

/* Client Recv */
bool    ftDataMultiplex::recvData(std::string peerId,
                                  std::string hash, uint64_t size,
                                  uint64_t offset, uint32_t chunksize, void *data) {
#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::recvData() Client Recv";
    std::cerr << std::endl;
#endif
    /* Store in Queue */
    MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
    mRequestQueue.push_back(
        ftRequest(FT_DATA,peerId,hash,size,offset,chunksize,data));

    return true;
}


/* Server Recv */
bool    ftDataMultiplex::recvDataRequest(std::string peerId,
        std::string hash, uint64_t size,
        uint64_t offset, uint32_t chunksize) {
#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::recvDataRequest() Server Recv";
    std::cerr << std::endl;
#endif
    /* Store in Queue */
    MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
    mRequestQueue.push_back(
        ftRequest(FT_DATA_REQ,peerId,hash,size,offset,chunksize,NULL));

    return true;
}


/*********** BACKGROUND THREAD OPERATIONS ***********/
bool    ftDataMultiplex::workQueued() {
    MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
    if (mRequestQueue.size() > 0) {
        return true;
    }

    if (mSearchQueue.size() > 0) {
        return true;
    }

    return false;
}

bool    ftDataMultiplex::doWork() {
    bool doRequests = true;

    /* Handle All the current Requests */
    while (doRequests) {
        ftRequest req;

        {
            MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
            if (mRequestQueue.size() == 0) {
                doRequests = false;
                continue;
            }

            req = mRequestQueue.front();
            mRequestQueue.pop_front();
        }

        /* MUTEX FREE */

        switch (req.mType) {
            case FT_DATA:
#ifdef MPLEX_DEBUG
                std::cerr << "ftDataMultiplex::doWork() Handling FT_DATA";
                std::cerr << std::endl;
#endif
                handleRecvData(req.mPeerId, req.mHash, req.mSize,
                               req.mOffset, req.mChunk, req.mData);
                break;

            case FT_DATA_REQ:
#ifdef MPLEX_DEBUG
                std::cerr << "ftDataMultiplex::doWork() Handling FT_DATA_REQ";
                std::cerr << std::endl;
#endif
                handleRecvDataRequest(req.mPeerId, req.mHash,
                                      req.mSize,  req.mOffset, req.mChunk);
                break;

            default:
#ifdef MPLEX_DEBUG
                std::cerr << "ftDataMultiplex::doWork() Ignoring UNKNOWN";
                std::cerr << std::endl;
#endif
                break;
        }
    }

    /* Only Handle One Search Per Period....
        * Lower Priority
     */
    ftRequest req;

    {
        MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
        if (mSearchQueue.size() == 0) {
            /* Finished */
            return true;
        }

        req = mSearchQueue.front();
        mSearchQueue.pop_front();
    }

#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::doWork() Handling Search Request";
    std::cerr << std::endl;
#endif
    handleSearchRequest(req.mPeerId, req.mHash, req.mSize,
                        req.mOffset, req.mChunk);

    return true;
}


bool    ftDataMultiplex::handleRecvData(std::string peerId,
                                        std::string hash, uint64_t,
                                        uint64_t offset, uint32_t chunksize, void *data) {
    MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
    QMap<std::string, ftClient>::iterator it;
    if (mClients.end() == (it = mClients.find(hash))) {
#ifdef MPLEX_DEBUG
        std::cerr << "ftDataMultiplex::handleRecvData() ERROR: No matching Client!";
        std::cerr << std::endl;
#endif
        /* error */
        return false;
    }

#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::handleRecvData() Passing to Module";
    std::cerr << std::endl;
#endif

    it.value().mModule->recvFileData(peerId, offset, chunksize, data);

    return true;
}


/* called by ftTransferModule */
bool    ftDataMultiplex::handleRecvDataRequest(std::string peerId,
        std::string hash, uint64_t size,
        uint64_t offset, uint32_t chunksize) {
    /**** Find Files *****/

    MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/
    QMap<std::string, ftClient>::iterator cit;
    if (mOwnId == peerId) {
        /* own requests must be passed to Servers */
#ifdef MPLEX_DEBUG
        std::cerr << "ftDataMultiplex::handleRecvData() OwnId, so skip Clients...";
        std::cerr << std::endl;
#endif
    } else if (mClients.end() != (cit = mClients.find(hash))) {
#ifdef MPLEX_DEBUG
        std::cerr << "ftDataMultiplex::handleRecvData() Matched to a Client.";
        std::cerr << std::endl;
#endif
        locked_handleServerRequest(cit.value().mCreator,
                                   peerId, hash, size, offset, chunksize);
        return true;
    }

    QMap<std::string, ftFileProvider *>::iterator sit;
    if (mServers.end() != (sit = mServers.find(hash))) {
#ifdef MPLEX_DEBUG
        std::cerr << "ftDataMultiplex::handleRecvData() Matched to a Provider.";
        std::cerr << std::endl;
#endif
        locked_handleServerRequest(sit.value(),
                                   peerId, hash, size, offset, chunksize);
        return true;
    }

#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::handleRecvData() No Match... adding to Search Queue.";
    std::cerr << std::endl;
#endif

    /* Add to Search Queue */
    mSearchQueue.push_back(
        ftRequest(FT_DATA_REQ, peerId, hash,
                  size, offset, chunksize, NULL));

    return true;
}

bool    ftDataMultiplex::locked_handleServerRequest(ftFileProvider *provider,
        std::string peerId, std::string hash, uint64_t size,
        uint64_t offset, uint32_t chunksize) {
    void *data = malloc(chunksize);

    if (data == NULL) {
        std::cerr << "WARNING: Could not allocate data for a chunksize of " << chunksize << std::endl ;
        return false ;
    }
#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::locked_handleServerRequest()";
    std::cerr << "\t peer: " << peerId << " hash: " << hash;
    std::cerr << " size: " << size;
    std::cerr << std::endl;
    std::cerr << "\t offset: " << offset;
    std::cerr << " chunksize: " << chunksize << " data: " << data;
    std::cerr << std::endl;
#endif

    if (provider->getFileData(offset, chunksize, data)) {
        // setup info
        provider->setPeerId(peerId) ;
        /* send data out */
        sendData(peerId, hash, size, offset, chunksize, data);
        return true;
    }
#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::locked_handleServerRequest()";
    std::cerr << " FAILED";
    std::cerr << std::endl;
#endif
    free(data);

    return false;
}

void ftDataMultiplex::clearUploads() {
    MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/

    mServers.clear();
}

bool    ftDataMultiplex::handleSearchRequest(std::string peerId,
        std::string hash, uint64_t size,
        uint64_t offset, uint32_t chunksize) {


#ifdef MPLEX_DEBUG
    std::cerr << "ftDataMultiplex::handleSearchRequest(";
    std::cerr << peerId << ", " << hash << ", " << size << "...)";
    std::cerr << std::endl;
#endif

    /*
     * Do Actual search
         * Could be Cache File, Local or Item
     * (anywhere but remote really)
     */

    FileInfo info;
    uint32_t hintflags = (FILE_HINTS_ITEM |
                          FILE_HINTS_SPEC_ONLY);

    if (mSearch->search(hash, size, hintflags, info)) {

#ifdef MPLEX_DEBUG
        std::cerr << "ftDataMultiplex::handleSearchRequest(";
        std::cerr << " Found Local File, sharing...";
        std::cerr << std::endl;
#endif


        /* setup a new provider */
        MixStackMutex stack(dataMtx); /******* LOCK MUTEX ******/

        ftFileProvider *provider =
            new ftFileProvider(info.paths.first(), size, hash);

        mServers[hash] = provider;

        /* handle request finally */
        locked_handleServerRequest(provider,
                                   peerId, hash, size, offset, chunksize);


        /* now we should should check if any further requests for the same
         * file exists ... (can happen with caches!)
         *
         * but easier to check pre-search....
         */

        return true;
    }
    return false;
}
