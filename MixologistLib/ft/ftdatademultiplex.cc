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
 * ftDataDemultiplexModule.
 *
 * This multiplexes the data from PQInterface to the ftTransferModules.
 */

#include <cstdlib>
#include "ft/ftdatademultiplex.h"
#include "ft/fttransfermodule.h"
#include "ft/ftfilecreator.h"
#include "ft/ftfileprovider.h"
#include "ft/ftsearch.h"
#include "ft/ftserver.h"
#include "util/debug.h"


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

ftDataDemultiplex::ftDataDemultiplex(std::string ownId, ftSearch *search)
    :MixQueueThread(DMULTIPLEX_MIN, DMULTIPLEX_MAX, DMULTIPLEX_RELAX),
     mSearch(search), mOwnId(ownId) {
    return;
}

bool ftDataDemultiplex::addTransferModule(ftTransferModule *mod, ftFileCreator *f) {
    MixStackMutex stack(dataMtx);
    QMap<std::string, ftClient>::iterator it;
    if (mClients.end() != (it = mClients.find(f->getHash()))) {
        log(LOG_ERROR, FTDATADEMULTIPLEXZONE, "Tried to add a new file transfer but a duplicate exists!");
        return false;
    }
    mClients[f->getHash()] = ftClient(mod, f);

    return true;
}

bool ftDataDemultiplex::removeTransferModule(std::string hash) {
    MixStackMutex stack(dataMtx);
    QMap<std::string, ftClient>::iterator it;
    if (mClients.end() == (it = mClients.find(hash))) {
        return false;
    }
    mClients.erase(it);
    return true;
}


bool ftDataDemultiplex::FileUploads(QList<FileInfo> &uploads) {
    MixStackMutex stack(dataMtx);
    QMap<std::string, ftFileProvider *>::const_iterator it;
    for (it = mServers.begin(); it != mServers.end(); it++) {
        FileInfo info;
        FileDetails(it.value()->getHash(), info);
        uploads.push_back(info);
    }
    return true;;
}

bool ftDataDemultiplex::FileDetails(std::string hash, FileInfo &info) {
    QMap<std::string, ftFileProvider *>::iterator sit;
    sit = mServers.find(hash);
    if (sit != mServers.end()) {
        sit.value()->FileDetails(info);
        return true;
    }
    return false;
}

/* data interface */

/*************** RECV INTERFACE (provides ftDataRecv) ****************/

/* Client Recv */
bool    ftDataDemultiplex::recvData(std::string peerId,
                                  std::string hash, uint64_t size,
                                  uint64_t offset, uint32_t chunksize, void *data) {
    /* Store in Queue */
    MixStackMutex stack(dataMtx);
    mRequestQueue.push_back(
        ftRequest(FT_DATA,peerId,hash,size,offset,chunksize,data));

    return true;
}


/* Server Recv */
bool    ftDataDemultiplex::recvDataRequest(std::string peerId,
        std::string hash, uint64_t size,
        uint64_t offset, uint32_t chunksize) {
    /* Store in Queue */
    MixStackMutex stack(dataMtx);
    mRequestQueue.push_back(
        ftRequest(FT_DATA_REQ,peerId,hash,size,offset,chunksize,NULL));

    return true;
}


/*********** BACKGROUND THREAD OPERATIONS ***********/
bool ftDataDemultiplex::workQueued() {
    MixStackMutex stack(dataMtx);
    if (mRequestQueue.size() > 0) {
        return true;
    }

    if (mSearchQueue.size() > 0) {
        return true;
    }

    return false;
}

bool ftDataDemultiplex::doWork() {
    bool doRequests = true;

    /* Handle All the current Requests */
    while (doRequests) {
        ftRequest req;

        {
            MixStackMutex stack(dataMtx);
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
                handleRecvData(req.mPeerId, req.mHash, req.mSize, req.mOffset, req.mChunk, req.mData);
                break;

            case FT_DATA_REQ:
                handleRecvDataRequest(req.mPeerId, req.mHash, req.mSize,  req.mOffset, req.mChunk);
                break;

            default:
                break;
        }
    }

    /* Only Handle One Search Per Period....
     * Lower Priority
     */
    ftRequest req;

    {
        MixStackMutex stack(dataMtx);
        if (mSearchQueue.size() == 0) {
            /* Finished */
            return true;
        }

        req = mSearchQueue.front();
        mSearchQueue.pop_front();
    }

    handleSearchRequest(req.mPeerId, req.mHash, req.mSize, req.mOffset, req.mChunk);

    return true;
}


