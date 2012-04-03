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
#include "interface/files.h"
#include <QFile>
#include <QMutex>

/*
 * ftFileProvider represents a single file that is being read from.
 * This is extended by ftFileCreator in order to represent a file being written to.
 */
class ftFileProvider {
public:
    ftFileProvider(QString path, uint64_t size, QString hash);
    virtual ~ftFileProvider();

    //Returns true if the file is found on disk and has the size expected. Does not check hash.
    bool checkFileValid();

    //Called by ftDataDemultiplex to fill out the uploadFileInfo with all of the stats on this file
    virtual bool FileDetails(uploadFileInfo &fileInfo);

    //Reads from the file starting at the specified offset for an amount equal to chunk_size into data
    //Returns false on any type of failure to read
    virtual bool getFileData(uint64_t offset, uint32_t &chunk_size, void *data);

    //Closes the file handle to the file
    void closeFile();

    //Return the file's full path
    QString getPath() const;

    //Returns the file hash
    QString getHash() const;

    //Returns the file size
    uint64_t getFileSize() const;

    //Called from ftDataDemultiplex to update the stat on the last friend to have requested this
    void setLastRequestor(unsigned int librarymixer_id);

    //Moves the old file to new location and updates internal variables
    bool moveFile(QString newPath);

    //Accessors for a flag on whether this is an internal Mixologist file.
    //Generally used to share files without displaying them in the UI
    bool isInternalMixologistFile() const {return internalMixologistFile;}
    void setInternalMixologistFile(bool newValue) {internalMixologistFile = newValue;}

protected:
    //Total file size of the file
    uint64_t fullFileSize;
    //Hash of the file
    QString hash;
    //Path to the file
    QString path;
    //QFile object that we use for all file operations
    QFile *file;
    //True if this is a file that is being shared for the Mixologist's own operations rather than by the user.
    //Used by off-LibraryMixer sharing when transfering the XML share information
    bool internalMixologistFile;

    //These stats are used to report information to the GUI
    //Right now, we are combining stats if multiple friends are requesting the same file
    //This is not ideal, and should be fixed in the future
    //The librarymixer_id of the last friend to have requested this
    unsigned int lastRequestor;
    //The offset of the last request + the amount last requested
    uint64_t lastRequestedEnd;
    uint32_t lastRequestSize;
    time_t lastRequestTime; //Used for checking if it's alive

    float transferRate;
    time_t lastTransferRateCalc; //Used for estimating transfer rate.
    uint32_t transferredSinceLastCalc;

    mutable QMutex ftcMutex;
};

#endif // FT_FILE_PROVIDER_HEADER
