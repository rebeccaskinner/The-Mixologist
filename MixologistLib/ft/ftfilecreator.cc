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

#include <ft/ftfilecreator.h>
#include <util/dir.h>
#include <util/debug.h>
#include <time.h>

#include <QFileInfo>

#include <cstdlib>

/* This is the max age a chunk that has been allocated can get to before being re-requested next time a chunk is requested.
   Note that this number must be significantly larger than the rtt time in transferModule,
   otherwise when transferModule tentatively increases the chunk size requested and we fall behind,
   we'll start duplicatively re-requesting chunks before transferModule has time just to ease up on the throttle
   (it's really bad for transfer rates when the duplicative requests pile up). */
#define CHUNK_MAX_AGE 20

ftFileCreator::ftFileCreator(QString path, uint64_t size, QString hash)
    :ftFileProvider(path, size, hash), fileWriteAccessor(NULL) {

    log(LOG_DEBUG_BASIC, FTFILECREATORZONE,
        QString("ftFileCreator() ") +
        " path: " + path +
        " size: " + QString::number(size) +
        " hash: " + hash);

    /* The amount of the file on disk when initializing is the amount that has been received. */
    bytesSaved = QFileInfo(path).size();

    /* During initialization, we haven't yet requested anything beyond the amount received. */
    firstUnrequestedByte = bytesSaved;

    /* Handle the specical case of a 0 byte file, where addFileData will never be called. */
    if (size == 0) {
        QFile finishedFile(path);
        finishedFile.open(QIODevice::WriteOnly);
        finishedFile.close();
    }
}

ftFileCreator::~ftFileCreator() {
    foreach (uint64_t currentChunk, mReceivedChunks.keys()) {
        free(mReceivedChunks[currentChunk].data);
    }
}

void ftFileCreator::tick() {
    QMutexLocker stack(&ftcMutex);

    /* Write all received chunks to disk that are ready to be written. */
    foreach (uint64_t currentReceivedStartingByte, mReceivedChunks.keys()) {
        /* The bytesSaved will be incremented by writeFileData if we succesfully write a chunk.
           Therefore, it's possible to start from the beginning and keep writing chunks.
           Because keys is returned in order by a map, we stop as soon as we hit a chunk that isn't where we're ready to write. */
        if (currentReceivedStartingByte <= bytesSaved) {
            /* If we are unsuccessful in writing to the file, for example for a full disk, we do not remove the data so we can try again. */
            if (writeFileData(currentReceivedStartingByte,
                              mReceivedChunks[currentReceivedStartingByte].lengthInBytes,
                              mReceivedChunks[currentReceivedStartingByte].data)) {
                free(mReceivedChunks[currentReceivedStartingByte].data);
                mReceivedChunks.remove(currentReceivedStartingByte);
            } else break;
        } else break;
    }
}

void ftFileCreator::closeFile() {
    QMutexLocker stack(&ftcMutex);
    if (fileWriteAccessor) {
        fileWriteAccessor->close();
        fileWriteAccessor->deleteLater();
        fileWriteAccessor = NULL;
    }
}

bool ftFileCreator::finished() const {
    QMutexLocker stack(&ftcMutex);
    return bytesSaved == fullFileSize;
}

uint64_t ftFileCreator::amountReceived() const {
    QMutexLocker stack(&ftcMutex);
    return bytesSaved;
}

