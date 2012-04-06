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
 * ftFileProvider represents a single file that is being read from by a single user.
 * A single file may have multiple ftFileProviders, each reading from the same file.
 * This is extended by ftFileCreator in order to represent a file being written to.
 */
class ftFileProvider {
public:
    ftFileProvider(QString path, uint64_t size, QString hash);
    virtual ~ftFileProvider();

    /* Returns true if the file is found on disk and has the size expected. Does not check hash. */
    bool checkFileValid();

    /* Called by ftDataDemultiplex to fill out the uploadFileInfo with all of the stats on this file. */
    virtual bool FileDetails(uploadFileInfo &fileInfo);

    /* Reads from the file starting at the specified offset for an amount equal to chunk_size into data/
       Returns false on any type of failure to read.
       librarymixer_id is used to attribute stats to that friend.
       Does NOT check security for that friend, do so before calling this. */
    virtual bool getFileData(uint64_t offset, uint32_t &chunk_size, void *data, unsigned int librarymixer_id);

    /* Closes the file handle to the file. */
    void closeFile();

    /* Return the file's full path. */
    QString getPath() const;

    /* Returns the file hash. */
    QString getHash() const;

    /* Returns the file size. */
    uint64_t getFileSize() const;

    /* Moves the old file to new location and updates internal variables. */
    bool moveFile(QString newPath);

    /* Accessors for a flag on whether this is an internal Mixologist file.
       Generally used to share files without displaying them in the UI. */
    bool isInternalMixologistFile() const {return internalMixologistFile;}
    void setInternalMixologistFile(bool newValue) {internalMixologistFile = newValue;}

    /* Sets the given friend as allowed to request from this file.
       If this is the first person added, changes this from a freely-accessible file to a file limited only to that person. */
    void addPermittedRequestor(unsigned int librarymixer_id);

    /* Disables security limitationsn on this file, and allows all friends to freely-access it. */
    void allowAllRequestors();

    /* True if the given friend is allowed to request from this file.
       If this is not a security limited file, will return true for any ID. */
    bool isPermittedRequestor(unsigned int librarymixer_id);

protected:
    mutable QMutex ftcMutex;

    /* Total file size of the file. */
    uint64_t fullFileSize;
    /* Hash of the file. */
    QString hash;
    /* Path to the file. */
    QString path;
    /* QFile object that we use for all file operations. */
    QFile *file;

    /* True if this is a file that is being shared for the Mixologist's own operations rather than by the user.
       Used by off-LibraryMixer sharing when transfering the XML share information. */
    bool internalMixologistFile;

    /* Friends who have been authorized to read from this file. */
    QList<unsigned int> permittedRequestors;

    /* All friends that have requested this file, keyed by their LibraryMixer ID. */
    struct requestors {
        /* Useful for being able to display how much of the file the friend is up to. */
        uint64_t lastRequestedEnd;

        /* Useful for being able to decide whether to display this file as being actively transferred. */
        time_t lastRequestTime;

        /* These are used for estimating the transfer rate. */
        time_t lastTransferRateCalc;
        uint32_t transferredSinceLastCalc;

        float transferRate;

        /* The amount that has been sent to this friend. */
        uint64_t transferred;
    };
    QHash<unsigned int, struct requestors> requestingFriends;    
};

#endif // FT_FILE_PROVIDER_HEADER
