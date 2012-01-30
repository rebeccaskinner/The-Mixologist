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

#include <cstdlib>
#include <time.h>
#include "ft/ftdatademultiplex.h"
#include "ft/ftcontroller.h"
#include "ft/ftfilecreator.h"
#include "ft/ftfileprovider.h"
#include "ft/ftfilemethod.h"
#include "ft/ftserver.h"
#include "util/debug.h"

/* For Thread Behaviour */
const uint32_t DMULTIPLEX_MIN   = 10; /* 1ms sleep */
const uint32_t DMULTIPLEX_MAX   = 1000; /* 1 sec sleep */
const double   DMULTIPLEX_RELAX = 0.5; /* ??? */

const uint32_t FT_DATA      = 0x0001;
const uint32_t FT_DATA_REQ  = 0x0002;

ftRequest::ftRequest(uint32_t type, std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunk, void *data)
    :mType(type), mPeerId(peerId), mHash(hash), mSize(size),
     mOffset(offset), mChunk(chunk), mData(data) {
    return;
}

ftDataDemultiplex::ftDataDemultiplex(ftController *controller) :mController(controller) {}

void ftDataDemultiplex::addFileMethod(ftFileMethod *fileMethod) {
    QMutexLocker stack(&dataMtx);
    mFileMethods.append(fileMethod);
}

void ftDataDemultiplex::run() {
    while (1) {
        bool doneWork = false;
        while (workQueued() && doWork()) {
            doneWork = true;
        }
        time_t now = time(NULL);
        if (doneWork) {
            mLastWork = now;
            mLastSleep = (uint32_t) (DMULTIPLEX_MIN + (mLastSleep - DMULTIPLEX_MIN) / 2.0);
        } else {
            uint32_t deltaT = now - mLastWork;
            double frac = deltaT / DMULTIPLEX_RELAX;

            mLastSleep += (uint32_t)((DMULTIPLEX_MAX - DMULTIPLEX_MIN) * (frac + 0.05));
            if (mLastSleep > DMULTIPLEX_MAX) {
                mLastSleep = DMULTIPLEX_MAX;
            }
        }
        msleep(mLastSleep);
    }
}

void ftDataDemultiplex::FileUploads(QList<uploadFileInfo> &uploads) {
    QMutexLocker stack(&dataMtx);
    QMap<QString, ftFileProvider *>::const_iterator it;
    foreach (ftFileProvider* fileServe, activeFileServes.values()) {
        if (!fileServe->isInternalMixologistFile()) {
            uploadFileInfo info;
            fileServe->FileDetails(info);
            uploads.push_back(info);
        }
    }
    foreach (ftFileProvider* fileServe, deadFileServes) {
        /* No need to worry about internalMixologistFiles because they're deleted rather than added to dead list. */
        uploadFileInfo info;
        fileServe->FileDetails(info);
        uploads.push_back(info);
    }
}

/*************** RECV INTERFACE (provides ftDataRecv) ****************/

/* Client Recv */
bool ftDataDemultiplex::recvData(std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize, void *data) {
    /* Store in Queue */
    QMutexLocker stack(&dataMtx);
    mRequestQueue.push_back(ftRequest(FT_DATA,peerId,hash,size,offset,chunksize,data));

    return true;
}

/* Server Recv */
bool ftDataDemultiplex::recvDataRequest(std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
    /* Store in Queue */
    QMutexLocker stack(&dataMtx);
    mRequestQueue.push_back(ftRequest(FT_DATA_REQ,peerId,hash,size,offset,chunksize,NULL));

    return true;
}

void ftDataDemultiplex::fileNoLongerAvailable(QString hash, qulonglong size) {
    QMutexLocker stack(&dataMtx);
    deactivateFileServe(hash, size);
}

/*********** BACKGROUND THREAD OPERATIONS ***********/
bool ftDataDemultiplex::workQueued() {
    QMutexLocker stack(&dataMtx);
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
            QMutexLocker stack(&dataMtx);
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
                handleIncomingData(req.mPeerId, req.mHash, req.mOffset, req.mChunk, req.mData);
                break;

            case FT_DATA_REQ:
                handleOutgoingDataRequest(req.mPeerId, req.mHash, req.mSize,  req.mOffset, req.mChunk);
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
        QMutexLocker stack(&dataMtx);
        if (mSearchQueue.size() == 0) {
            return true;
        }

        req = mSearchQueue.front();
        mSearchQueue.pop_front();
    }

    handleSearchRequest(req.mPeerId, req.mHash, req.mSize, req.mOffset, req.mChunk);

    return true;
}