bool ftFileCreator::addFileData(unsigned int friend_id, uint64_t startingByte, uint32_t lengthInBytes, void *data) {
    QMutexLocker stack(&ftcMutex);

    if (fileWriteAccessor == NULL){
        log(LOG_DEBUG_ALERT, FTFILECREATORZONE, "ftFileCreator::addFileData() preparing to write to " + path);
        fileWriteAccessor= new QFile(path);
        if (!fileWriteAccessor->open(QIODevice::ReadWrite)) {
            free(data);
            return false;
        }
    }

    if (mRequestedChunks.isEmpty()) {
        log(LOG_WARNING, FTFILECREATORZONE,
            "Received a file chunk at " + QString::number(startingByte) +
            " when we have no outstanding requests for " + path);
        free(data);
        return false;
    }

    /* Guard against any strange behavior, and make sure data isn't after the latest point in the file we're accepting.
       (The opposite where we receive data already written could just be retransmission, and unnecessary data is dropped by writeFileData().) */
    if (startingByte >= firstUnrequestedByte) {
        log(LOG_WARNING, FTFILECREATORZONE,
            "Received a file chunk at " + QString::number(startingByte) +
            " containing data we never requested for " + path);
        free(data);
        return false;
    }
    if (startingByte + lengthInBytes > firstUnrequestedByte) {
        lengthInBytes = firstUnrequestedByte - startingByte;
        log(LOG_WARNING, FTFILECREATORZONE, "Received a file chunk greater than extends past what we requested, adjusting chunk size");
    }

    /* Add the data to our received list, and remove from requested list.
       This way if we are missing a few bytes at the beginning, we can simply re-request those few bytes while knowing we have the rest cached. */
    if (mReceivedChunks.contains(startingByte)) {
        log(LOG_WARNING, FTFILECREATORZONE, "Ignoring duplicative data at " + QString::number(startingByte) + " for " + path);
        free(data);
        return true;
    }
    mReceivedChunks[startingByte] = receivedChunk(startingByte, lengthInBytes, data, friend_id);
    removeFromRequestedChunks(startingByte, lengthInBytes);

    return true;
}

bool ftFileCreator::writeFileData(uint64_t startingByte, uint32_t lengthInBytes, void *data) {
    if (startingByte + lengthInBytes <= bytesSaved) {
        log(LOG_WARNING, FTFILECREATORZONE,
            "Discarding data already written at " + QString::number(startingByte) +
            " with length " + QString::number(lengthInBytes) +
            " for " + path);
        return true;
    }

    uint64_t byteToWriteFrom;
    uint64_t bytesToWrite;
    uint64_t offsetInData;
    if (startingByte < bytesSaved) {
        log(LOG_WARNING, FTFILECREATORZONE,
            "Partially discarding data already written at " + QString::number(startingByte) +
            " with length " + QString::number(lengthInBytes) +
            " for " + path);
        byteToWriteFrom = bytesSaved;
        bytesToWrite = lengthInBytes - (bytesSaved - startingByte);
        offsetInData = lengthInBytes - bytesToWrite;
    } else {
        byteToWriteFrom = startingByte;
        bytesToWrite = lengthInBytes;
        offsetInData = 0;
    }

    fileWriteAccessor->seek(byteToWriteFrom);
    if (fileWriteAccessor->write(&(((char *)data)[offsetInData]), bytesToWrite) == -1) {
        log(LOG_ERROR, FTFILECREATORZONE, "Error while attempting to write to file " + path);
        return false;
    }

    /* Update how much has been completed, and no longer needs to be requested. */
    bytesSaved += bytesToWrite;

    /* Normally it's fine that the QFile is buffered, as it presumably improves performance.
       However, on the final finish of the file, it is important that it is written to disk,
       otherwise when the file is moved in ftController, it may be truncated. */
    if (bytesSaved == fullFileSize) {
        if (!fileWriteAccessor->flush()) {
            log(LOG_ERROR, FTFILECREATORZONE, "Error while attempting to write to file " + path);
            return false;
        } else {
            fileWriteAccessor->close();
        }
    }

    return true;
}

bool ftFileCreator::moveFileToDirectory(QString newPath) {
    bool ok;
    QMutexLocker stack(&ftcMutex);
    if (fileWriteAccessor) fileWriteAccessor->close();

    QFileInfo fileToMove(path);
    QString fullNewPath = newPath + QDir::separator() + fileToMove.fileName();

    ok = DirUtil::moveFile(path, fullNewPath);
    if (ok) {
        log(LOG_DEBUG_ALERT, FTFILEPROVIDERZONE, "ftFileProvider::moveFile() succeeded");
        if (fileWriteAccessor) {
            fileWriteAccessor->deleteLater();
            fileWriteAccessor = NULL;
        }
        path = fullNewPath;
        return true;
    } else {
        log(LOG_DEBUG_ALERT, FTFILEPROVIDERZONE, "ftFileProvider::moveFile() failed");
        return false;
    }
}

