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

#include <ft/ftfileprovider.h>
#include <QMap>

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

    ~ftFileCreator();

    /* Called from ftTransferModule to save any received data to disk. */
    void tick();

    /* Closes the file handle to the file if this is a ftFileCreator that holds the file open, otherwise does nothing. */
    virtual void closeFile();

    /* Returns if this file is completed. */
    bool finished() const;

    /* Returns the amount of the file received so far. */
    uint64_t amountReceived() const;

    /* Called from ftTransferModule to find out the next chunk of the file to request.
       lengthInBytes is the requested size of the chunk, but it may be altered if a different sized chunk is allocated.
       Checks to see if there are any old requests that need to be re-requested, and if so returns one of those.
       Otherwise, returns the next section of the file that is needed, and adds it to mRequestedChunks.
       Returns false only if the file is already complete.
       If there are no chunks left to request, will return a lengthInBytes of 0, but still be true. */
    bool allocateRemainingChunk(unsigned int friend_id, uint64_t &startingByte, uint32_t &lengthInBytes);

    /* Makes all chunks that we are currently waiting for from that friend available for allocation again.
       Useful when we get disconnected from a friend, and hence know they won't be responding. */
    void invalidateChunksRequestedFrom(unsigned int friend_id);

    /* Called from ftTransferModule to write newly received file data.
       Should always assume it frees the data (though technically sometimes it stores it for a little while internally). */
    bool addFileData(unsigned int friend_id, uint64_t startingByte, uint32_t lengthInBytes, void *data);

    /* Moves the old file to new location and updates internal variables. */
    bool moveFileToDirectory(QString newPath);

    /* Closes any open file handle and deletes the file from disk. Useful when cancelling a download. */
    bool deleteFileFromDisk();

    /* Overloaded from FileProvider
       This is to be able to send parts of a file that the user is currently downloading.
       Not currently used, as multi-source downloading is not working yet. */
    //virtual bool getFileData(uint64_t startingByte, uint32_t &lengthInBytes, void *data);
private:
    /* Removes the given set of bytes from the requested chunks. */
    void removeFromRequestedChunks(uint64_t receivedStartingByte, uint32_t lengthInBytes);

    /* Used internally to write file data when we have data ready to be written.
       fileWriteAccessor must already be initialized.
       Must be called from within mutex.
       Returns false on file write failure, where data must not be destroyed so it can be tried again. */
    bool writeFileData(uint64_t startingByte, uint32_t lengthInBytes, void *data);

    /* QFile object that we use for writing to file.
       The ftFileProvider simply reads using a temporary file object, since it has no need to hold the file open.
       Initialized only when needed, NULL if this creator has not been used to write data yet. */
    QFile *fileWriteAccessor;

    /* Amount of the file received so far. */
    uint64_t bytesSaved;

    /* First byte in the file that is unrequested.
       Alternatively, can be seen as number of bytes of the file that have been requested for so far. */
    uint64_t firstUnrequestedByte;

    /* This map tracks all of the parts of the file that have been requested so far.
       When there are no outstanding requests, it will be empty.
       Keyed by startingBytes in the file, and since this is a map, it will be in order. */
    class requestedChunk {
    public:
        /* Default constructor supplied only so QMap works. */
        requestedChunk() {}

        requestedChunk(uint64_t istartingByte, uint64_t size, time_t now, unsigned int friend_id)
            :startingByte(istartingByte), lengthInBytes(size), requestTime(now), friend_requested_from(friend_id) {}

        ~requestedChunk() {}

        uint64_t startingByte;
        uint64_t lengthInBytes;
        time_t requestTime;

        /* The friend that we requested send us this chunk.
           Useful for invalidating chunks when we get disconnected. */
        unsigned int friend_requested_from;
    };
    QMap<uint64_t, requestedChunk> mRequestedChunks;
    
    /* This map contains all data we've received in response to our requests that has not yet been written to disk.
       When there is no out of order data stored in memory, it will generally be empty except for some transient usage.
       Keyed by startingBytes in the file, and since this is a map, it will be in order. */
    class receivedChunk {
    public:
        receivedChunk() {}

        receivedChunk(uint64_t startingByte, uint64_t size, void* data, unsigned int friend_id)
            :startingByte(startingByte), lengthInBytes(size), data(data), friend_received_from(friend_id) {}

        ~receivedChunk() {}

        uint64_t startingByte;
        uint64_t lengthInBytes;
        void *data;

        /* The friend that we received this chunk from.
           We currently use this to drop all receivedChunks when we disconnect from that friend.
           When multi-source is implemented this doesn't necessarily make sense, as another friend could fill in data.
           However, for now, this is a good idea, because it prevents a disconnected friend from hogging our memory indefinitely. */
        unsigned int friend_received_from;
    };
    QMap<uint64_t, receivedChunk> mReceivedChunks;
};

#endif // FT_FILE_CREATOR_HEADER
