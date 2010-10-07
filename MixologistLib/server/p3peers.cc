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


#include "server/p3peers.h"
#include "server/server.h"
#include "pqi/p3connmgr.h"
#include "pqi/authmgr.h"
#include <interface/init.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include "pqi/authmgr.h"


Peers *peers = NULL;

/*******
 * #define P3PEERS_DEBUG 1
 *******/

p3Peers::p3Peers(p3ConnectMgr *cm, AuthMgr *am)
    :mConnMgr(cm), mAuthMgr(am) {
    return;
}

/* Peer Details (Net & Auth) */
std::string p3Peers::getOwnCertId() {
    return mAuthMgr->OwnCertId();
}

int p3Peers::getOwnLibraryMixerId() {
    return mAuthMgr->OwnLibraryMixerId();
}

QString p3Peers::getOwnName(){
    return mConnMgr->getOwnName();
}


bool    p3Peers::getOnlineList(std::list<int> &ids) {
    mConnMgr->getOnlineList(ids);
    return true;
}

bool    p3Peers::getSignedUpList(std::list<int> &ids) {
    mConnMgr->getSignedUpList(ids);
    return true;
}

bool    p3Peers::getFriendList(std::list<int> &ids) {
    mConnMgr->getFriendList(ids);
    return true;
}

bool    p3Peers::isOnline(int librarymixer_id) {
    return mConnMgr->isOnline(librarymixer_id);
}

bool p3Peers::isFriend(int librarymixer_id) {
    return mConnMgr->isFriend(librarymixer_id);
}

bool    p3Peers::getPeerDetails(int librarymixer_id, PeerDetails &d) {
    peerConnectState pcs;
    if (!mConnMgr->getPeerConnectState(librarymixer_id, pcs)) return false;
    d.id = pcs.id;
    d.librarymixer_id = pcs.librarymixer_id;
    d.name = pcs.name;

    /* fill from pcs */

    d.localAddr = inet_ntoa(pcs.localaddr.sin_addr);
    d.localPort = ntohs(pcs.localaddr.sin_port);
    d.extAddr   = inet_ntoa(pcs.serveraddr.sin_addr);
    d.extPort   = ntohs(pcs.serveraddr.sin_port);
    d.lastConnect   = pcs.lastcontact;
    d.connectPeriod = 0;

    /* Translate */

    if (pcs.state & PEER_S_CONNECTED)
        d.state = PEER_STATE_CONNECTED;
    else if (pcs.schedulednexttry != 0)
        d.state = PEER_STATE_WAITING_FOR_RETRY;
    else if (pcs.inConnAttempt)
        d.state = PEER_STATE_TRYING;
    else if (pcs.state & PEER_S_NO_CERT)
        d.state = PEER_STATE_NO_CERT;
    else d.state = PEER_STATE_OFFLINE;

    switch (pcs.netMode & NET_MODE_ACTUAL) {
        case NET_MODE_EXT:
            d.netMode   = NETMODE_EXT;
            break;
        case NET_MODE_UPNP:
            d.netMode   = NETMODE_UPNP;
            break;
        case NET_MODE_UDP:
            d.netMode   = NETMODE_UDP;
            break;
        case NET_MODE_UNREACHABLE:
        case NET_MODE_UNKNOWN:
        default:
            d.netMode   = NETMODE_UNREACHABLE;
            break;
    }

    if (pcs.netMode & NET_MODE_TRY_EXT) {
        d.tryNetMode    = NETMODE_EXT;
    } else if (pcs.netMode & NET_MODE_TRY_UPNP) {
        d.tryNetMode    = NETMODE_UPNP;
    } else {
        d.tryNetMode    = NETMODE_UDP;
    }

    d.visState  = 0;
    /*  if (!(pcs.visState & VIS_STATE_NODISC)) {
                    d.visState |= VS_DISC_ON;
        }*/

    if (!(pcs.visState & VIS_STATE_NODHT)) {
        d.visState |= VS_DHT_ON;
    }

    return true;
}

std::string p3Peers::findCertByLibraryMixerId(int librarymixer_id) {
    return mAuthMgr->findCertByLibraryMixerId(librarymixer_id);
}