bool ftFileCreator::deleteFileFromDisk() {
    closeFile();
    QFile fileToDelete(path);
    return fileToDelete.remove();
}

bool ftFileCreator::allocateRemainingChunk(unsigned int friend_id, uint64_t &startingByte, uint32_t &lengthInBytes) {
    QMutexLocker stack(&ftcMutex);

    if (bytesSaved == fullFileSize) return false;

    time_t currentTime = time(NULL);

    /* Check for timed out chunks we can hand out again. */
    foreach (uint64_t currentChunk, mRequestedChunks.keys()) {
        if (mRequestedChunks[currentChunk].requestTime + CHUNK_MAX_AGE < currentTime) {
            mRequestedChunks[currentChunk].requestTime = currentTime;
            lengthInBytes = mRequestedChunks[currentChunk].lengthInBytes;
            startingByte = mRequestedChunks[currentChunk].startingByte;

            log(LOG_DEBUG_ALERT, FTFILECREATORZONE,
                "ftFileCreator::allocateRemainingChunk() re-requesting timed out chunk request at " + QString::number(mRequestedChunks[currentChunk].startingByte) +
                " with length " + QString::number(lengthInBytes) +
                " for " + path);

            return true;
        }
    }

    /* Never allocate a chunk larger than remaining amont to be transferred */
    if (fullFileSize - firstUnrequestedByte < lengthInBytes) lengthInBytes = fullFileSize - firstUnrequestedByte;

    if (lengthInBytes == 0) return true;

    startingByte = firstUnrequestedByte;
    firstUnrequestedByte += lengthInBytes;

    log(LOG_DEBUG_BASIC, FTFILECREATORZONE,
        QString("ftFileCreator::allocateRemainingChunk() adding new chunk") +
        " bytesSaved: " + QString::number(bytesSaved) +
        " firstUnrequestedByte: " + QString::number(firstUnrequestedByte) +
        " fullFileSize: " + QString::number(fullFileSize));
    mRequestedChunks[startingByte] = requestedChunk(startingByte, lengthInBytes, currentTime, friend_id);

    return true;
}

/* Chart of all possible cases and the proper response. Assume that we have an original request that is 10 bytes from 10-20.
   The word start is used inclusive, the word ending is used non-inclusive.
   SIZE MATCH
   9-19  move up old start to 20
   10-20 remove
   11-21 move down old end to 11

   SMALLER SIZE
   9-17  move up old start to 18
   10-18 move up old start to 19
   11-19 move down old end to 11, create new starting from 19
   12-20 move down old end to 12
   13-21 move down old end to 13

   LARGER SIZE
   8-19  move up old start to 20
   9-20  remove
   10-21 remove
   11-22 move down old end to 11

   In light of the chart, the rule is easy to see.
   If the totality of the request is encompassed, remove the old request.
   If the totality of the response is encompassed with room to spare, split the old request.
   Otherwise, if the ending isn't encompassed, move up the start, and if the start isn't encompassed, move down the end. */
