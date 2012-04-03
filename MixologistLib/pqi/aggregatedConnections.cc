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
#include "pqi/connectionToFriend.h"
#include "pqi/friendsConnectivityManager.h"
#include "pqi/ownConnectivityManager.h"
#include "pqi/pqissl.h"
#include "pqi/pqissllistener.h"
#include "tcponudp/udpsorter.h"
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
        log(LOG_DEBUG_BASIC, AGGREGATED_CONNECTIONS_ZONE,
            "AggregatedConnectionsToFriends::tickTunnelServer() Incoming TunnelItem from " + QString::number(incomingItem->LibraryMixerId()));
        incoming(incomingItem);
    }

    if (i > 0) return 1;

    return 0;
}

int AggregatedConnectionsToFriends::tickServiceSend() {
    RawItem *outboundItem = NULL;
    int i = 0;

    p3ServiceServer::tick();

    while (NULL != (outboundItem = outgoing())) { /* outgoing has own locking */
        ++i;
        log(LOG_DEBUG_BASIC, AGGREGATED_CONNECTIONS_ZONE,
            "AggregatedConnectionsToFriends::tickTunnelServer() OutGoing NetItem to " + QString::number(outboundItem->LibraryMixerId()));

        SendRawItem(outboundItem); /* Locked by pqihandler */
    }
    if (i > 0) return 1;

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
    pqil = new pqissllistener(ownConnectivityManager->getOwnLocalAddress());
}

void AggregatedConnectionsToFriends::stop_listener() {
    QMutexLocker stack(&coreMtx);
    if (pqil) {
        pqil->resetlisten();
        delete pqil;
        pqil = NULL;
    }
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
        if (currentPeer.actions & PEER_CONNECT_REQ) connectPeer(currentPeer.librarymixer_id);
        if (currentPeer.actions & PEER_TIMEOUT) timeoutPeer(currentPeer.librarymixer_id);
        if (currentPeer.actions & PEER_CERT_AND_ADDRESS_UPDATED) {
            removePeer(currentPeer.librarymixer_id);
            addPeer(currentPeer.cert_id, currentPeer.librarymixer_id);
        }
    }
}

int AggregatedConnectionsToFriends::addPeer(std::string id, unsigned int librarymixer_id) {
    log(LOG_DEBUG_BASIC, AGGREGATED_CONNECTIONS_ZONE, "AggregatedConnectionsToFriends::addPeer() id: " + QString::number(librarymixer_id));

    {
        QMutexLocker stack(&coreMtx);
        if (connectionsToFriends.contains(librarymixer_id)) {
            log(LOG_DEBUG_ALERT, AGGREGATED_CONNECTIONS_ZONE, "AggregatedConnectionsToFriends::addPeer() Peer with that ID already exists!");
            return -1;
        }
    }

    ConnectionToFriend *newFriend = createPerson(id, librarymixer_id, pqil);

    //reset it to start it working.
    newFriend->reset();
    newFriend->listen();

    return AddPQI(newFriend);
}

bool AggregatedConnectionsToFriends::removePeer(unsigned int librarymixer_id) {
    log(LOG_DEBUG_BASIC, AGGREGATED_CONNECTIONS_ZONE, "AggregatedConnectionsToFriends::removePeer() id: " + QString::number(librarymixer_id));

    QMutexLocker stack(&coreMtx);

    if (!connectionsToFriends.contains(librarymixer_id)) return false;

    ConnectionToFriend *friendToRemove = (ConnectionToFriend *) connectionsToFriends[librarymixer_id];
    friendToRemove->reset();
    delete friendToRemove;
    connectionsToFriends.remove(librarymixer_id);

    return true;
}

int AggregatedConnectionsToFriends::connectPeer(unsigned int librarymixer_id) {
    QMutexLocker stack(&coreMtx);
    if (!connectionsToFriends.contains(librarymixer_id)) return 0;

    ConnectionToFriend *currentFriend = (ConnectionToFriend *) connectionsToFriends[librarymixer_id];

    /* Dequeue a connection attempt from ConnectivityManager */
    if (!friendsConnectivityManager) return 0;

    struct sockaddr_in addr;
    QueuedConnectionType queuedConnectionType;

    if (!friendsConnectivityManager->getQueuedConnectAttempt(librarymixer_id, addr, queuedConnectionType)) {
        return 0;
    }

    ConnectionType ptype;
    if (queuedConnectionType == CONNECTION_TYPE_TCP_LOCAL ||
        queuedConnectionType == CONNECTION_TYPE_TCP_EXTERNAL) {
        ptype = TCP_CONNECTION;
        log(LOG_DEBUG_ALERT, AGGREGATED_CONNECTIONS_ZONE,
            "Attempting TCP connection to " + QString::number(librarymixer_id) + " via " + addressToString(&addr));
        currentFriend->connect(ptype, addr, 0, TCP_STD_TIMEOUT_PERIOD);
    } else if (queuedConnectionType == CONNECTION_TYPE_UDP) {
        ptype = UDP_CONNECTION;
        log(LOG_DEBUG_ALERT, AGGREGATED_CONNECTIONS_ZONE,
            "Attempting UDP connection to " + QString::number(librarymixer_id) + " via " + addressToString(&addr));
        if (udpMainSocket) {
            udpMainSocket->sendUdpConnectionNotice(&addr, ownConnectivityManager->getOwnExternalAddress(), peers->getOwnLibraryMixerId());
        }
        currentFriend->connect(ptype, addr, UDP_STD_PERIOD, UDP_STD_PERIOD * 2);
    } else if (queuedConnectionType == CONNECTION_TYPE_TCP_BACK) {
        if (udpMainSocket) {
            udpMainSocket->sendTcpConnectionRequest(&addr, ownConnectivityManager->getOwnExternalAddress(), peers->getOwnLibraryMixerId());
        }
        /* We can just immediately report failure now as this method doesn't produce an instant connection,
           at best we'll have a new incoming connection that is handled seperately from here. */
        friendsConnectivityManager->reportConnectionUpdate(librarymixer_id, -1, TCP_CONNECTION, &addr);
    }

    return 1;
}

void AggregatedConnectionsToFriends::timeoutPeer(unsigned int librarymixer_id) {
    QMutexLocker stack(&coreMtx);
    if (!connectionsToFriends.contains(librarymixer_id)) return;

    ((ConnectionToFriend *) connectionsToFriends[librarymixer_id])->reset();
}

bool AggregatedConnectionsToFriends::notifyConnect(unsigned int librarymixer_id, int result, ConnectionType type, struct sockaddr_in *remoteAddress) {
    if (friendsConnectivityManager) friendsConnectivityManager->reportConnectionUpdate(librarymixer_id, result, type, remoteAddress);

    return (NULL != friendsConnectivityManager);
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
