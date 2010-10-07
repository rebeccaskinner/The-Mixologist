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
#include "pqi/pqiloopback.h"

/***
#define LOOPBACK_DEBUG 1
***/

pqiloopback::pqiloopback(std::string id, int librarymixer_id)
    :PQInterface(id, librarymixer_id) {
    setMaxRate(true, 0);
    setMaxRate(false, 0);
    setRate(true, 0);
    setRate(false, 0);

    return;
}

pqiloopback::~pqiloopback() {
    return;
}

int pqiloopback::SendItem(NetItem *i) {

#ifdef  LOOPBACK_DEBUG
    std::cerr << "pqiloopback::SendItem()";
    std::cerr << std::endl;
    i->print(std::cerr);
    std::cerr << std::endl;
#endif
    objs.push_back(i);
    return 1;
}

NetItem   *pqiloopback::GetItem() {
    if (objs.size() > 0) {
        NetItem *pqi = objs.front();
        objs.pop_front();
#ifdef  LOOPBACK_DEBUG
        std::cerr << "pqiloopback::GetItem()";
        std::cerr << std::endl;
        pqi->print(std::cerr);
        std::cerr << std::endl;
#endif
        return pqi;
    }
    return NULL;
}

// PQI interface.
int     pqiloopback::tick() {
    return 0;
}

int     pqiloopback::status() {
    return 0;
}


