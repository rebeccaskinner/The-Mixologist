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

#include "ft/ftdata.h"

/******* Pair of Send/Recv (Only need to handle Send side) ******/
ftDataSendPair::ftDataSendPair(ftDataRecv *recv)
    :mDataRecv(recv) {
    return;
}

/* Client Send */
bool    ftDataSendPair::sendDataRequest(std::string peerId, std::string hash,
                                        uint64_t size, uint64_t offset, uint32_t chunksize) {
    return mDataRecv->recvDataRequest(peerId,hash,size,offset,chunksize);
}

/* Server Send */
bool    ftDataSendPair::sendData(std::string peerId,
                                 std::string hash, uint64_t size,
                                 uint64_t offset, uint32_t chunksize, void *data) {
    return mDataRecv->recvData(peerId, hash,size,offset,chunksize,data);
}


/* Client Send */
bool    ftDataSendDummy::sendDataRequest(std::string, std::string,
        uint64_t, uint64_t, uint32_t) {
    return true;
}

/* Server Send */
bool    ftDataSendDummy::sendData(std::string,
                                  std::string, uint64_t,
                                  uint64_t, uint32_t, void *) {
    return true;
}


/* Client Recv */
bool    ftDataRecvDummy::recvData(std::string,
                                  std::string, uint64_t,
                                  uint64_t, uint32_t, void *) {
    return true;
}


/* Server Recv */
bool    ftDataRecvDummy::recvDataRequest(std::string, std::string,
        uint64_t, uint64_t, uint32_t) {
    return true;
}

