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

#include "pqi/pqipersongrp.h"
#include "pqi/connectivitymanager.h"
#include "util/debug.h"
#include "interface/settings.h"
#include "interface/peers.h"
#include <QSettings>

#include <sstream>
#include <stdio.h>

const int pqipersongrpzone = 354;



/****
 *#define PGRP_DEBUG 1
 ****/

/* MUTEX NOTES:
 * Functions like GetRawItem() lock itself (pqihandler) and
 * likewise ServiceServer mutex themselves.
 * This means the only things we need to worry about are:
 *  pqilistener and when accessing pqihandlers data.
 */

// handle the tunnel services.
int pqipersongrp::tickServiceRecv() {
    RawItem *pqi = NULL;
    int i = 0;
    pqioutput(PQL_DEBUG_ALL, pqipersongrpzone, "pqipersongrp::tickTunnelServer()");

    //p3ServiceServer::tick();

    while (NULL != (pqi = GetRawItem())) {
        ++i;
        pqioutput(PQL_DEBUG_BASIC, pqipersongrpzone,
                  "pqipersongrp::tickTunnelServer() Incoming TunnelItem");
        incoming(pqi);
    }

    if (0 < i) {
        return 1;
    }
    return 0;
}

// handle the tunnel services.
int pqipersongrp::tickServiceSend() {
    RawItem *pqi = NULL;
    int i = 0;
    pqioutput(PQL_DEBUG_ALL, pqipersongrpzone, "pqipersongrp::tickServiceSend()");

    p3ServiceServer::tick();

    while (NULL != (pqi = outgoing())) { /* outgoing has own locking */
        ++i;
        pqioutput(PQL_DEBUG_BASIC, pqipersongrpzone,
                  "pqipersongrp::tickTunnelServer() OutGoing NetItem");

        SendRawItem(pqi); /* Locked by pqihandler */
    }
    if (0 < i) {
        return 1;
    }
    return 0;
}


// init
pqipersongrp::pqipersongrp(unsigned long flags)
    :pqil(NULL), initFlags(flags) {
}


int pqipersongrp::tick() {
    /* could limit the ticking of listener / tunnels to 1/sec...
     * but not too important.
     */

    {
        QMutexLocker stack(&coreMtx);
        if (pqil) pqil -> tick();
    }

    int i = 0;

    if (tickServiceSend()) i = 1;

    /* does actual Send/Recv */
    if (pqihandler::tick()) i = 1;

    if (tickServiceRecv()) i = 1;

    return i;
}

/* Initialise pqilistener */
int pqipersongrp::init_listener() {
    /* extract our information from the ConnectivityManager */
    if (initFlags & PQIPERSON_NO_LISTENER) {
        pqil = NULL;
    } else {
        /* extract details from
         */
        peerConnectState state;
        connMgr->getPeerConnectState(authMgr->OwnLibraryMixerId(), state);

        QMutexLocker stack(&coreMtx);
        pqil = createListener(state.localaddr);
    }
    return 1;
}

int pqipersongrp::restart_listener() {
    // stop it,
    // change the address.
    // restart.
    bool haveListener = false;
    {
        QMutexLocker stack(&coreMtx);
        haveListener = (pqil != NULL);
    } /* UNLOCKED */


    if (haveListener) {
        peerConnectState state;
        connMgr->getPeerConnectState(authMgr->OwnLibraryMixerId(), state);

        QMutexLocker stack(&coreMtx);

        pqil -> resetlisten();
        pqil -> setListenAddr(state.localaddr);
        pqil -> setuplisten();
    }
    return 1;
}

void pqipersongrp::load_transfer_rates() {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    setMaxRate(true, settings.value("Transfers/MaxTotalDownloadRate", DEFAULT_MAX_TOTAL_DOWNLOAD).toInt());
    setMaxRate(false, settings.value("Transfers/MaxTotalDownloadRate", DEFAULT_MAX_TOTAL_UPLOAD).toInt());
    setMaxIndivRate(true, settings.value("Transfers/MaxIndividualDownloadRate", DEFAULT_MAX_INDIVIDUAL_DOWNLOAD).toInt());
    setMaxIndivRate(false, settings.value("Transfers/MaxIndividualUploadRate", DEFAULT_MAX_INDIVIDUAL_UPLOAD).toInt());
}


