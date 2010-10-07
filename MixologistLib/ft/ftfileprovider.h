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

#ifndef FT_FILE_PROVIDER_HEADER
#define FT_FILE_PROVIDER_HEADER

/*
 * ftFileProvider.
 * Corresponds to a single file that is being read from.
 */
#include <iostream>
#include <stdint.h>
#include "util/threads.h"
#include "interface/files.h"
#include <QFile>

class ftFileProvider {
public:
    ftFileProvider(QString path, uint64_t size, std::string hash);
    virtual ~ftFileProvider();

    virtual bool    getFileData(uint64_t offset, uint32_t &chunk_size, void *data);
    virtual bool    FileDetails(FileInfo &info);
    std::string getHash();
    uint64_t getFileSize(){return mSize;}

    void setPeerId(const std::string &id) ;

    //Moves the old file to new location and updates internal variables
    bool moveFile(QString newPath);

protected:
    uint64_t    mSize;
    std::string hash;
    QString path;
    QFile *file;

    /*
     * Structure to gather statistics FIXME: lastRequestor - figure out a
     * way to get last requestor (peerID)
     */
    std::string lastRequestor;
    uint64_t   req_loc;
    uint32_t   req_size;
    time_t    lastTS;           // used for checking if it's alive
    time_t    lastTS_t;     // used for estimating transfer rate.

    // these two are used for speed estimation
    float       transfer_rate ;
    uint32_t    total_size ;

    MixMutex ftcMutex;
};


#endif // FT_FILE_PROVIDER_HEADER