bool ftDataDemultiplex::handleIncomingData(std::string peerId, QString hash, uint64_t offset, uint32_t chunksize, void *data) {
    return mController->handleReceiveData(peerId, hash, offset, chunksize, data);
}

void ftDataDemultiplex::handleOutgoingDataRequest(std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
    /**** Find Files *****/

    QMutexLocker stack(&dataMtx);

    /* Once multi-source is implemented, we should scan the files we're downloading here to see if
       we're downloading the thing that is being requested and can respond with what we have. */

    //If the data being requested is something we're currently uploading
    if (activeFileServes.contains(hash) && activeFileServes[hash]->getFileSize() == size) {
        sendRequestedData(activeFileServes[hash], peerId, hash, size, offset, chunksize);
        return;
    }

    /* Not present in our available files. We'll do a search for it in our file list when we have a chance.
       The results won't be available immediately, at best it will be added to activeFileServes, and become
       available for the next time the requestor sends a request for this. */
    mSearchQueue.push_back(ftRequest(FT_DATA_REQ, peerId, hash, size, offset, chunksize, NULL));

    return;
}

bool ftDataDemultiplex::sendRequestedData(ftFileProvider *provider, std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
    void *data = malloc(chunksize);

    if (data == NULL) {
        log(LOG_DEBUG_ALERT, FTDATADEMULTIPLEXZONE,
            "ftDataDemultiplex::sendRequestedData malloc failed for a chunksize of " + QString::number(chunksize));
        return false;
    }

    if (provider->getFileData(offset, chunksize, data)) {
        log(LOG_DEBUG_ALL, FTDATADEMULTIPLEXZONE,
            QString("ftDataDemultiplex::sendRequestedData") +
            " hash: " + hash +
            " offset: " + QString::number(offset) +
            " chunksize: " + QString::number(chunksize));
        provider->setLastRequestor(peerId);
        ftserver->sendData(peerId, hash, size, offset, chunksize, data);
        return true;
    } else {
        log(LOG_WARNING, FTDATADEMULTIPLEXZONE, "Unable to read file " + provider->getPath());
        deactivateFileServe(hash, size);
        return false;
    }
}

void ftDataDemultiplex::clearUploads() {
    QMutexLocker stack(&dataMtx);
    foreach (ftFileProvider* provider, activeFileServes.values()) {
        delete provider;
    }
    foreach (ftFileProvider* provider, deadFileServes) {
        delete provider;
    }
    activeFileServes.clear();
    deadFileServes.clear();
}

bool ftDataDemultiplex::handleSearchRequest(std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
    QString path;
    uint32_t hintflags = (FILE_HINTS_TEMP |
                          FILE_HINTS_ITEM |
                          FILE_HINTS_OFF_LM);

    ftFileMethod::searchResult result = ftFileMethod::SEARCH_RESULT_NOT_FOUND;
    {
        QMutexLocker stack(&dataMtx);
        foreach (ftFileMethod* fileMethod, mFileMethods) {
            result = fileMethod->search(hash, size, hintflags, path);
            if (result != ftFileMethod::SEARCH_RESULT_NOT_FOUND) break;
        }
    }

    if (result != ftFileMethod::SEARCH_RESULT_NOT_FOUND) {

        /* setup a new provider */
        QMutexLocker stack(&dataMtx);

        ftFileProvider *provider = new ftFileProvider(path, size, hash);
        if (provider->checkFileValid()) {
            if (result == ftFileMethod::SEARCH_RESULT_FOUND_INTERNAL_FILE) provider->setInternalMixologistFile(true);
            activeFileServes[hash] = provider;

            /* handle request finally */
            sendRequestedData(provider, peerId, hash, size, offset, chunksize);
            return true;
        } else delete provider;
    }
    return false;
}

void ftDataDemultiplex::deactivateFileServe(QString hash, uint64_t size) {
    if (activeFileServes.contains(hash) &&
        activeFileServes[hash]->getFileSize() == size &&
        !activeFileServes[hash]->isInternalMixologistFile()) {
        activeFileServes[hash]->closeFile();
        deadFileServes.append(activeFileServes[hash]);
        activeFileServes.remove(hash);
    }
}
