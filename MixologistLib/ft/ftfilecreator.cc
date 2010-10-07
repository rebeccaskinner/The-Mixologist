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
#include <errno.h>
#include <stdio.h>
#include <QFileInfo>

/*******
 * #define FILE_DEBUG 1
 ******/

#define CHUNK_MAX_AGE 20


/***********************************************************
*
*   ftFileCreator methods
*
***********************************************************/

ftFileCreator::ftFileCreator(QString path, uint64_t size, std::string hash)
    : ftFileProvider(path,size,hash) {

#ifdef FILE_DEBUG
    std::cerr << "ftFileCreator()";
    std::cerr << std::endl;
    std::cerr << "\tpath: " << path.toStdString();
    std::cerr << std::endl;
    std::cerr << "\tsize: " << size;
    std::cerr << std::endl;
    std::cerr << "\thash: " << hash;
    std::cerr << std::endl;
#endif

    MixStackMutex stack(ftcMutex); /********** STACK LOCKED MTX ******/

    mStart = QFileInfo(path).size();
    mEnd = mStart;
}

bool    ftFileCreator::getFileData(uint64_t offset,
                                   uint32_t &chunk_size, void *data) {
    {
        MixStackMutex stack(ftcMutex); /********** STACK LOCKED MTX ******/
        if (offset + chunk_size > mStart) {
            /* don't have the data */
            return false;
        }
    }

    return ftFileProvider::getFileData(offset, chunk_size, data);
}

bool ftFileCreator::addFileData(uint64_t offset, uint32_t chunk_size, void *data) {
#ifdef FILE_DEBUG
    std::cerr << "ftFileCreator::addFileData(";
    std::cerr << offset;
    std::cerr << ", " << chunk_size;
    std::cerr << ", " << data << ")";
    std::cerr << " this: " << this;
    std::cerr << std::endl;
#endif
    MixStackMutex stack(ftcMutex); /********** STACK LOCKED MTX ******/
    /* Check File is open */
    if (file == NULL)
        if (!initializeFileAttrs())
            return false;


    /*
     * check its at the correct location
     */
    if (offset + chunk_size > mSize) {
        chunk_size = mSize - offset;
#ifdef FILE_DEBUG
        std::cerr <<"Chunk Size greater than total file size, adjusting chunk " << "size " << chunk_size << std::endl;
#endif
    }

    /*
     * go to the offset of the file
     */
    file->seek(offset);
    /*
     * add the data
      */
    //void *data2 = malloc(chunk_size);
    //std::cerr << "data2: " << data2 << std::endl;
    //if (1 != fwrite(data2, chunk_size, 1, this->fd))

    if (file->write((char *)data, chunk_size) == -1) {
#ifdef FILE_DEBUG
        std::cerr << "ftFileCreator::addFileData() Bad fwrite" << std::endl;
        std::cerr << "ERRNO: " << errno << std::endl;
#endif
        return 0;
    }

    /*
     * Notify file chunker about chunks received
     */
    locked_notifyReceived(offset,chunk_size);

    /*
     * FIXME HANDLE COMPLETION HERE - Any better way?
     */

    return 1;
}

int ftFileCreator::initializeFileAttrs() {
#ifdef FILE_DEBUG
    std::cerr << "ftFileCreator::initializeFileAttrs() Filename: ";
    std::cerr << path;
    std::cerr << " this: " << this;
    std::cerr << std::endl;
#endif
    /*
         * check if the file exists
     * cant use FileProviders verion because that opens readonly.
         */

    if (file != NULL) return 1;
    /*
         * attempt to open file
         */

    file = new QFile(path);
    if (!file->open(QIODevice::ReadWrite)) return 0;

    return 1;
}

