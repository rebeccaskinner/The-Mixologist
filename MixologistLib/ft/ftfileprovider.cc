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

#include <QFile>

ftFileProvider::ftFileProvider(QString _path, uint64_t size, std::string hash)
    : fullFileSize(size), hash(hash), path(_path), file(NULL), transferRate(0), transferredSinceLastCalc(0) {
    lastRequestTime = time(NULL);
    lastTransferRateCalc = lastRequestTime;
}

ftFileProvider::~ftFileProvider() {
    if (file != NULL) {
        file->close();
        file->deleteLater();
    }
}

void ftFileProvider::setLastRequestor(const std::string &id) {
    MixStackMutex stack(ftcMutex);
    lastRequestor = id ;
}

bool ftFileProvider::FileDetails(FileInfo &fileInfo) {
    MixStackMutex stack(ftcMutex);
    fileInfo.hash = hash;
    fileInfo.size = fullFileSize;
    fileInfo.paths.append(path);
    fileInfo.transfered = lastRequestedEnd;
    fileInfo.lastTS = lastRequestTime;
    fileInfo.status = FT_STATE_DOWNLOADING;

    fileInfo.peers.clear();

    TransferInfo tInfo;
    tInfo.cert_id = lastRequestor;
    tInfo.librarymixer_id = peers->findLibraryMixerByCertId(lastRequestor);
    tInfo.status = FT_STATE_DOWNLOADING;
    tInfo.tfRate = transferRate/1024.0;

    fileInfo.tfRate = transferRate/1024.0;
    fileInfo.peers.push_back(tInfo);

    return true;
}

bool ftFileProvider::getFileData(uint64_t offset, uint32_t &chunk_size, void *data) {

    MixStackMutex stack(ftcMutex);

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

    //Update stats
    time_t currentTime = time(NULL);

    long int timeSinceLastCalc = (long int)currentTime - (long int)lastTransferRateCalc;

    if (timeSinceLastCalc > 3) {
        transferRate = transferredSinceLastCalc / (float)timeSinceLastCalc ;
        log(LOG_DEBUG_BASIC, FTFILEPROVIDERZONE,
            "ftFileProvider::getFileData() transfer rate: " + QString::number(transferRate) +
            " transferredSinceLastCalc: " + QString::number(transferredSinceLastCalc));
        lastTransferRateCalc = currentTime;
        transferredSinceLastCalc = 0;
    }

    lastRequestedEnd = baseFileOffset + requestSize;
    lastRequestTime = currentTime;
    lastRequestSize = requestSize;
    transferredSinceLastCalc += lastRequestSize;

    return true;
}

bool ftFileProvider::moveFile(QString newPath) {
    bool ok;
    MixStackMutex stack(ftcMutex);
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
