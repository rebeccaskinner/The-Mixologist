/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-2008, Robert Fernie
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

#ifndef OPEN_DHT_MGR_H
#define OPEN_DHT_MGR_H

#include "pqi/p3dhtmgr.h"
#include "dht/dhtclient.h"

#include <string>
#include <inttypes.h>
#include <list>
#include <QString>

class OpenDHTMgr: public p3DhtMgr {
public:

    OpenDHTMgr(std::string ownId, pqiConnectCb *cb, QString configdir);

protected:

    /********** OVERLOADED FROM p3DhtMgr ***************/
    virtual bool    dhtInit();
    virtual bool    dhtShutdown();
    virtual bool    dhtActive();
    virtual int     status(std::ostream &out);

    /* Blocking calls (only from thread) */
    virtual bool publishDHT(std::string key, std::string value, uint32_t ttl);
    virtual bool searchDHT(std::string key);

    /********** OVERLOADED FROM p3DhtMgr ***************/

private:

    DHTClient *mClient;
    QString mConfigDir;
};


#endif