bool ftDataDemultiplex::handleRecvData(std::string peerId, std::string hash, uint64_t,
                                          uint64_t offset, uint32_t chunksize, void *data) {
    MixStackMutex stack(dataMtx);
    QMap<std::string, ftClient>::iterator it;
    if (mClients.end() == (it = mClients.find(hash))) {
        return false;
    }

    it.value().mModule->recvFileData(peerId, offset, chunksize, data);

    return true;
}


/* called by ftTransferModule */
void ftDataDemultiplex::handleRecvDataRequest(std::string peerId, std::string hash, uint64_t size,
        uint64_t offset, uint32_t chunksize) {
    /**** Find Files *****/

    MixStackMutex stack(dataMtx);
    QMap<std::string, ftClient>::iterator cit;
    if (mOwnId == peerId) {
    /* own requests must be passed to Servers */
    //If the data being requested is something we're currently downloading
    } else if (mClients.end() != (cit = mClients.find(hash))) {
        locked_handleServerRequest(cit.value().mCreator, peerId, hash, size, offset, chunksize);
        return;
    }

    //If the data being requested is something we're currently uploading
    QMap<std::string, ftFileProvider *>::iterator sit;
    if (mServers.end() != (sit = mServers.find(hash))) {
        locked_handleServerRequest(sit.value(), peerId, hash, size, offset, chunksize);
        return;
    }

    //Otherwise we'll need to do a search for it in our file list
    mSearchQueue.push_back(ftRequest(FT_DATA_REQ, peerId, hash, size, offset, chunksize, NULL));

    return;
}

bool ftDataDemultiplex::locked_handleServerRequest(ftFileProvider *provider,
        std::string peerId, std::string hash, uint64_t size,
        uint64_t offset, uint32_t chunksize) {
    void *data = malloc(chunksize);

    if (data == NULL) {
        log(LOG_DEBUG_ALERT, FTDATADEMULTIPLEXZONE,
            "ftDataDemultiplex::locked_handleServerRequest malloc failed for a chunksize of " + QString::number(chunksize));
        return false;
    }

    if (provider->getFileData(offset, chunksize, data)) {
        log(LOG_DEBUG_ALL, FTDATADEMULTIPLEXZONE,
            QString("ftDataDemultiplex::locked_handleServerRequest") +
            " hash: " + hash.c_str() +
            " offset: " + QString::number(offset) +
            " chunksize: " + QString::number(chunksize));
        provider->setLastRequestor(peerId);
        ftserver->sendData(peerId, hash, size, offset, chunksize, data);
        return true;
    }
    log(LOG_DEBUG_ALERT, FTDATADEMULTIPLEXZONE,
        "ftDataDemultiplex::locked_handleServerRequest unable to read file data");
    free(data);

    return false;
}

void ftDataDemultiplex::clearUploads() {
    MixStackMutex stack(dataMtx);
    mServers.clear();
}

bool ftDataDemultiplex::handleSearchRequest(std::string peerId, std::string hash, uint64_t size,
                                               uint64_t offset, uint32_t chunksize) {

    /*
     * Do Actual search
     * Could be Cache File, Local or Item
     * (anywhere but remote really)
     */

    FileInfo info;
    uint32_t hintflags = (FILE_HINTS_ITEM |
                          FILE_HINTS_SPEC_ONLY);

    if (mSearch->search(hash, size, hintflags, info)) {

        /* setup a new provider */
        MixStackMutex stack(dataMtx);

        ftFileProvider *provider = new ftFileProvider(info.paths.first(), size, hash);

        mServers[hash] = provider;

        /* handle request finally */
        locked_handleServerRequest(provider, peerId, hash, size, offset, chunksize);


        /* now we should should check if any further requests for the same
         * file exists ... (can happen with caches!)
         *
         * but easier to check pre-search....
         */

        return true;
    }
    return false;
}
