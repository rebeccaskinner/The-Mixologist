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

#include "ftfileprovider.h"

#include "util/debug.h"
#include <util/dir.h>
#include <stdlib.h>
#include <stdio.h>
#include <interface/peers.h>
#include <time.h>

#include <QFile>

ftFileProvider::ftFileProvider(QString _path, uint64_t size, QString hash)
    :fullFileSize(size), hash(hash), path(_path), file(NULL), internalMixologistFile(false) {}

ftFileProvider::~ftFileProvider() {
    if (file != NULL) {
        file->close();
        file->deleteLater();
    }
}

bool ftFileProvider::checkFileValid() {
    QFileInfo info(path);
    if (!info.exists()) return false;
    if (info.size() != (qlonglong)fullFileSize) return false;
    return true;
}

bool ftFileProvider::FileDetails(uploadFileInfo &fileInfo) {
    QMutexLocker stack(&ftcMutex);
    fileInfo.path = path;

    fileInfo.peers.clear();

    foreach (unsigned int librarymixer_id, requestingFriends.keys()) {
        TransferInfo transferInfo;

        transferInfo.librarymixer_id = librarymixer_id;
        transferInfo.transferred = requestingFriends[librarymixer_id].transferred;
        transferInfo.transferRate = requestingFriends[librarymixer_id].transferRate/1024.0;

        if (time(NULL) - requestingFriends[librarymixer_id].lastRequestTime > 5)
            transferInfo.status = FT_STATE_WAITING;
        else
            transferInfo.status = FT_STATE_TRANSFERRING;
        fileInfo.peers.push_back(transferInfo);
    }

    return true;
}

void ftFileProvider::closeFile() {
    QMutexLocker stack(&ftcMutex);
    if (file) file->close();
}

QString ftFileProvider::getPath() const {
    return path;
}

QString ftFileProvider::getHash() const {
    return hash;
}

uint64_t ftFileProvider::getFileSize() const {
    return fullFileSize;
}

bool ftFileProvider::getFileData(uint64_t offset, uint32_t &chunk_size, void *data, unsigned int librarymixer_id) {

    QMutexLocker stack(&ftcMutex);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    uint32_t requestSize = chunk_size;
    uint64_t baseFileOffset = offset;

    if (baseFileOffset + requestSize > fullFileSize) {
        requestSize = fullFileSize - baseFileOffset;
        chunk_size = fullFileSize - baseFileOffset;
        log(LOG_DEBUG_BASIC, FTFILEPROVIDERZONE,
            "ftFileProvider::getFileData() Chunk Size greater than total file size, adjusting chunk size " +
            QString::number(requestSize));
    }

    if (requestSize <= 0) {
        log(LOG_DEBUG_ALERT, FTFILEPROVIDERZONE, "ftFileProvider::getFileData() No data to read");
        return false;
    }

    file.seek(baseFileOffset);

    if (file.read((char *)data, requestSize) == -1) return false;

    /* Update stats. */
    time_t currentTime = time(NULL);

    /* Create a new friend entry if necessary. */
    if (!requestingFriends.contains(librarymixer_id)) {
        requestingFriends[librarymixer_id].transferRate = 0;
        requestingFriends[librarymixer_id].lastTransferRateCalc = currentTime;
        requestingFriends[librarymixer_id].transferredSinceLastCalc = 0;
    } else {
        long int timeSinceLastCalc = (long int)currentTime - (long int)requestingFriends[librarymixer_id].lastTransferRateCalc;
        if (timeSinceLastCalc > 3) {
            requestingFriends[librarymixer_id].transferRate = requestingFriends[librarymixer_id].transferredSinceLastCalc /
                                                              (float) timeSinceLastCalc;
            requestingFriends[librarymixer_id].lastTransferRateCalc = currentTime;
            requestingFriends[librarymixer_id].transferredSinceLastCalc = 0;
        }
    }

    requestingFriends[librarymixer_id].lastRequestedEnd = baseFileOffset + requestSize;
    requestingFriends[librarymixer_id].lastRequestTime = currentTime;
    requestingFriends[librarymixer_id].transferredSinceLastCalc += requestSize;
    requestingFriends[librarymixer_id].transferred += requestSize;

    return true;
}

bool ftFileProvider::moveFile(QString newPath) {
    bool ok;
    QMutexLocker stack(&ftcMutex);
    file->close();
    ok = DirUtil::moveFile(path, newPath);
    if (ok) {
        log(LOG_DEBUG_ALERT, FTFILEPROVIDERZONE, "ftFileProvider::moveFile() succeeded");
        file->deleteLater();
        file=NULL;
        path = newPath;
        return true;
    } else {
        log(LOG_DEBUG_ALERT, FTFILEPROVIDERZONE, "ftFileProvider::moveFile() failed");
        return false;
    }
}

void ftFileProvider::addPermittedRequestor(unsigned int librarymixer_id) {
    QMutexLocker stack(&ftcMutex);
    if (!permittedRequestors.contains(librarymixer_id)) permittedRequestors.append(librarymixer_id);
}

void ftFileProvider::allowAllRequestors() {
    QMutexLocker stack(&ftcMutex);
    permittedRequestors.clear();
}

bool ftFileProvider::isPermittedRequestor(unsigned int librarymixer_id) {
    QMutexLocker stack(&ftcMutex);
    if (permittedRequestors.isEmpty()) return true;
    return permittedRequestors.contains(librarymixer_id);
}
