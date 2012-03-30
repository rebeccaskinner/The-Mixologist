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

#include "pqi/aggregatedConnections.h"
#include "pqi/connectivitymanager.h"
#include "pqi/pqissl.h"
#include "pqi/pqissllistener.h"
#include "util/debug.h"
#include "interface/settings.h"
#include "interface/peers.h"
#include <QSettings>

/****
 * #define PQI_DISABLE_UDP 1
 ***/

#ifndef PQI_DISABLE_UDP
#include "pqi/pqissludp.h"
#endif

/* Used as the timeout periods for connecting to a friend. */
const uint32_t TCP_STD_TIMEOUT_PERIOD = 5; /* 5 seconds */
const uint32_t UDP_STD_PERIOD = 5; /* 5 seconds */

/* MUTEX NOTES:
 * Functions from the implemented classes (pqihandler, p3ServiceServer) have their own mutexes. */

AggregatedConnectionsToFriends::AggregatedConnectionsToFriends() :pqil(NULL) {}

int AggregatedConnectionsToFriends::tickServiceRecv() {
    RawItem *incomingItem = NULL;
    int i = 0;
    log(LOG_DEBUG_ALL, AGGREGATED_CONNECTIONS_ZONE, "AggregatedConnectionsToFriends::tickTunnelServer()");

    while (NULL != (incomingItem = GetRawItem())) {
        ++i;
        log(LOG_DEBUG_BASIC, AGGREGATED_CONNECTIONS_ZONE, "AggregatedConnectionsToFriends::tickTunnelServer() Incoming TunnelItem");
        incoming(incomingItem);
    }

    if (0 < i) {
        return 1;
    }
    return 0;
}

int AggregatedConnectionsToFriends::tickServiceSend() {
    RawItem *outboundItem = NULL;
    int i = 0;

    p3ServiceServer::tick();

    while (NULL != (outboundItem = outgoing())) { /* outgoing has own locking */
        ++i;
        log(LOG_DEBUG_BASIC, AGGREGATED_CONNECTIONS_ZONE, "AggregatedConnectionsToFriends::tickTunnelServer() OutGoing NetItem");

        SendRawItem(outboundItem); /* Locked by pqihandler */
    }
    if (0 < i) {
        return 1;
    }
    return 0;
}

int AggregatedConnectionsToFriends::tick() {
    {
        QMutexLocker stack(&coreMtx);
        if (pqil) pqil->tick();
    }

    int i = 0;

    if (tickServiceSend()) i = 1;

    /* does actual Send/Recv */
    if (pqihandler::tick()) i = 1;

    if (tickServiceRecv()) i = 1;

    return i;
}

/* Initialise pqilistener */
void AggregatedConnectionsToFriends::init_listener() {
    QMutexLocker stack(&coreMtx);
    pqil = new pqissllistener(connMgr->getOwnLocalAddress());
}

void AggregatedConnectionsToFriends::load_transfer_rates() {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    setMaxRate(true, settings.value("Transfers/MaxTotalDownloadRate", DEFAULT_MAX_TOTAL_DOWNLOAD).toInt());
    setMaxRate(false, settings.value("Transfers/MaxTotalDownloadRate", DEFAULT_MAX_TOTAL_UPLOAD).toInt());
    setMaxIndivRate(true, settings.value("Transfers/MaxIndividualDownloadRate", DEFAULT_MAX_INDIVIDUAL_DOWNLOAD).toInt());
    setMaxIndivRate(false, settings.value("Transfers/MaxIndividualUploadRate", DEFAULT_MAX_INDIVIDUAL_UPLOAD).toInt());
}


void AggregatedConnectionsToFriends::statusChange(const std::list<pqipeer> &changedFriends) {
    foreach(pqipeer currentPeer, changedFriends) {
        if (currentPeer.actions & PEER_NEW) addPeer(currentPeer.cert_id, currentPeer.librarymixer_id);
        if (currentPeer.actions & PEER_CONNECT_REQ) connectPeer(currentPeer.cert_id, currentPeer.librarymixer_id);
        if (currentPeer.actions & PEER_TIMEOUT) timeoutPeer(currentPeer.cert_id);
    }
}

int AggregatedConnectionsToFriends::addPeer(std::string id, unsigned int librarymixer_id) {
    log(LOG_DEBUG_BASIC, AGGREGATED_CONNECTIONS_ZONE, "AggregatedConnectionsToFriends::addPeer() id: " + QString::number(librarymixer_id));

    ConnectionToFriend *newFriend;
    {
        QMutexLocker stack(&coreMtx);
        QMap<std::string, PQInterface *>::iterator it;
        it = connectionsToFriends.find(id);
        if (it != connectionsToFriends.end()) {
            log(LOG_DEBUG_ALERT, AGGREGATED_CONNECTIONS_ZONE, "AggregatedConnectionsToFriends::addPeer() Peer already in Use!");
            return -1;
        }

        newFriend = createPerson(id, librarymixer_id, pqil);

        //reset it to start it working.
        newFriend->reset();
        newFriend->listen();
    }
    return AddPQI(newFriend);
}

