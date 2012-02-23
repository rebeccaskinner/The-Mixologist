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

#ifndef MRK_PQI_LOOPBACK_HEADER
#define MRK_PQI_LOOPBACK_HEADER

// The standard data types and the search interface.
#include "pqi/pqi.h"

#include <map>
#include <list>
#include <iostream>

class pqiloopback: public PQInterface {
public:
    pqiloopback(std::string id, unsigned int librarymixer_id);
    virtual ~pqiloopback();

    // search Interface.
    virtual int SendItem(NetItem *item);
    virtual NetItem *GetItem();

    // PQI interface.
    virtual int tick();

    virtual int     notifyEvent(NetInterface *ni, int event) {
        (void) ni;    /* Not used */
        (void) event;
        return 0;
    }
private:
    std::list<NetItem *> objs;
};

#endif //MRK_PQI_LOOPBACK_HEADER