void ftFileCreator::removeFromRequestedChunks(uint64_t receivedStartingByte, uint32_t receivedSize) {
    foreach (uint64_t currentStartingByte, mRequestedChunks.keys()) {
        /* If we've already passed the end of the chunk in iterating through our requests, then we're done. */
        uint64_t firstByteAfterReceived = receivedStartingByte + receivedSize;
        if (firstByteAfterReceived <= currentStartingByte) return;

        /* If we haven't reached the beginning of the chunk yet, keep iterating. */
        uint64_t firstByteAfterCurrent = currentStartingByte + mRequestedChunks[currentStartingByte].lengthInBytes;
        if (receivedStartingByte >= firstByteAfterCurrent) continue;

        /* If the old request is completely contained by the new chunk, remove it. */
        if (receivedStartingByte <= currentStartingByte &&
            firstByteAfterReceived >= firstByteAfterCurrent) {
            mRequestedChunks.remove(currentStartingByte);
            continue;
        }

        /* If the old request completely contains the new chunk with room to spare on both ends, split it. */
        if (currentStartingByte < receivedStartingByte &&
            firstByteAfterCurrent > firstByteAfterReceived) {
            mRequestedChunks[currentStartingByte].lengthInBytes = receivedStartingByte - currentStartingByte;

            mRequestedChunks[receivedStartingByte + receivedSize] = requestedChunk(firstByteAfterReceived,
                                                                                   firstByteAfterCurrent - firstByteAfterReceived,
                                                                                   mRequestedChunks[currentStartingByte].requestTime,
                                                                                   mRequestedChunks[currentStartingByte].friend_requested_from);

            return;
        }

        /* If the old request extends farther than the received data, just move up the old start point.
           Note we already know the old request won't also start earlier. */
        if (firstByteAfterCurrent > firstByteAfterReceived) {
            mRequestedChunks[currentStartingByte].startingByte = firstByteAfterReceived;
            mRequestedChunks[currentStartingByte].lengthInBytes = firstByteAfterCurrent - firstByteAfterReceived;
            mRequestedChunks[firstByteAfterReceived] = mRequestedChunks[currentStartingByte];
            mRequestedChunks.remove(currentStartingByte);
            return;
        }

        /* If the old request starts earlier than the received data, just move down the old end point.
           Note we already know the old request won't also extend farther. */
        if (currentStartingByte < receivedStartingByte) {
            mRequestedChunks[currentStartingByte].lengthInBytes = receivedStartingByte - currentStartingByte;
            continue;
        }
    }
}

void ftFileCreator::invalidateChunksRequestedFrom(unsigned int friend_id) {
    QMutexLocker stack(&ftcMutex);
    foreach (uint64_t currentChunk, mRequestedChunks.keys()) {
        if (mRequestedChunks[currentChunk].friend_requested_from == friend_id) {
            mRequestedChunks.remove(currentChunk);
        }
    }
    /* We also clear out received but unsaved data, which is generally out-of-order data,
       as we will likely not receiving the remaining data we need to write this our-of-order data to disk anytime soon. */
    foreach (uint64_t currentChunk, mReceivedChunks.keys()) {
        if (mReceivedChunks[currentChunk].friend_received_from == friend_id) {
            free(mReceivedChunks[currentChunk].data);
            mReceivedChunks.remove(currentChunk);
        }
    }
}

#ifdef false
bool    ftFileCreator::getFileData(uint64_t startingByte, uint32_t &lengthInBytes, void *data) {
    {
        QMutexLocker stack(&ftcMutex);
        /* If we don't have the data */
        if (startingByte + lengthInBytes > bytesSaved) return false;
    }

    return ftFileProvider::getFileData(startingByte, lengthInBytes, data);
}


bool ftFileCreator::locked_printChunkMap() {
    std::cerr << "ftFileCreator::locked_printChunkMap()";
    std::cerr << " this: " << this;
    std::cerr << std::endl;

    /* check start point */
    std::cerr << "Size: " << fullFileSize << " Start: " << bytesSaved << " End: " << firstUnrequestedByte;
    std::cerr << std::endl;
    std::cerr << "\tOutstanding Chunks (in the middle)";
    std::cerr << std::endl;

    QMap<uint64_t, requestedChunk>::iterator it;

    time_t ts = time(NULL);
    for (it = mRequestedChunks.begin(); it != mRequestedChunks.end(); it++) {
        std::cerr << "\tChunk [" << it->second.startingByte << "] size: ";
        std::cerr << it->second.lengthInBytes;
        std::cerr << "  Age: " << ts - it->second.ts;
        std::cerr << std::endl;
    }
    return true;
}
#endif
