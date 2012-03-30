/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-8, Robert Fernie
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

#ifndef PQI_SERVICE_HEADER
#define PQI_SERVICE_HEADER

#include "pqi/pqi_base.h"
#include <QMutex>

// PQI Service, is a generic lower layer on which services can run on.
//
// these packets get passed through the
// server, to a service that is registered.
//
// example services:
//  proxytunnel. -> p3proxy.
//  sockettunnel
//      -> either broadcast (attach to
//              open socket instead
//              of broadcast address)
//      -> or requested/signon.
//
//  games,
//  voice
//  video
//
//
// DataType is defined in the serialiser directory.

class RawItem;

class pqiService {
protected:

    pqiService(uint32_t t) // our type of packets.
        :type(t) {
        return;
    }

    virtual ~pqiService() {
        return;
    }

public:
    //
    virtual int     receive(RawItem *) = 0;
    virtual RawItem    *send() = 0;

    uint32_t getType() {
        return type;
    }

    virtual int tick() {
        return 0;
    }

private:
    uint32_t type;
};

#include <map>


class p3ServiceServer {
public:
    p3ServiceServer();

    int addService(pqiService *);

    int incoming(RawItem *);
    RawItem *outgoing();

    /* This tick is called from AggregatedConnectionsToFriends. */
    int tick();

private:

    mutable QMutex srvMtx;
    std::map<uint32_t, pqiService *> services;
    std::map<uint32_t, pqiService *>::iterator rrit;

};


#endif // PQI_SERVICE_HEADER
