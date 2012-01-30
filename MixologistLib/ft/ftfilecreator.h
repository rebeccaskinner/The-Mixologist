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


#ifndef FT_FILE_CREATOR_HEADER
#define FT_FILE_CREATOR_HEADER

#include "ftfileprovider.h"
#include <QMap>

class ftChunk;

/*
 * Corresponds to a single file that is being written to, and by extending ftFileProvider,
 * also represents read access to that file.
 */
class ftFileCreator: public ftFileProvider {
public:

    /* In initializing to the stated information, if there is already a file at savepath,
       it is assumed that file is a partial copy of our target file, and file creation
       will resume using that file as its base. */
    ftFileCreator(QString savepath, uint64_t size, QString hash);

    /* Returns if this file is completed. */
    bool finished() const;

    /* Returns the amount of the file received so far. */
    uint64_t amountReceived() const;

    /* Called from ftTransferModule to find out the next chunk of the file to request.
       Checks to see if there are any old requests that need to be re-requested, and if so returns one of those.
       Otherwise, returns the next section of the file that is needed, and adds it to mChunks.
       Returns false only if the file is already complete.
       If there are no chunks left to request, will return a chunk_size of 0, but still be true. */
    bool allocateRemainingChunk(uint64_t &offset, uint32_t &chunk_size);

    /* Called from ftTransferModule to write newly received file data */
    bool addFileData(uint64_t offset, uint32_t chunk_size, void *data);

    /* Closes any open file handle and deletes the file from disk. Useful when cancelling a download. */
    bool deleteFileFromDisk();

    /* Overloaded from FileProvider
       This is to be able to send parts of a file that the user is currently downloading.
       Not currently used, as multi-source downloading is not working yet. */
    //virtual bool getFileData(uint64_t offset, uint32_t &chunk_size, void *data);
private:

    /* Updates mChunks with the last chunk received, returns false if this was an unrequested chunk. */
    bool locked_updateChunkMap(uint64_t offset, uint32_t chunk_size);

    /* Amount of the file received so far. */
    uint64_t firstUnreadByte;

    /* Amount of the file chunks have been requested for so far. */
    uint64_t lastRequestedByte;

    /* This map tracks all of the parts of the file that have been requested so far.
       When there are no outstanding requests, it will be empty. */
    QMap<uint64_t, ftChunk> mChunks; //offsets, ftChunk
};

/* These are stored in mChunks to represent the chunks that have been requested. */
class ftChunk {
public:
    ftChunk(uint64_t ioffset,uint64_t size,time_t now): offset(ioffset), chunk_size(size), requestTime(now) {}
    ftChunk():offset(0), chunk_size(0), requestTime(0) {}
    ~ftChunk() {}

    uint64_t offset;
    uint64_t chunk_size;
    time_t   requestTime;
};

#endif // FT_FILE_CREATOR_HEADER
