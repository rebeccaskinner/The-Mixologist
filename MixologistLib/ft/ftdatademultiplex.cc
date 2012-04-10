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

#include <QTimer>

/* For Thread Behaviour */
const uint32_t DMULTIPLEX_MIN   = 10; /* 1ms sleep */
const uint32_t DMULTIPLEX_MAX   = 1000; /* 1 sec sleep */
const double   DMULTIPLEX_RELAX = 0.5; /* ??? */

const uint32_t FT_DATA      = 0x0001;
const uint32_t FT_DATA_REQ  = 0x0002;

ftRequest::ftRequest(uint32_t type, unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t offset, uint32_t chunk, void *data)
    :mType(type), mLibraryMixerId(librarymixer_id), mHash(hash), mSize(size),
     mOffset(offset), mChunk(chunk), mData(data) {
    return;
}

ftDataDemultiplex::ftDataDemultiplex(ftController *controller) :mController(controller) {}

void ftDataDemultiplex::addFileMethod(ftFileMethod *fileMethod) {
    QMutexLocker stack(&dataMtx);
    mFileMethods.append(fileMethod);
}

void ftDataDemultiplex::run() {
    QTimer::singleShot(1000, this, SLOT(runThread()));
    exec();
}

void ftDataDemultiplex::runThread() {
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
    QTimer::singleShot(mLastSleep, this, SLOT(runThread()));
}

void ftDataDemultiplex::FileUploads(QList<uploadFileInfo> &uploads) {
    QMutexLocker stack(&dataMtx);

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
bool ftDataDemultiplex::recvData(unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize, void *data) {
    /* Store in Queue */
    QMutexLocker stack(&dataMtx);
    mRequestQueue.push_back(ftRequest(FT_DATA, librarymixer_id, hash, size, offset, chunksize, data));

    return true;
}

/* Server Recv */
bool ftDataDemultiplex::recvDataRequest(unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
    /* Store in Queue */
    QMutexLocker stack(&dataMtx);
    mRequestQueue.push_back(ftRequest(FT_DATA_REQ, librarymixer_id, hash, size, offset, chunksize, NULL));

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

    /* Handle all the current requests. */
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

        switch (req.mType) {
            case FT_DATA:
                handleIncomingData(req.mLibraryMixerId, req.mHash, req.mOffset, req.mChunk, req.mData);
                break;

            case FT_DATA_REQ:
                handleOutgoingDataRequest(req.mLibraryMixerId, req.mHash, req.mSize,  req.mOffset, req.mChunk);
                break;

            default:
                break;
        }
    }

    /* Only handle one search per period, we treat these as lower priority. */
    ftRequest req;
    {
        QMutexLocker stack(&dataMtx);

        if (mSearchQueue.size() == 0) {
            return true;
        }

        req = mSearchQueue.front();
        mSearchQueue.pop_front();
    }

    handleSearchRequest(req.mLibraryMixerId, req.mHash, req.mSize, req.mOffset, req.mChunk);

    return true;
}


bool ftDataDemultiplex::handleIncomingData(unsigned int librarymixer_id, QString hash, uint64_t offset, uint32_t chunksize, void *data) {
    return mController->handleReceiveData(librarymixer_id, hash, offset, chunksize, data);
}

void ftDataDemultiplex::handleOutgoingDataRequest(unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
    QMutexLocker stack(&dataMtx);

    /* Once multi-source is implemented, we should scan the files we're downloading here to see if
       we're downloading the thing that is being requested and can respond with what we have. */

    /* If the data being requested is something we've already got a file provider for, and this friend is allowed to request it. */
    if (activeFileServes.contains(hash) &&
        activeFileServes[hash]->getFileSize() == size &&
        activeFileServes[hash]->isPermittedRequestor(librarymixer_id)) {
        sendRequestedData(activeFileServes[hash], librarymixer_id, hash, size, offset, chunksize);
        return;
    }

    /* Not present in our available files. We'll do a search for it in our file list when we have a chance. */
    mSearchQueue.push_back(ftRequest(FT_DATA_REQ, librarymixer_id, hash, size, offset, chunksize, NULL));

    return;
}

bool ftDataDemultiplex::sendRequestedData(ftFileProvider *provider, unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
    void *data = malloc(chunksize);

    if (data == NULL) {
        log(LOG_DEBUG_ALERT, FTDATADEMULTIPLEXZONE, "ftDataDemultiplex::sendRequestedData malloc failed for a chunksize of " + QString::number(chunksize));
        return false;
    }

    if (provider->getFileData(offset, chunksize, data, librarymixer_id)) {
        log(LOG_DEBUG_ALL, FTDATADEMULTIPLEXZONE,
            QString("ftDataDemultiplex::sendRequestedData") +
            " hash: " + hash +
            " offset: " + QString::number(offset) +
            " chunksize: " + QString::number(chunksize));
        ftserver->sendData(librarymixer_id, hash, size, offset, chunksize, data);
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

bool ftDataDemultiplex::handleSearchRequest(unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
    QString path;
    uint32_t hintflags = (FILE_HINTS_TEMP |
                          FILE_HINTS_ITEM |
                          FILE_HINTS_OFF_LM);

    ftFileMethod::searchResult result = ftFileMethod::SEARCH_RESULT_NOT_FOUND;
    {
        QMutexLocker stack(&dataMtx);
        foreach (ftFileMethod* fileMethod, mFileMethods) {
            result = fileMethod->search(hash, size, hintflags, librarymixer_id, path);
            if (result != ftFileMethod::SEARCH_RESULT_NOT_FOUND) break;
        }
    }

    if (result != ftFileMethod::SEARCH_RESULT_NOT_FOUND) {
        QMutexLocker stack(&dataMtx);

        /* If we already have a file serve that previously rejected this friend for security failure.
           We now know that this friend is authorized by a search provider to access this file. */
        if (activeFileServes.contains(hash)) {
            if (ftFileMethod::SEARCH_RESULT_FOUND_SHARED_FILE_LIMITED == result) {
                activeFileServes[hash]->addPermittedRequestor(librarymixer_id);
            } else {
                activeFileServes[hash]->allowAllRequestors();
            }
            return true;
        }

        /* Otherwise, we are creating a new file serve. */
        else {
            ftFileProvider *provider = new ftFileProvider(path, size, hash);
            if (provider->checkFileValid()) {
                if (ftFileMethod::SEARCH_RESULT_FOUND_INTERNAL_FILE == result)
                    provider->setInternalMixologistFile(true);
                else if (ftFileMethod::SEARCH_RESULT_FOUND_SHARED_FILE_LIMITED == result)
                    provider->addPermittedRequestor(librarymixer_id);

                activeFileServes[hash] = provider;

                /* Now that we have created a file serve, we also handle their data request. */
                sendRequestedData(provider, librarymixer_id, hash, size, offset, chunksize);
                return true;
            } else delete provider;
        }
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
