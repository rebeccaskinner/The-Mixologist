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

#include "pqi/pqi.h"
#include "pqi/connectionToFriend.h"
#include "pqi/aggregatedConnections.h"

#include "util/debug.h"

ConnectionToFriend::ConnectionToFriend(std::string id, unsigned int librarymixer_id)
    :PQInterface(id, librarymixer_id), active(false), activeConnectionMethod(NULL),
     inConnectAttempt(false), waittimes(0) {

    /* must check id! */

    return;
}

ConnectionToFriend::~ConnectionToFriend() {
    QMap<ConnectionType, connectionMethod *>::iterator it;
    foreach (connectionMethod *method, connectionMethods.values()) {
        delete method;
    }

    connectionMethods.clear();
}


// The PQInterface interface.
int ConnectionToFriend::SendItem(NetItem *i) {
    if (active) {
        return activeConnectionMethod->SendItem(i);
    } else {
        delete i;
    }
    return 0; // queued.
}

NetItem *ConnectionToFriend::GetItem() {
    if (active)
        return activeConnectionMethod->GetItem();
    // else not possible.
    return NULL;
}

int ConnectionToFriend::tick() {
    int activeTick = 0;

    {
        foreach (connectionMethod *method, connectionMethods.values()) {
            if (method->tick() > 0) activeTick = 1;
        }
    }

    return activeTick;
}

// callback function for the child - notify of a change.
// This is only used for out-of-band info....
// otherwise could get dangerous loops.
int ConnectionToFriend::notifyEvent(NetInterface *notifyingInterface, NetNotificationEvent newState) {
    ConnectionType type = TCP_CONNECTION;
    connectionMethod *pqi = NULL;
    foreach (ConnectionType currentType, connectionMethods.keys()) {
        if (connectionMethods[currentType]->thisNetInterface(notifyingInterface)) {
            type = currentType;
            pqi = connectionMethods[currentType];
            break;
        }
    }

    if (!pqi) {
        log(LOG_DEBUG_ALERT, CONNECTION_TO_FRIEND_ZONE, "ConnectionToFriend::notifyEvent() Unknown notifyEvent Source!");
        return -1;
    }

    switch (newState) {
    case NET_CONNECT_SUCCESS:
        aggregatedConnectionsToFriends->notifyConnect(PeerId(), type, 1);

        if (active && (activeConnectionMethod != pqi)) {
            log(LOG_DEBUG_ALERT, CONNECTION_TO_FRIEND_ZONE,
                "ConnectionToFriend::notifyEvent() Connected to friend, but there was an existing connection, resetting");
            activeConnectionMethod->reset();
        }

        active = true;
        activeConnectionMethod = pqi;
        inConnectAttempt = false;

        foreach (connectionMethod *method, connectionMethods.values()) {
            if (method != activeConnectionMethod) method->reset();
        }

        return 1;
    case NET_CONNECT_FAILED:
    case NET_CONNECT_FAILED_RETRY:
        if (active) {
            if (activeConnectionMethod == pqi) {
                log(LOG_DEBUG_ALERT, CONNECTION_TO_FRIEND_ZONE, "ConnectionToFriend::notifyEvent() Connection failed");
                active = false;
                activeConnectionMethod = NULL;
            } else {
                /* Most likely cause of this is if a long-running UDP connection has failed, but the TCP connection has since connected. */
                log(LOG_DEBUG_ALERT, CONNECTION_TO_FRIEND_ZONE,
                    "ConnectionToFriend::notifyEvent() Connection failed (not activeConnectionMethod)");
                return -1;
            }
        } else {
            log(LOG_DEBUG_ALERT, CONNECTION_TO_FRIEND_ZONE, "ConnectionToFriend::notifyEvent() Connection failed while not active");
        }

        if (newState == NET_CONNECT_FAILED)
            aggregatedConnectionsToFriends->notifyConnect(PeerId(), type, -1);
        else if (newState == NET_CONNECT_FAILED_RETRY)
            aggregatedConnectionsToFriends->notifyConnect(PeerId(), type, 0);
        return 1;
    }
    return -1;
}

/***************** Not PQInterface Fns ***********************/

int ConnectionToFriend::reset() {
    log(LOG_DEBUG_BASIC, CONNECTION_TO_FRIEND_ZONE, "ConnectionToFriend::reset() Id: " + QString::number(LibraryMixerId()));

    foreach (connectionMethod *method, connectionMethods.values()) {
        method->reset();
    }

    activeConnectionMethod = NULL;
    active = false;

    return 1;
}

int ConnectionToFriend::addConnectionMethod(ConnectionType type, connectionMethod *pqi) {
    log(LOG_DEBUG_BASIC, CONNECTION_TO_FRIEND_ZONE, "ConnectionToFriend::addConnectionMethod() : Id " + QString::number(LibraryMixerId()));

    connectionMethods[type] = pqi;
    return 1;
}

/***************** PRIVATE FUNCTIONS ***********************/
// functions to iterate over the connects and change state.


int ConnectionToFriend::listen() {
    log(LOG_DEBUG_BASIC, CONNECTION_TO_FRIEND_ZONE, "ConnectionToFriend::listen() Id: " + QString::number(LibraryMixerId()));

    if (!active) {
        foreach (connectionMethod *method, connectionMethods.values()) {
            method->listen();
        }
    }
    return 1;
}


int ConnectionToFriend::stoplistening() {
    log(LOG_DEBUG_BASIC, CONNECTION_TO_FRIEND_ZONE, "ConnectionToFriend::stoplistening() Id: " + QString::number(LibraryMixerId()));

    foreach (connectionMethod *method, connectionMethods.values()) {
        method->stoplistening();
    }

    return 1;
}

int ConnectionToFriend::connect(ConnectionType type, struct sockaddr_in raddr, uint32_t delay, uint32_t period, uint32_t timeout) {
    if (!connectionMethods.contains(type)) return 0;

    connectionMethods[type]->reset();

    connectionMethods[type]->setConnectionParameter(NetInterface::NET_PARAM_CONNECT_DELAY, delay);
    connectionMethods[type]->setConnectionParameter(NetInterface::NET_PARAM_CONNECT_PERIOD, period);
    connectionMethods[type]->setConnectionParameter(NetInterface::NET_PARAM_CONNECT_TIMEOUT, timeout);

    connectionMethods[type]->connect(raddr);

    inConnectAttempt = true;

    return 1;
}


float ConnectionToFriend::getRate(bool in) {
    if ((!active) || (activeConnectionMethod == NULL)) return 0;
    return activeConnectionMethod->getRate(in);
}

void ConnectionToFriend::setMaxRate(bool in, float val) {
    // set to all of them. (and us)
    PQInterface::setMaxRate(in, val);

    foreach (connectionMethod *method, connectionMethods.values()) {
        method->setMaxRate(in, val);
    }
}

