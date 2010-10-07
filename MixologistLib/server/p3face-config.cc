/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-6, Robert Fernie
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

#include "server/server.h"
#include "pqi/pqipersongrp.h"
#include "pqi/authmgr.h"
#include "pqi/p3connmgr.h"

#include <iostream>
#include <sstream>

#include "util/debug.h"
const int p3facemsgzone = 11453;

#include <sys/time.h>
#include <time.h>


/****************************************
 * This file contains the Server's network configuration options.
 * This is mostly unused at the moment, but could be useful should we decide to query these things.
 ****************************************/

void    Server::ReloadTransferRates() {
    pqih->load_transfer_rates();
}

int Server::UpdateAllConfig() {
    /* fill the interface class */
    Iface &iface = getIface();

    /* lock Mutexes */
    lockCore();     /* LOCK */
    iface.lockData(); /* LOCK */

    NetConfig &config = iface.mConfig;

    /* ports */
    peerConnectState pstate;
    mConnMgr->getPeerConnectState(mAuthMgr->OwnLibraryMixerId(), pstate);
    config.localAddr = inet_ntoa(pstate.localaddr.sin_addr);
    config.localPort = ntohs(pstate.localaddr.sin_port);

    config.extAddr = inet_ntoa(pstate.serveraddr.sin_addr);
    config.extPort = ntohs(pstate.serveraddr.sin_port);

    /* update network configuration */

    config.netOk =   mConnMgr->getNetStatusOk();
    config.netUpnpOk = mConnMgr->getNetStatusUpnpOk();
    config.netDhtOk = mConnMgr->getNetStatusDhtOk();
    config.netExtOk = mConnMgr->getNetStatusExtOk();
    config.netUdpOk = mConnMgr->getNetStatusUdpOk();
    config.netTcpOk = mConnMgr->getNetStatusTcpOk();

    /* update DHT/UPnP config */

    config.uPnPState  = mConnMgr->getUPnPState();
    config.uPnPActive = mConnMgr->getUPnPEnabled();
    config.DHTActive  = mConnMgr->getDHTEnabled();

    /* unlock Mutexes */
    iface.unlockData(); /* UNLOCK */
    unlockCore();     /* UNLOCK */

    return 1;


}

