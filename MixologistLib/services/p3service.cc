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


#include "pqi/pqi.h"
#include "services/p3service.h"
#include <sstream>
#include <iomanip>
#include <time.h>

/*****
 * #define SERV_DEBUG 1
 ****/

void    p3Service::addSerialType(SerialType *st) {
    serialiser->addSerialType(st);
}

NetItem *p3Service::recvItem() {
    srvMtx.lock();

    if (recv_queue.size() == 0) {
        srvMtx.unlock();
        return NULL; /* nothing there! */
    }

    /* get something off front */
    NetItem *item = recv_queue.front();
    recv_queue.pop_front();

    srvMtx.unlock();
    return item;
}


bool    p3Service::receivedItems() {
    srvMtx.lock();

    bool moreData = (recv_queue.size() != 0);

    srvMtx.unlock();

    return moreData;
}


int p3Service::sendItem(NetItem *item) {
    srvMtx.lock();

    send_queue.push_back(item);

    srvMtx.unlock();

    return 1;
}

// overloaded pqiService interface.
int p3Service::receive(RawItem *raw) {
    srvMtx.lock();

#ifdef SERV_DEBUG
    std::cerr << "p3Service::receive()";
    std::cerr << std::endl;
#endif

    /* convert to ServiceItem */
    uint32_t size = raw->getRawLength();
    NetItem *item = serialiser->deserialise(raw->getRawData(), &size);
    if ((!item) || (size != raw->getRawLength())) {
        /* error in conversion */
#ifdef SERV_DEBUG
        std::cerr << "p3Service::receive() Error" << std::endl;
        std::cerr << "p3Service::receive() Size: " << size << std::endl;
        std::cerr << "p3Service::receive() RawLength: " << raw->getRawLength() << std::endl;
#endif

        if (item) {
#ifdef SERV_DEBUG
            std::cerr << "p3Service::receive() Bad Item:";
            std::cerr << std::endl;
            item->print(std::cerr, 0);
            std::cerr << std::endl;
#endif
            delete item;
        }
    }


    /* if we have something - pass it on */
    if (item) {
#ifdef SERV_DEBUG
        std::cerr << "p3Service::receive() item:";
        std::cerr << std::endl;
        item->print(std::cerr, 0);
        std::cerr << std::endl;
#endif

        item->LibraryMixerId(raw->LibraryMixerId());
        recv_queue.push_back(item);
    }

    /* cleanup input */
    delete raw;

    srvMtx.unlock();

    return (item != NULL);
}

RawItem *p3Service::send() {
    srvMtx.lock();

    if (send_queue.size() == 0) {
        srvMtx.unlock();
        return NULL; /* nothing there! */
    }

    /* get something off front */
    NetItem *si = send_queue.front();
    send_queue.pop_front();

#ifdef SERV_DEBUG
    std::cerr << "p3Service::send() Sending item:";
    std::cerr << std::endl;
    si->print(std::cerr, 0);
    std::cerr << std::endl;
#endif

    /* try to convert */
    uint32_t size = serialiser->size(si);
    if (!size) {
        std::cerr << "p3Service::send() ERROR size == 0";
        std::cerr << std::endl;

        /* can't convert! */
        delete si;
        srvMtx.unlock();
        return NULL;
    }

    RawItem *raw = new RawItem(si->PacketId(), size);
    if (!serialiser->serialise(si, raw->getRawData(), &size)) {
        std::cerr << "p3Service::send() ERROR serialise failed";
        std::cerr << std::endl;

        delete raw;
        raw = NULL;
    }

    if ((raw) && (size != raw->getRawLength())) {
        std::cerr << "p3Service::send() ERROR serialise size mismatch";
        std::cerr << std::endl;

        delete raw;
        raw = NULL;
    }

    raw->LibraryMixerId(si->LibraryMixerId());

    /* cleanup */
    delete si;

    srvMtx.unlock();
    return raw;
}
