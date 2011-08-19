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

#include <iostream>
#include <stdint.h>
#include "util/threads.h"
#include "interface/files.h"
#include <QFile>

/*
 * ftFileProvider represents a single file that is being read from.
 * This is extended ftFileCreator in order to represent a file being written to.
 */
class ftFileProvider {
public:
    ftFileProvider(QString path, uint64_t size, std::string hash);
    virtual ~ftFileProvider();

    //Reads from the file starting at the specified offset for an amount equal to chunk_size into data
    //Returns false on any type of failure to read
    virtual bool getFileData(uint64_t offset, uint32_t &chunk_size, void *data);
    //Called by ftDataDemultiplex to fill out the FileInfo with all of the stats on this file
    virtual bool FileDetails(FileInfo &fileInfo);
    //Returns the file hash
    std::string getHash() {return hash;}
    //Returns the file size
    uint64_t getFileSize() {return fullFileSize;}
    //Called from ftDataDemultiplex to update the stat on the last friend to have requested this
    void setLastRequestor(const std::string &id) ;
    //Moves the old file to new location and updates internal variables
    bool moveFile(QString newPath);

protected:
    //Total file size of the file
    uint64_t fullFileSize;
    //Hash of the file
    std::string hash;
    //Path to the file
    QString path;
    //QFile object that we use for all file operations
    QFile *file;

    //These stats are used to report information to the GUI
    //Right now, we are combining stats if multiple friends are requesting the same file
    //This is not ideal, and should be fixed in the future
    //The cert_id of the last friend to have requested this
    std::string lastRequestor;
    //The offset of the last request + the amount last requested
    uint64_t lastRequestedEnd;
    uint32_t lastRequestSize;
    time_t lastRequestTime; //Used for checking if it's alive

    float transferRate;
    time_t lastTransferRateCalc; //Used for estimating transfer rate.
    uint32_t transferredSinceLastCalc;

    MixMutex ftcMutex;
};

#endif // FT_FILE_PROVIDER_HEADER
