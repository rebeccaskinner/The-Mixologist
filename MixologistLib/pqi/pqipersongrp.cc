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
#include "pqi/p3connmgr.h"
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
pqipersongrp::pqipersongrp(SecurityPolicy *glob, unsigned long flags)
    :pqihandler(glob), pqil(NULL), initFlags(flags) {
}


int pqipersongrp::tick() {
    /* could limit the ticking of listener / tunnels to 1/sec...
     * but not too important.
     */

    {
        MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
        if (pqil) {
            pqil -> tick();
        }
    } /* UNLOCKED */

    int i = 0;

    if (tickServiceSend()) {
        i = 1;
#ifdef PGRP_DEBUG
        std::cerr << "pqipersongrp::tick() moreToTick from tickServiceSend()" << std::endl;
#endif
    }

    if (pqihandler::tick()) { /* does actual Send/Recv */
        i = 1;
#ifdef PGRP_DEBUG
        std::cerr << "pqipersongrp::tick() moreToTick from pqihandler::tick()" << std::endl;
#endif
    }


    if (tickServiceRecv()) {
        i = 1;
#ifdef PGRP_DEBUG
        std::cerr << "pqipersongrp::tick() moreToTick from tickServiceRecv()" << std::endl;
#endif
    }

    return i;
}

int pqipersongrp::status() {
    {
        MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
        if (pqil) {
            pqil -> status();
        }
    } /* UNLOCKED */

    return pqihandler::status();
}


/* Initialise pqilistener */
int pqipersongrp::init_listener() {
    /* extract our information from the p3ConnectMgr */
    if (initFlags & PQIPERSON_NO_LISTENER) {
        pqil = NULL;
    } else {
        /* extract details from
         */
        peerConnectState state;
        mConnMgr->getPeerConnectState(getAuthMgr()->OwnLibraryMixerId(), state);

        MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
        pqil = createListener(state.localaddr);
    }
    return 1;
}

int     pqipersongrp::restart_listener() {
    // stop it,
    // change the address.
    // restart.
    bool haveListener = false;
    {
        MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
        haveListener = (pqil != NULL);
    } /* UNLOCKED */


    if (haveListener) {
        peerConnectState state;
        mConnMgr->getPeerConnectState(getAuthMgr()->OwnLibraryMixerId(), state);

        MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

        pqil -> resetlisten();
        pqil -> setListenAddr(state.localaddr);
        pqil -> setuplisten();
    }
    return 1;
}

void    pqipersongrp::load_transfer_rates() {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    setMaxRate(true, settings.value("Transfers/MaxTotalDownloadRate", DEFAULT_MAX_TOTAL_DOWNLOAD).toInt());
    setMaxRate(false, settings.value("Transfers/MaxTotalDownloadRate", DEFAULT_MAX_TOTAL_UPLOAD).toInt());
    setMaxIndivRate(true, settings.value("Transfers/MaxIndividualDownloadRate", DEFAULT_MAX_INDIVIDUAL_DOWNLOAD).toInt());
    setMaxIndivRate(false, settings.value("Transfers/MaxIndividualUploadRate", DEFAULT_MAX_INDIVIDUAL_UPLOAD).toInt());
}


void    pqipersongrp::statusChange(const std::list<pqipeer> &plist) {
    /* iterate through, only worry about the friends */
    std::list<pqipeer>::const_iterator it;
    for (it = plist.begin(); it != plist.end(); it++) {
        if (it->actions & PEER_NEW) addPeer(it->cert_id, it->librarymixer_id);
        if (it->actions & PEER_CONNECT_REQ) connectPeer(it->cert_id, it->librarymixer_id);
        if (it->actions & PEER_TIMEOUT) timeoutPeer(it->cert_id);
    }
}

