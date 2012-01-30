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

#include "pqi/pqiservice.h"
#include "util/debug.h"
#include <sstream>

const int pqiservicezone = 60478;

/****
 * #define SERVICE_DEBUG 1
 ****/

p3ServiceServer::p3ServiceServer() {
    QMutexLocker stack(&srvMtx);

#ifdef  SERVICE_DEBUG
    pqioutput(PQL_DEBUG_BASIC, pqiservicezone,
              "p3ServiceServer::p3ServiceServer()");
#endif

    rrit = services.begin();
    return;
}

int p3ServiceServer::addService(pqiService *ts) {
    QMutexLocker stack(&srvMtx);

#ifdef  SERVICE_DEBUG
    pqioutput(PQL_DEBUG_BASIC, pqiservicezone,
              "p3ServiceServer::addService()");
#endif

    std::map<uint32_t, pqiService *>::iterator it;
    it = services.find(ts -> getType());
    if (it != services.end()) {
        // it exists already!
        return -1;
    }

    services[ts -> getType()] = ts;
    rrit = services.begin();
    return 1;
}

int p3ServiceServer::incoming(RawItem *item) {
    QMutexLocker stack(&srvMtx);

#ifdef  SERVICE_DEBUG
    pqioutput(PQL_DEBUG_BASIC, pqiservicezone,
              "p3ServiceServer::incoming()");

    {
        std::ostringstream out;
        out << "p3ServiceServer::incoming() PacketId: ";
        out << std::hex << item -> PacketId() << std::endl;
        out << "Looking for Service: ";
        out << (item -> PacketId() & 0xffffff00) << std::dec << std::endl;

        out << "Item:" << std::endl;
        item -> print(out);
        out << std::endl;
        pqioutput(PQL_DEBUG_BASIC, pqiservicezone, out.str());
    }
#endif

    std::map<uint32_t, pqiService *>::iterator it;
    it = services.find(item -> PacketId() & 0xffffff00);
    if (it == services.end()) {
#ifdef  SERVICE_DEBUG
        pqioutput(PQL_DEBUG_BASIC, pqiservicezone,
                  "p3ServiceServer::incoming() Service: No Service - deleting");
#endif

        // delete it.
        delete item;

        // it exists already!
        return -1;
    }

    {
#ifdef  SERVICE_DEBUG
        std::ostringstream out;
        out << "p3ServiceServer::incoming() Sending to";
        out << it -> second << std::endl;
        pqioutput(PQL_DEBUG_BASIC, pqiservicezone, out.str());
#endif

        return (it->second) -> receive(item);
    }

    delete item;
    return -1;
}



RawItem *p3ServiceServer::outgoing() {
    QMutexLocker stack(&srvMtx);

#ifdef  SERVICE_DEBUG
    pqioutput(PQL_DEBUG_ALL, pqiservicezone,
              "p3ServiceServer::outgoing()");
#endif

    if (rrit != services.end()) {
        rrit++;
    } else {
        rrit = services.begin();
    }

    std::map<uint32_t, pqiService *>::iterator sit = rrit;
    // run to the end.
    RawItem *item;

    // run through to the end,
    for (; rrit != services.end(); rrit++) {
        if (NULL != (item = (rrit -> second) -> send())) {

#ifdef  SERVICE_DEBUG
            std::ostringstream out;
            out << "p3ServiceServer::outgoing() Got Item From:";
            out << rrit -> second << std::endl;

            item -> print(out);
            out << std::endl;
            pqioutput(PQL_DEBUG_BASIC, pqiservicezone, out.str());
#endif

            return item;
        }
    }

    // from the beginning to where we started.
    for (rrit = services.begin(); rrit != sit; rrit++) {
        if (NULL != (item = (rrit -> second) -> send())) {

#ifdef  SERVICE_DEBUG
            std::ostringstream out;
            out << "p3ServiceServer::outgoing() Got Item From:";
            out << rrit -> second << std::endl;

            item -> print(out);
            out << std::endl;
            pqioutput(PQL_DEBUG_BASIC, pqiservicezone, out.str());
#endif

            return item;
        }
    }
    return NULL;
}



int p3ServiceServer::tick() {

    QMutexLocker stack(&srvMtx);

#ifdef  SERVICE_DEBUG
    pqioutput(PQL_DEBUG_ALL, pqiservicezone,
              "p3ServiceServer::tick()");
#endif

    std::map<uint32_t, pqiService *>::iterator it;

    // from the beginning to where we started.
    for (it = services.begin(); it != services.end(); it++) {

#ifdef  SERVICE_DEBUG
        std::ostringstream out;
        out << "p3ServiceServer::service id:" << it -> first;
        out << " -> Service: " << it -> second;
        out << std::endl;
        pqioutput(PQL_DEBUG_ALL, pqiservicezone, out.str());
#endif

        // now we should actually tick the service.
        (it -> second) -> tick();
    }
    return 1;
}