void pqipersongrp::statusChange(const std::list<pqipeer> &plist) {
    /* iterate through, only worry about the friends */
    std::list<pqipeer>::const_iterator it;
    for (it = plist.begin(); it != plist.end(); it++) {
        if (it->actions & PEER_NEW) addPeer(it->cert_id, it->librarymixer_id);
        if (it->actions & PEER_CONNECT_REQ) connectPeer(it->cert_id, it->librarymixer_id);
        if (it->actions & PEER_TIMEOUT) timeoutPeer(it->cert_id);
    }
}

int pqipersongrp::addPeer(std::string id, unsigned int librarymixer_id) {
    {
        std::ostringstream out;
        out << "pqipersongrp::addPeer() cert_id: " << id;
        pqioutput(PQL_DEBUG_BASIC, pqipersongrpzone, out.str().c_str());
    }

    pqiperson *pqip;
    {
        QMutexLocker stack(&coreMtx);
        std::map<std::string, PQInterface *>::iterator it;
        it = pqis.find(id);
        if (it != pqis.end()) {
            pqioutput(LOG_DEBUG_ALERT, pqipersongrpzone,
                      "pqipersongrp::addPeer() Peer already in Use!");
            return -1;
        }

        pqip = createPerson(id, librarymixer_id, pqil);

        // reset it to start it working.
        pqip -> reset();
        pqip -> listen();
    }
    return AddPQI(pqip);
}

#ifdef false
int pqipersongrp::removePeer(std::string id) {
    std::map<std::string, PQInterface *>::iterator it;

    QMutexLocker stack(&coreMtx);

    it = pqis.find(id);
    if (it != pqis.end()) {
        // Don't duplicate remove!!!
        //RemovePQInterface(p);
        pqiperson *p = (pqiperson *) it->second;
        p -> reset();
        delete p;
        pqis.erase(it);
    }
    return 1;
}
#endif

int pqipersongrp::connectPeer(std::string cert_id, unsigned int librarymixer_id) {
    /* get status from ConnectivityManager */

    {
        QMutexLocker stack(&coreMtx);
        std::map<std::string, PQInterface *>::iterator it;
        it = pqis.find(cert_id);
        if (it == pqis.end()) return 0;

        /* get the connect attempt details from the ConnectivityManager. */
        pqiperson *p = (pqiperson *) it->second;

        /* get address from ConnectivityManager */
        if (!connMgr) return 0;

        struct sockaddr_in addr;
        uint32_t delay, period, type;

        if (!connMgr->getQueuedConnectAttempt(librarymixer_id, addr, delay, period, type)) {
            return 0;
        }

        uint32_t ptype, timeout;
        if (type & NET_CONN_TCP_ALL) {
            ptype = PQI_CONNECT_TCP;
            timeout = TCP_STD_TIMEOUT_PERIOD;
        } else if (type & NET_CONN_UDP_ALL) {
            ptype = PQI_CONNECT_UDP;
            timeout = period * 2;
        } else
            return 0;

        p->connect(ptype, addr, delay, period, timeout);

    } /* UNLOCKED */
    return 1;
}

void pqipersongrp::timeoutPeer(std::string cert_id) {
    QMutexLocker stack(&coreMtx);
    std::map<std::string, PQInterface *>::iterator it;
    it = pqis.find(cert_id);
    if (it == pqis.end()) return;

    /* get the connect attempt details from the ConnectivityManager */
    pqiperson *p = (pqiperson *) it->second;

    p->reset();
}

bool    pqipersongrp::notifyConnect(std::string id, uint32_t ptype, bool success) {
    uint32_t type = 0;
    if (ptype == PQI_CONNECT_TCP) {
        type = NET_CONN_TCP_ALL;
    } else {
        type = NET_CONN_UDP_ALL;
    }

    if (connMgr) connMgr->reportConnectionUpdate(authMgr->findLibraryMixerByCertId(id), success, type);

    return (NULL != connMgr);
}