int     pqipersongrp::addPeer(std::string id, int librarymixer_id) {
    {
        std::ostringstream out;
        out << "pqipersongrp::addPeer() cert_id: " << id;
        pqioutput(PQL_DEBUG_BASIC, pqipersongrpzone, out.str().c_str());
    }

    SearchModule *sm = NULL;
    {
        MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
        std::map<std::string, SearchModule *>::iterator it;
        it = mods.find(id);
        if (it != mods.end()) {
            pqioutput(PQL_DEBUG_BASIC, pqipersongrpzone,
                      "pqipersongrp::addPeer() Peer already in Use!");
            return -1;
        }

        pqiperson *pqip = createPerson(id, librarymixer_id, pqil);

        // attach to pqihandler
        sm = new SearchModule();
        sm -> cert_id = id;
        sm -> pqi = pqip;
        sm -> sp = secpolicy_create();

        // reset it to start it working.
        pqip -> reset();
        pqip -> listen();
    } /* UNLOCKED */

    return AddSearchModule(sm);
}

#ifdef false
int     pqipersongrp::removePeer(std::string id) {
    std::map<std::string, SearchModule *>::iterator it;

    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

    it = mods.find(id);
    if (it != mods.end()) {
        SearchModule *mod = it->second;
        // Don't duplicate remove!!!
        //RemoveSearchModule(mod);
        secpolicy_delete(mod -> sp);
        pqiperson *p = (pqiperson *) mod -> pqi;
        p -> reset();
        delete p;
        mods.erase(it);
    }
    return 1;
}
#endif

int     pqipersongrp::connectPeer(std::string cert_id, int librarymixer_id) {
    /* get status from p3connectMgr */
#ifdef PGRP_DEBUG
    std::cerr << " pqipersongrp::connectPeer() id: " << id  << std::endl;
#endif

    {
        MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
        std::map<std::string, SearchModule *>::iterator it;
        it = mods.find(cert_id);
        if (it == mods.end()) return 0;

        /* get the connect attempt details from the p3connmgr... */
        SearchModule *mod = it->second;
        pqiperson *p = (pqiperson *) mod -> pqi;

        /* get address from p3connmgr */
        if (!mConnMgr) return 0;

        struct sockaddr_in addr;
        uint32_t delay, period, type;

        if (!mConnMgr->connectAttempt(librarymixer_id, addr, delay, period, type)) {
#ifdef PGRP_DEBUG
            std::cerr << " pqipersongrp::connectPeer() No Net Address";
            std::cerr << std::endl;
#endif
            return 0;
        }

#ifdef PGRP_DEBUG
        std::cerr << " pqipersongrp::connectPeer() connectAttempt data id: " << id;
        std::cerr << " addr: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port);
        std::cerr << " delay: " << delay;
        std::cerr << " period: " << period;
        std::cerr << " type: " << type;
        std::cerr << std::endl;
#endif


        uint32_t ptype, timeout;
        if (type & NET_CONN_TCP_ALL) {
            ptype = PQI_CONNECT_TCP;
            timeout = TCP_STD_TIMEOUT_PERIOD;
#ifdef PGRP_DEBUG
            std::cerr << " pqipersongrp::connectPeer() connecting with TCP: Timeout :" << timeout;
            std::cerr << std::endl;
#endif
        } else if (type & NET_CONN_UDP_ALL) {
            ptype = PQI_CONNECT_UDP;
            timeout = period * 2;
#ifdef PGRP_DEBUG
            std::cerr << " pqipersongrp::connectPeer() connecting with UDP: Timeout :" << timeout;
            std::cerr << std::endl;
#endif
        } else
            return 0;

        p->connect(ptype, addr, delay, period, timeout);

    } /* UNLOCKED */
    return 1;
}

void    pqipersongrp::timeoutPeer(std::string cert_id) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
    std::map<std::string, SearchModule *>::iterator it;
    it = mods.find(cert_id);
    if (it == mods.end()) return;

    /* get the connect attempt details from the p3connmgr... */
    SearchModule *mod = it->second;
    pqiperson *p = (pqiperson *) mod -> pqi;

    p->reset();
}

bool    pqipersongrp::notifyConnect(std::string id, uint32_t ptype, bool success) {
    uint32_t type = 0;
    if (ptype == PQI_CONNECT_TCP) {
        type = NET_CONN_TCP_ALL;
    } else {
        type = NET_CONN_UDP_ALL;
    }

    if (mConnMgr) mConnMgr->connectResult(getAuthMgr()->findLibraryMixerByCertId(id), success, type);

    return (NULL != mConnMgr);
}
