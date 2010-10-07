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

/*
 * ftFileCreator
 *
 * Corresponds to a single file that is being written to.
 *
 */
#include "ftfileprovider.h"
#include <map>
class ftChunk;

class ftFileCreator: public ftFileProvider {
public:

    ftFileCreator(QString savepath, uint64_t size, std::string hash);

    /* overloaded from FileProvider */
    virtual bool    getFileData(uint64_t offset, uint32_t &chunk_size, void *data);
    bool    finished() {return getRecvd() == getFileSize();}
    uint64_t getRecvd() {return mStart;}

    //creation functions for FileCreator
    bool    getMissingChunk(uint64_t &offset, uint32_t &chunk);
    bool    addFileData(uint64_t offset, uint32_t chunk_size, void *data);

protected:

    //Not mutex protected, call from inside mutex
    virtual int initializeFileAttrs();

private:

    bool    locked_printChunkMap();
    int     locked_notifyReceived(uint64_t offset, uint32_t chunk_size);

    //structure to track missing chunks
    uint64_t mStart;
    uint64_t mEnd;

    std::map<uint64_t, ftChunk> mChunks;
};

class ftChunk {
public:
    ftChunk(uint64_t ioffset,uint64_t size,time_t now): offset(ioffset), chunk(size), ts(now) {}
    ftChunk():offset(0), chunk(0), ts(0) {}
    ~ftChunk() {}

    uint64_t offset;
    uint64_t chunk;
    time_t   ts;
};

#endif // FT_FILE_CREATOR_HEADER