int AggregatedConnectionsToFriends::connectPeer(std::string cert_id, unsigned int librarymixer_id) {
    QMutexLocker stack(&coreMtx);
    if (!connectionsToFriends.contains(cert_id)) return 0;

    ConnectionToFriend *currentFriend = (ConnectionToFriend *) connectionsToFriends[cert_id];

    /* Dequeue a connection attempt from ConnectivityManager */
    if (!connMgr) return 0;

    struct sockaddr_in addr;
    uint32_t delay;
    QueuedConnectionType queuedConnectionType;

    if (!connMgr->getQueuedConnectAttempt(librarymixer_id, addr, delay, queuedConnectionType)) {
        return 0;
    }

    ConnectionType ptype;
    if (queuedConnectionType == CONNECTION_TYPE_TCP_LOCAL ||
        queuedConnectionType == CONNECTION_TYPE_TCP_EXTERNAL) {
        ptype = TCP_CONNECTION;
        log(LOG_DEBUG_ALERT, AGGREGATED_CONNECTIONS_ZONE,
            "Attempting TCP connection to " + QString::number(librarymixer_id) +
            " via " + QString(inet_ntoa(addr.sin_addr)) +
            ":" + QString::number(ntohs(addr.sin_port)));
        currentFriend->connect(ptype, addr, delay, 0, TCP_STD_TIMEOUT_PERIOD);
    } else if (queuedConnectionType == CONNECTION_TYPE_UDP) {
        ptype = UDP_CONNECTION;
        log(LOG_DEBUG_ALERT, AGGREGATED_CONNECTIONS_ZONE,
            "Attempting UDP connection to " + QString::number(librarymixer_id) +
            " via " + QString(inet_ntoa(addr.sin_addr)) +
            ":" + QString::number(ntohs(addr.sin_port)));
        currentFriend->connect(ptype, addr, delay, UDP_STD_PERIOD, UDP_STD_PERIOD * 2);
    } else return 0;

    return 1;
}

void AggregatedConnectionsToFriends::timeoutPeer(std::string cert_id) {
    QMutexLocker stack(&coreMtx);
    if (!connectionsToFriends.contains(cert_id)) return;

    ((ConnectionToFriend *) connectionsToFriends[cert_id])->reset();
}

bool AggregatedConnectionsToFriends::notifyConnect(std::string id, ConnectionType ptype, int result) {
    bool tcpConnection;
    if (ptype == TCP_CONNECTION) {
        tcpConnection = true;
    } else {
        tcpConnection = false;
    }

    if (connMgr) connMgr->reportConnectionUpdate(authMgr->findLibraryMixerByCertId(id), result, tcpConnection);

    return (NULL != connMgr);
}

ConnectionToFriend *AggregatedConnectionsToFriends::createPerson(std::string id, unsigned int librarymixer_id, pqilistener *listener) {
    log(LOG_DEBUG_BASIC, AGGREGATED_CONNECTIONS_ZONE, "AggregatedConnectionsToFriends::createPerson() New friend " + QString::number(librarymixer_id));

    ConnectionToFriend *newPerson = new ConnectionToFriend(id, librarymixer_id);
    pqissl *newSsl = new pqissl((pqissllistener *) listener, newPerson);

    Serialiser *newTcpSerialiser = new Serialiser();
    newTcpSerialiser->addSerialType(new FileItemSerialiser());
    newTcpSerialiser->addSerialType(new ServiceSerialiser());

    connectionMethod *tcpInterface = new connectionMethod(newTcpSerialiser, newSsl);

    newPerson->addConnectionMethod(TCP_CONNECTION, tcpInterface);

#ifndef PQI_DISABLE_UDP
    pqissludp *newSslUdp = new pqissludp(newPerson);

    Serialiser *newUdpSerialiser = new Serialiser();
    newUdpSerialiser->addSerialType(new FileItemSerialiser());
    newUdpSerialiser->addSerialType(new ServiceSerialiser());

    connectionMethod *udpInterface  = new connectionMethod(newUdpSerialiser, newSslUdp);

    newPerson->addConnectionMethod(UDP_CONNECTION, udpInterface);
#endif

    return newPerson;
}

#ifdef false
int AggregatedConnectionsToFriends::removePeer(std::string id) {
    QMap<std::string, PQInterface *>::iterator it;

    QMutexLocker stack(&coreMtx);

    it = connectionsToFriends.find(id);
    if (it != connectionsToFriends.end()) {
        // Don't duplicate remove!!!
        //RemovePQInterface(p);
        ConnectionToFriend *p = (ConnectionToFriend *) it->second;
        p->reset();
        delete p;
        connectionsToFriends.erase(it);
    }
    return 1;
}
#endif

#ifdef false
int AggregatedConnectionsToFriends::restart_listener() {
    // stop it,
    // change the address.
    // restart.
    bool haveListener = false;
    {
        QMutexLocker stack(&coreMtx);
        haveListener = (pqil != NULL);
    } /* UNLOCKED */


    if (haveListener) {
        QMutexLocker stack(&coreMtx);

        pqil->resetlisten();
        pqil->setListenAddr(connMgr->getOwnLocalAddress());
        pqil->setuplisten();
    }
    return 1;
}
#endif

