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

#ifndef FT_DATA_DEMULTIPLEX_HEADER
#define FT_DATA_DEMULTIPLEX_HEADER

/*
 * ftDataDemultiplexModule.
 *
 * A thread that handles all incoming file-related requests and responses.
 * For incoming requests it arranges for the requested data to be sent.
 * For incoming responses it passes the received data to ftController for saving.
 *
 * As uploads has no central ftController analogue, much more of uploads is handled here than downloads.
 */

class ftController;
class ftFileProvider;
class ftFileCreator;
class ftFileMethod;

#include <string>
#include <list>
#include <map>
#include <inttypes.h>

#include "ft/ftdata.h"
#include "interface/files.h"

#include <QThread>
#include <QMutex>
#include <QMap>

class ftRequest {
public:

    ftRequest(uint32_t type, std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunk, void *data);

    ftRequest()
        :mType(0), mSize(0), mOffset(0), mChunk(0), mData(NULL) {
        return;
    }

    uint32_t mType;
    std::string mPeerId;
    QString mHash;
    uint64_t mSize;
    uint64_t mOffset;
    uint32_t mChunk;
    void *mData;
};

class ftDataDemultiplex: public QThread, public ftDataRecv {
    Q_OBJECT

public:

    ftDataDemultiplex(ftController *controller);

    void addFileMethod(ftFileMethod* fileMethod);

    void run();

    /* data interface */
    /* get Details of File Transfers */
    void FileUploads(QList<uploadFileInfo> &uploads);

    /* Clears out all existing servers. If the same data is requested again, will have to re-search. */
    void clearUploads();

    /*************** RECV INTERFACE (provides ftDataRecv) ****************/

    /* Client receive of a piece of data */
    virtual bool recvData(std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize, void *data);

    /* Server receive of a request for data */
    virtual bool recvDataRequest(std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize);

public slots:
    /* Should be called whenever any of the file providing classes knows that a given file that was previously available
       is no longer available, so that ftDataDemultiplex can clear out any cached information about the file.
       While it is nice to call this when a file is removed to keep things clean, it is important to call this when a file
       is known to be changed on disk, as any further file data sent would be corrupted, so we want to stop ASAP. */
    void fileNoLongerAvailable(QString hash, qulonglong size);

private:
    bool workQueued();
    bool doWork();

    /* Handling Job Queues */
    /* Passes incoming data to the appropriate transfer module, or returns false if this data is for a file we're not downloading. */
    bool handleIncomingData(std::string peerId, QString hash, uint64_t offset, uint32_t chunksize, void *data);

    /* Either responds to the data request by sending the requested data via locked_handleServerRequest,
       or adds it to mSearchQueue for further processing */
    void handleOutgoingDataRequest(std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize);

    /* Uses mFileMethods to find the file specified, and if the file is found, adds it to activeFileServes. */
    bool handleSearchRequest(std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize);

    /* Sends the requested file data */
    bool sendRequestedData(ftFileProvider *provider, std::string peerId, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize);

    /* Moves an ftFileProvider from activeFileServes to deadFileServes. */
    void deactivateFileServe(QString hash, uint64_t filesize);

    mutable QMutex dataMtx;

    /* List of current files being uploaded by file hash. */
    QMap<QString, ftFileProvider *> activeFileServes;
    /* List of files that had previously been uploaded. Kept around so GUI can display information on them until user clears it. */
    QList<ftFileProvider *> deadFileServes;

    /* This queue is filled by the ftserver, and contains both incoming data as well as incoming requests for data. */
    std::list<ftRequest> mRequestQueue;

    /* When there is an incoming request for data, and we aren't able to service it with an existing upload or download,
       this is a queue of searches to be run against our file list. */
    std::list<ftRequest> mSearchQueue;

    /* Interface for sending search requests. */
    QList<ftFileMethod*> mFileMethods;
    ftController *mController;

    /* Threading related variables. */
    uint32_t mLastSleep; /* ms */
    time_t mLastWork;  /* secs */

    friend class ftServer;
};

#endif