int     p3Peers::findLibraryMixerByCertId(std::string cert_id) {
    return mAuthMgr->findLibraryMixerByCertId(cert_id);
}

QString p3Peers::getPeerName(int librarymixer_id) {
    /* get from mAuthMgr as it should have more peers? */
    return mConnMgr->getFriendName(librarymixer_id);
}

/* Add/Remove Friends */
bool p3Peers::addUpdateFriend(int librarymixer_id, QString cert, QString name) {
    return mConnMgr->addUpdateFriend(librarymixer_id, cert, name);
}

/* Network Stuff */
void    p3Peers::connectAttempt(int librarymixer_id) {
    mConnMgr->retryConnect(librarymixer_id);
}

void    p3Peers::connectAll() {
    mConnMgr->retryConnectAll();
}

bool    p3Peers::setLocalAddress(int librarymixer_id, std::string addr_str, uint16_t port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    int ret = 1;
    /********************************** WINDOWS/UNIX SPECIFIC PART *******************/
#ifndef WINDOWS_SYS
    if (ret && (0 != inet_aton(addr_str.c_str(), &(addr.sin_addr))))
#else
    addr.sin_addr.s_addr = inet_addr(addr_str.c_str());
    if (ret)
#endif
        /********************************** WINDOWS/UNIX SPECIFIC PART *******************/
    {
        return mConnMgr->setLocalAddress(librarymixer_id, addr);
    }
    return false;
}

bool    p3Peers::setExtAddress(int librarymixer_id, std::string addr_str, uint16_t port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    int ret = 1;
    /********************************** WINDOWS/UNIX SPECIFIC PART *******************/
#ifndef WINDOWS_SYS
    if (ret && (0 != inet_aton(addr_str.c_str(), &(addr.sin_addr))))
#else
    addr.sin_addr.s_addr = inet_addr(addr_str.c_str());
    if (ret)
#endif
        /********************************** WINDOWS/UNIX SPECIFIC PART *******************/
    {
        return mConnMgr->setExtAddress(librarymixer_id, addr);
    }
    return false;
}


bool    p3Peers::setNetworkMode(int librarymixer_id, uint32_t extNetMode) {
    /* translate */
    uint32_t netMode = 0;
    switch (extNetMode) {
        case NETMODE_EXT:
            netMode = NET_MODE_EXT;
            break;
        case NETMODE_UPNP:
            netMode = NET_MODE_UPNP;
            break;
        case NETMODE_UDP:
            netMode = NET_MODE_UDP;
            break;
        case NETMODE_UNREACHABLE:
            netMode = NET_MODE_UNREACHABLE;
            break;
        default:
            break;
    }

    return mConnMgr->setNetworkMode(librarymixer_id, netMode);
}


bool
p3Peers::setVisState(int librarymixer_id, uint32_t extVisState) {
    uint32_t visState = 0;
    if (!(extVisState & VS_DHT_ON))
        visState |= VIS_STATE_NODHT;
    /*  if (!(extVisState & VS_DISC_ON))
                    visState |= VIS_STATE_NODISC;*/

    return mConnMgr->setVisState(librarymixer_id, visState);
}

PeerDetails::PeerDetails()
    :state(PEER_STATE_OFFLINE), netMode(0), lastConnect(0), connectPeriod(0) {
    return;
}

std::ostream &operator<<(std::ostream &out, const PeerDetails &detail) {
    out << "PeerDetail: " << detail.name.toStdString() << "  <" << detail.id << ">";
    out << std::endl;

    out << " fpr:     " << detail.fpr;
    out << std::endl;

    out << " state:       " << detail.state;
    out << " netMode:     " << detail.netMode;
    out << std::endl;

    out << " localAddr:   " << detail.localAddr;
    out << ":" << detail.localPort;
    out << std::endl;
    out << " extAddr:   " << detail.extAddr;
    out << ":" << detail.extPort;
    out << std::endl;


    out << " lastConnect:       " << detail.lastConnect;
    out << " connectPeriod:     " << detail.connectPeriod;
    out << std::endl;

    return out;
}