int ftFileCreator::locked_notifyReceived(uint64_t offset, uint32_t chunk_size) {
    /* ALREADY LOCKED */
#ifdef FILE_DEBUG
    std::cerr << "ftFileCreator::locked_notifyReceived( " << offset;
    std::cerr << ", " << chunk_size << " )";
    std::cerr << " this: " << this;
    std::cerr << std::endl;
#endif

    /* find the chunk */
    std::map<uint64_t, ftChunk>::iterator it;
    it = mChunks.find(offset);
    bool isFirst = false;
    if (it == mChunks.end()) {
#ifdef FILE_DEBUG
        std::cerr << "ftFileCreator::locked_notifyReceived() ";
        std::cerr << " Failed to match to existing chunk - ignoring";
        std::cerr << std::endl;

        locked_printChunkMap();
#endif
        return 0; /* ignoring */
    } else if (it == mChunks.begin()) {
        isFirst = true;
    }

    ftChunk chunk = it->second;
    mChunks.erase(it);

    if (chunk.chunk != chunk_size) {
        /* partial : shrink chunk */
        chunk.chunk -= chunk_size;
        chunk.offset += chunk_size;
        mChunks[chunk.offset] = chunk;
    }

    /* update how much has been completed */
    if (isFirst) {
        mStart = offset + chunk_size;
    }

    if (mChunks.size() == 0) {
        mStart = mEnd;
    }

    /* otherwise there is another earlier block to go
     */
    return 1;
}

/* Returns true if more to get
 * But can return size = 0, if we are still waiting for the data.
 */

bool ftFileCreator::getMissingChunk(uint64_t &offset, uint32_t &chunk) {
    MixStackMutex stack(ftcMutex); /********** STACK LOCKED MTX ******/
#ifdef FILE_DEBUG
    std::cerr << "ffc::getMissingChunk(...,"<< chunk << ")";
    std::cerr << " this: " << this;
    std::cerr << std::endl;
    locked_printChunkMap();
#endif

    /* check start point */

    if (finished()) return false;

    /* check for freed chunks */
    time_t ts = time(NULL);
    time_t old = ts-CHUNK_MAX_AGE;

    std::map<uint64_t, ftChunk>::iterator it;
    for (it = mChunks.begin(); it != mChunks.end(); it++) {
        /* very simple algorithm */
        if (it->second.ts < old) {
#ifdef FILE_DEBUG
            std::cerr << "ffc::getMissingChunk() ReAlloc";
            std::cerr << std::endl;
#endif

            /* retry this one */
            it->second.ts = ts;
            chunk = it->second.chunk;
            offset = it->second.offset;

            return true;
        }
    }

#ifdef FILE_DEBUG
    std::cerr << "ffc::getMissingChunk() new Alloc";
    std::cerr << "  mStart: " << mStart << " mEnd: " << mEnd;
    std::cerr << "mSize: " << mSize;
    std::cerr << std::endl;
#endif

    /* else allocate a new chunk */
    if (mSize - mEnd < chunk)
        chunk = mSize - mEnd;

    offset = mEnd;
    mEnd += chunk;

    if (chunk > 0) {
#ifdef FILE_DEBUG
        std::cerr << "ffc::getMissingChunk() Allocated " << chunk;
        std::cerr << " offset: " << offset;
        std::cerr << std::endl;
        std::cerr << "  mStart: " << mStart << " mEnd: " << mEnd;
        std::cerr << "mSize: " << mSize;
        std::cerr << std::endl;
#endif

        mChunks[offset] = ftChunk(offset, chunk, ts);
    }

    return true; /* cos more data to get */
}


bool ftFileCreator::locked_printChunkMap() {
#ifdef FILE_DEBUG
    std::cerr << "ftFileCreator::locked_printChunkMap()";
    std::cerr << " this: " << this;
    std::cerr << std::endl;

    /* check start point */
    std::cerr << "Size: " << mSize << " Start: " << mStart << " End: " << mEnd;
    std::cerr << std::endl;
    std::cerr << "\tOutstanding Chunks (in the middle)";
    std::cerr << std::endl;
#endif
    std::map<uint64_t, ftChunk>::iterator it;
#ifdef FILE_DEBUG
    time_t ts = time(NULL);
    for (it = mChunks.begin(); it != mChunks.end(); it++) {
        std::cerr << "\tChunk [" << it->second.offset << "] size: ";
        std::cerr << it->second.chunk;
        std::cerr << "  Age: " << ts - it->second.ts;
        std::cerr << std::endl;
    }
#endif
    return true;
}
