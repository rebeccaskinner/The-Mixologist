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

#include "ftfilecreator.h"
#include <QFileInfo>
#include "util/debug.h"
#include <time.h>

#define CHUNK_MAX_AGE 20 //20 seconds

ftFileCreator::ftFileCreator(QString path, uint64_t size, QString hash)
    : ftFileProvider(path, size, hash) {

    {
        QString toLog = "ftFileCreator()";
        toLog += " path: " + path;
        toLog += " size: " + QString::number(size);
        toLog += " hash: " + hash;
        log(LOG_DEBUG_BASIC, FTFILECREATORZONE, toLog);
    }

    firstUnreadByte = QFileInfo(path).size();
    lastRequestedByte = firstUnreadByte;

    /* Handle the specical case of a 0 byte file, where addFileData will never be called. */
    if (size == 0) {
        QFile finishedFile(path);
        finishedFile.open(QIODevice::WriteOnly);
        finishedFile.close();
    }
}

bool ftFileCreator::finished() const {
    int received = amountReceived();
    int total = getFileSize();
    return received == total;
}

uint64_t ftFileCreator::amountReceived() const {
    return firstUnreadByte;
}

bool ftFileCreator::addFileData(uint64_t offset, uint32_t chunk_size, void *data) {
    {
        QString toLog = "ftFileCreator::addFileData()";
        toLog += " offset: " + QString::number(offset);
        toLog += " chunksize: " + QString::number(chunk_size);
        log(LOG_DEBUG_BASIC, FTFILECREATORZONE, toLog);
    }

    QMutexLocker stack(&ftcMutex);

    if (file == NULL){
        log(LOG_DEBUG_ALERT, FTFILECREATORZONE, "ftFileCreator::addFileData() creating " + path);
        file = new QFile(path);
        if (!file->open(QIODevice::ReadWrite)) return false;
    }

    if (offset + chunk_size > fullFileSize) {
        chunk_size = fullFileSize - offset;
        log(LOG_WARNING, FTFILECREATORZONE, "Received a file chunk greater than total file size, adjusting chunk size");
    }

    if (!locked_updateChunkMap(offset, chunk_size)) return false;

    file->seek(offset);

    if (file->write((char *)data, chunk_size) == -1) {
        log(LOG_ERROR, FTFILECREATORZONE, "Error while attempting to write to file " + path);
        return false;
    }

    /* Normally it's fine that the QFile is buffered, as it presumably improves performance.
       However, on the final finish of the file, it is important that it is written to disk,
       otherwise when the file is moved in ftController, it may be truncated. */
    if (finished()) {
        if (!file->flush()) {
            log(LOG_ERROR, FTFILECREATORZONE, "Error while attempting to write to file " + path);
            return false;
        } else {
            file->close();
        }
    }

    return true;
}

bool ftFileCreator::deleteFileFromDisk() {
    if (file) return file->remove();
    return true;
}

bool ftFileCreator::locked_updateChunkMap(uint64_t offset, uint32_t chunk_size) {
    /* Find the chunk */
    QMap<uint64_t, ftChunk>::iterator it;
    it = mChunks.find(offset);
    if (it == mChunks.end()) {
        log(LOG_WARNING, FTFILECREATORZONE, "Received an unrequested file chunk for " + path);
        return false;
    }

    /* If the chunk received is not the first, that means we have an unfilled chunk earlier in the file than this.
       We should really be storing this so that when we receive that earlier chunk we can save this to the file as well.
       However, for now, we will simply discard out of order chunks, which should be fairly rare before multi-source is implemented. */
    if (it != mChunks.begin()) {
        log(LOG_WARNING, FTFILECREATORZONE, "Received an out of order file chunk for " + path);
        return false;
    }

    /* Remove it from mChunks */
    ftChunk allocated_chunk = it.value();
    mChunks.erase(it);

    /* Partial chunk : We frequently request a larger chunk than can be returned in a single reponse.
       The sender will respond in its ftServer::sendData by breaking it up into many responses,
       so we should accept what we received, and then shrink and move up the chunk. */
    if (allocated_chunk.chunk_size > chunk_size) {
        log(LOG_DEBUG_BASIC, FTFILECREATORZONE,
            QString("ftFileCreator::locked_updateChunkMap() received partial chunk") +
            " expected size: " + QString::number(allocated_chunk.chunk_size) +
            " received size: " + QString::number(chunk_size));
        allocated_chunk.chunk_size -= chunk_size;
        allocated_chunk.offset += chunk_size;
        mChunks[allocated_chunk.offset] = allocated_chunk;
    }

    /* Update how much has been completed */
    firstUnreadByte = offset + chunk_size;

    if (mChunks.size() == 0) firstUnreadByte = lastRequestedByte;

    return true;
}

bool ftFileCreator::allocateRemainingChunk(uint64_t &offset, uint32_t &chunk_size) {
    QMutexLocker stack(&ftcMutex);

    if (finished()) {
        return false;
    }

    time_t currentTime = time(NULL);

    /* Check for timed out chunks */
    QMap<uint64_t, ftChunk>::iterator it;
    for (it = mChunks.begin(); it != mChunks.end(); it++) {
        if (it.value().requestTime < (currentTime - CHUNK_MAX_AGE)) {
            log(LOG_DEBUG_ALERT, FTFILECREATORZONE, "ftFileCreator::allocateRemainingChunk() chunk request timed out, re-requesting");
            it.value().requestTime = currentTime;
            chunk_size = it.value().chunk_size;
            offset = it.value().offset;

            return true;
        }
    }

    /* Never allocate a chunk larger than remaining amont to be transferred */
    if (fullFileSize - lastRequestedByte < chunk_size) chunk_size = fullFileSize - lastRequestedByte;

    offset = lastRequestedByte;
    lastRequestedByte += chunk_size;

    if (chunk_size > 0) {
        log(LOG_DEBUG_BASIC, FTFILECREATORZONE,
            QString("ftFileCreator::allocateRemainingChunk() adding new chunk") +
            " firstUnreadByte: " + QString::number(firstUnreadByte) +
            " lastRequestedByte: " + QString::number(lastRequestedByte) +
            " fullFileSize: " + QString::number(fullFileSize));
        mChunks[offset] = ftChunk(offset, chunk_size, currentTime);
    }

    return true;
}


#ifdef false
bool    ftFileCreator::getFileData(uint64_t offset, uint32_t &chunk_size, void *data) {
    {
        QMutexLocker stack(&ftcMutex);
        /* If we don't have the data */
        if (offset + chunk_size > firstUnreadByte) return false;
    }

    return ftFileProvider::getFileData(offset, chunk_size, data);
}


bool ftFileCreator::locked_printChunkMap() {
    std::cerr << "ftFileCreator::locked_printChunkMap()";
    std::cerr << " this: " << this;
    std::cerr << std::endl;

    /* check start point */
    std::cerr << "Size: " << fullFileSize << " Start: " << firstUnreadByte << " End: " << lastRequestedByte;
    std::cerr << std::endl;
    std::cerr << "\tOutstanding Chunks (in the middle)";
    std::cerr << std::endl;

    QMap<uint64_t, ftChunk>::iterator it;

    time_t ts = time(NULL);
    for (it = mChunks.begin(); it != mChunks.end(); it++) {
        std::cerr << "\tChunk [" << it->second.offset << "] size: ";
        std::cerr << it->second.chunk_size;
        std::cerr << "  Age: " << ts - it->second.ts;
        std::cerr << std::endl;
    }
    return true;
}
#endif
