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

#ifndef P3_GENERIC_SERVICE_HEADER
#define P3_GENERIC_SERVICE_HEADER

#include "pqi/pqi.h"
#include "pqi/pqiservice.h"
#include <QThread>

/* This provides easy to use extensions to the pqiservice class provided in src/pqi.
 *
 *    Basic service with serialisation handled by a Serialiser.
 *
 */

class p3Service: public pqiService {
protected:

    p3Service(uint16_t type)
        :pqiService((((uint32_t) PKT_VERSION_SERVICE) << 24) + (((uint32_t) type) << 8)),
         serialiser(NULL) {
        serialiser = new Serialiser();
        return;
    }

public:

    virtual ~p3Service() {
        delete serialiser;
        return;
    }

    /*************** INTERFACE ******************************/
    /* called from Thread/tick/GUI */
    //Adds an item to the output queue.
    int             sendItem(NetItem *);
    //Returns the first item off the input queue.
    NetItem         *recvItem();
    //Returns whether or not new data has been received.
    bool        receivedItems();

    virtual int tick() {
        return 0;
    }
    /*************** INTERFACE ******************************/


public:
    // overloaded pqiService interface.
    virtual int     receive(RawItem *);
    virtual RawItem    *send();

protected:
    void    addSerialType(SerialType *);

private:

    mutable QMutex srvMtx;
    /* below locked by Mutex */

    Serialiser *serialiser;
    std::list<NetItem *> recv_queue, send_queue;
};

#endif // P3_GENERIC_SERVICE_HEADER

