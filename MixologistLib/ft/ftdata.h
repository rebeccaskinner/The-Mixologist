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


#ifndef FT_DATA_INTERFACE_HEADER
#define FT_DATA_INTERFACE_HEADER

/*
 * ftData.
 *
 * Internal Interfaces for sending and receiving data.
 * Most likely to be implemented by ftServer.
 * Provided as an independent interface for testing purposes.
 *
 */

#include <string>
#include <inttypes.h>

/*************** SEND INTERFACE *******************/

class ftDataSend {
public:
    virtual ~ftDataSend() {
        return;
    }

    /* Client Send */
    virtual bool    sendDataRequest(std::string peerId, std::string hash,
                                    uint64_t size, uint64_t offset, uint32_t chunksize) = 0;

    /* Server Send */
    virtual bool    sendData(std::string peerId, std::string hash, uint64_t size,
                             uint64_t offset, uint32_t chunksize, void *data) = 0;

};



/*************** RECV INTERFACE *******************/

class ftDataRecv {
public:

    virtual ~ftDataRecv() {
        return;
    }

    /* Client Recv */
    virtual bool    recvData(std::string peerId, std::string hash, uint64_t size,
                             uint64_t offset, uint32_t chunksize, void *data) = 0;

    /* Server Recv */
    virtual bool    recvDataRequest(std::string peerId, std::string hash,
                                    uint64_t size, uint64_t offset, uint32_t chunksize) = 0;


};

/******* Pair of Send/Recv (Only need to handle Send side) ******/
class ftDataSendPair: public ftDataSend {
public:

    ftDataSendPair(ftDataRecv *recv);
    virtual ~ftDataSendPair() {
        return;
    }

    /* Client Send */
    virtual bool    sendDataRequest(std::string peerId, std::string hash,
                                    uint64_t size, uint64_t offset, uint32_t chunksize);

    /* Server Send */
    virtual bool    sendData(std::string peerId, std::string hash, uint64_t size,
                             uint64_t offset, uint32_t chunksize, void *data);

    ftDataRecv *mDataRecv;
};


class ftDataSendDummy: public ftDataSend {
public:
    virtual ~ftDataSendDummy() {
        return;
    }

    /* Client Send */
    virtual bool    sendDataRequest(std::string peerId, std::string hash,
                                    uint64_t size, uint64_t offset, uint32_t chunksize);

    /* Server Send */
    virtual bool    sendData(std::string peerId, std::string hash, uint64_t size,
                             uint64_t offset, uint32_t chunksize, void *data);

};

class ftDataRecvDummy: public ftDataRecv {
public:

    virtual ~ftDataRecvDummy() {
        return;
    }

    /* Client Recv */
    virtual bool    recvData(std::string peerId, std::string hash, uint64_t size,
                             uint64_t offset, uint32_t chunksize, void *data);

    /* Server Recv */
    virtual bool    recvDataRequest(std::string peerId, std::string hash,
                                    uint64_t size, uint64_t offset, uint32_t chunksize);


};

#endif
