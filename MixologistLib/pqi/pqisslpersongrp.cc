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

#include <sstream>

#include "util/debug.h"

#include "pqi/pqisslpersongrp.h"
#include "pqi/authmgr.h"


const int pqipersongrpzone = 354;

/****
 * #define PQI_DISABLE_UDP 1
 ***/

/********************************** SSL Specific features ***************************/

#include "pqi/pqissl.h"
#include "pqi/pqissllistener.h"

#ifndef PQI_DISABLE_UDP
#include "pqi/pqissludp.h"
#endif

pqilistener *pqisslpersongrp::createListener(struct sockaddr_in laddr) {
    pqilistener *listener = new pqissllistener(laddr);
    return listener;
}

pqiperson *pqisslpersongrp::createPerson(std::string id, unsigned int librarymixer_id, pqilistener *listener) {
    {
        std::ostringstream out;
        out << "pqipersongrp::createPerson() PeerId: " << id;
        pqioutput(PQL_DEBUG_BASIC, pqipersongrpzone, out.str().c_str());
    }

    pqiperson *pqip = new pqiperson(id, librarymixer_id, this);
    pqissl *pqis   = new pqissl((pqissllistener *) listener, pqip);

    /* construct the serialiser ....
     * Needs:
     * * FileItem
     * * FileData
     * * ServiceGeneric
     */

    Serialiser *rss = new Serialiser();
    rss->addSerialType(new FileItemSerialiser());
    rss->addSerialType(new ServiceSerialiser());

    pqiconnect *pqisc = new pqiconnect(rss, pqis);

    pqip -> addChildInterface(PQI_CONNECT_TCP, pqisc);

#ifndef PQI_DISABLE_UDP
    pqissludp *pqius    = new pqissludp(pqip);

    Serialiser *rss2 = new Serialiser();
    rss2->addSerialType(new FileItemSerialiser());
    rss2->addSerialType(new ServiceSerialiser());

    pqiconnect *pqiusc  = new pqiconnect(rss2, pqius);

    // add a ssl + proxy interface.
    // Add Proxy First.
    pqip -> addChildInterface(PQI_CONNECT_UDP, pqiusc);
#endif

    return pqip;
}


/********************************** SSL Specific features ***************************/


