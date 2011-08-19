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
 * This multiplexes the data from PQInterface to the ftTransferModules.
 */

class ftTransferModule;
class ftFileProvider;
class ftFileCreator;
class ftSearch;

#include <string>
#include <list>
#include <map>
#include <inttypes.h>

#include "util/threads.h"

#include "ft/ftdata.h"
#include "interface/files.h"

#include <QMap>

//Internal storage class used by ftDataDemultiplex to store a file that is being downloaded
class ftClient {
public:

    ftClient() :mModule(NULL), mCreator(NULL) {}
    ftClient(ftTransferModule *module, ftFileCreator *creator)
        :mModule(module), mCreator(creator) {}

    ftTransferModule *mModule;
    ftFileCreator    *mCreator;
};

class ftRequest {
public:

    ftRequest(uint32_t type, std::string peerId, std::string hash, uint64_t size, uint64_t offset, uint32_t chunk, void *data);

    ftRequest()
        :mType(0), mSize(0), mOffset(0), mChunk(0), mData(NULL) {
        return;
    }

    uint32_t mType;
    std::string mPeerId;
    std::string mHash;
    uint64_t mSize;
    uint64_t mOffset;
    uint32_t mChunk;
    void *mData;
};



class ftDataDemultiplex: public ftDataRecv, public MixQueueThread {

public:

    ftDataDemultiplex(std::string ownId, ftSearch *search);

    /* ftController Interface */
    //Adds a new download
    bool    addTransferModule(ftTransferModule *mod, ftFileCreator *f);
    //Removes a download
    bool    removeTransferModule(std::string hash);

    /* data interface */
    /* get Details of File Transfers */
    bool    FileUploads(QList<FileInfo> &uploads);
    /* No mutex protected */
    bool    FileDetails(std::string hash, FileInfo &info);

    void    clearUploads();

    /*************** RECV INTERFACE (provides ftDataRecv) ****************/

    /* Client receive of a piece of data */
    virtual bool    recvData(std::string peerId, std::string hash, uint64_t size, uint64_t offset, uint32_t chunksize, void *data);

    /* Server receive of a request for data */
    virtual bool    recvDataRequest(std::string peerId, std::string hash, uint64_t size, uint64_t offset, uint32_t chunksize);


protected:

    /* Overloaded from MixQueueThread, the thread loop */
    virtual bool workQueued();
    virtual bool doWork();

private:

    /* Handling Job Queues */
    //Passes incoming data to the appropriate transfer module, or returns false if this data is for a file we're not downloading
    bool    handleRecvData(std::string peerId, std::string hash, uint64_t size,
                           uint64_t offset, uint32_t chunksize, void *data);
    /* Either responds to the data request by sending the requested data via locked_handleServerRequest,
       or adds it to mSearchQueue for further processing */
    void    handleRecvDataRequest(std::string peerId,
                                  std::string hash, uint64_t size,
                                  uint64_t offset, uint32_t chunksize);

    //Calls mSearch to find the file specified, and if the file is found, adds it to mServers
    bool    handleSearchRequest(std::string peerId, std::string hash, uint64_t size,
                                uint64_t offset, uint32_t chunksize);

    /* Sends the requested file data */
    bool    locked_handleServerRequest(ftFileProvider *provider,
                                       std::string peerId, std::string hash, uint64_t size,
                                       uint64_t offset, uint32_t chunksize);

    MixMutex dataMtx;

    //List of current files being downloaded
    //The string is the hash of the file, and ftClient is a holder class for information
    QMap<std::string, ftClient> mClients;
    //List of current files being uploaded
    QMap<std::string, ftFileProvider *> mServers;

    //This queue is filled by the ftserver, and contains both incoming data as well as incoming requests for data
    std::list<ftRequest> mRequestQueue;
    /* When there is an incoming request for data, and we aren't able to service it with an existing upload or download,
       this is a queue of searches to be run against our file list */
    std::list<ftRequest> mSearchQueue;

    //Interface for sending search requests
    ftSearch   *mSearch;

    std::string mOwnId;

    friend class ftServer;
};

#endif
