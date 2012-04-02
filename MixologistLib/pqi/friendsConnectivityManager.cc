/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-8, Robert Fernie
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


#include <tcponudp/tou.h>
#include <tcponudp/udpsorter.h>
#include <tcponudp/stunpacket.h>

#include <upnp/upnphandler.h>

#include <pqi/friendsConnectivityManager.h>
#include <pqi/ownConnectivityManager.h>
#include <pqi/pqinotify.h>

#include <interface/peers.h>
#include <interface/settings.h>
#include <interface/librarymixer-connect.h>

#include <util/debug.h>

//#define NO_TCP_CONNECTIONS 1
//#define NO_TCP_BACK_CONNECTIONS 1
//#define NO_AUTO_CONNECTION 1
//#define NO_UDP_CONNECTIONS 1

#define TCP_RETRY_PERIOD 600 //10 minute wait between automatic connection retries
#define TCP_RETRY_PERIOD_SYMMETRIC_NAT 60 //1 minute wait between automatic connection retries
#define USED_SOCKET_WAIT_TIME 5 //5 second wait if tried to connect to an IP that was already in use in a connection attempt
#define REQUESTED_RETRY_WAIT_TIME 10 //10 seconds wait if our connection attempt failed and we were signaled to try again
#define LAST_HEARD_TIMEOUT 300 //5 minute wait if haven't heard from a friend before we mark them as disconnected
#define UDP_PUNCHING_PERIOD 20 //20 seconds between UDP hole punches

QueuedConnectionAttempt::QueuedConnectionAttempt() {sockaddr_clear(&addr);}

peerConnectState::peerConnectState()
    :name(""),
     id(""),
     lastcontact(0),
     nextTcpTryAt(0),
     state(FCS_NOT_MIXOLOGIST_ENABLED),
     actions(0) {
    sockaddr_clear(&localaddr);
    sockaddr_clear(&serveraddr);
}

FriendsConnectivityManager::FriendsConnectivityManager()
    :mStatusChanged(false),
     friendsManagerEnabled(false) {
    connect(librarymixerconnect, SIGNAL(downloadedFriends()), this, SLOT(friendsListUpdated()));
}

void FriendsConnectivityManager::tick() {
    friendsListUpdateTick();
    connectivityTick();
    monitorsTick();
}

void FriendsConnectivityManager::setEnabled(bool enabled) {
    {
        QMutexLocker stack(&connMtx);
        friendsManagerEnabled = enabled;
    }
    if (enabled) {
        tryConnectAll();
    }
}

/**********************************************************************************
 * Friends List Updating
 **********************************************************************************/

#define FRIENDS_LIST_DOWNLOAD_TIMEOUT 10
#define FRIENDS_LIST_UPDATE_PERIOD_LIMITED_INBOUND 300 //5 minute wait when inbound connections from friends will have difficulty
void FriendsConnectivityManager::friendsListUpdateTick() {
    QMutexLocker stack(&connMtx);

    if (!friendsManagerEnabled) return;

    /* Only restricted-cone and symmetric NAT firewalled connections need to maintain their friends list periodically.
       All other connections should be able to handle inbound connections, and if necessary they friends list can be updated in response to those. */
    if (ownConnectivityManager->getConnectionStatus() != CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING &&
        ownConnectivityManager->getConnectionStatus() != CONNECTION_STATUS_SYMMETRIC_NAT ) return;

    static time_t friendsListUpdateAttemptTime = 0;
    time_t now = time(NULL);
    if (now - FRIENDS_LIST_UPDATE_PERIOD_LIMITED_INBOUND > friendsListUpdateTime) {
        if (now - FRIENDS_LIST_DOWNLOAD_TIMEOUT > friendsListUpdateAttemptTime) {
            friendsListUpdateAttemptTime = time(NULL);
            librarymixerconnect->downloadFriends();
        }
    }
}

void FriendsConnectivityManager::friendsListUpdated() {
    QMutexLocker stack(&connMtx);
    friendsListUpdateTime = time(NULL);
    log(LOG_DEBUG_ALERT, FRIEND_CONNECTIVITY_ZONE, "Updated friends list from LibraryMixer.");
}

/**********************************************************************************
 * Connecting to Friends
 **********************************************************************************/

#define ADDRESS_UPLOAD_TIMEOUT 5
void FriendsConnectivityManager::connectivityTick() {
    QList<unsigned int> friendsToConnect;
    {
        QMutexLocker stack(&connMtx);

        if (!friendsManagerEnabled) return;

        time_t now = time(NULL);

        /* If applicable, do the UDP hole punching.
           Note that we are treating CONNECTION_STATUS_UDP_HOLE_PUNCHING (full-cone NAT hole punching)
           the same as CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING for now.
           The full-cone NAT hole punching actually only requires punching one friend each cycle, but we are punching all friends each cycle for both. */

        if (ownConnectivityManager->getConnectionStatus() == CONNECTION_STATUS_UDP_HOLE_PUNCHING ||
            ownConnectivityManager->getConnectionStatus() == CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING) {
            static time_t last_punch = 0;
            if (now - last_punch > UDP_PUNCHING_PERIOD) {
                last_punch = now;
                foreach (peerConnectState currentFriend, friendsConnectivityManager->mFriendList.values()) {
                    if (currentFriend.state == FCS_NOT_CONNECTED) {
                        if (udpMainSocket) {
                            udpMainSocket->sendUdpTunneler(&currentFriend.serveraddr, ownConnectivityManager->getOwnExternalAddress(), peers->getOwnLibraryMixerId());
                        }
                    }
                }
            }
        }

        time_t timeoutIfOlder = now - LAST_HEARD_TIMEOUT;

        /* Do both the timeout of connected friends, and any scheduled TCP trying now. */
        /* Can't simply use mFriendList.values() in foreach because that generates copies of the peerConnectState,
           and we need to be able to save in the changes. */
        foreach (unsigned int librarymixer_id, mFriendList.keys()) {
            if (mFriendList[librarymixer_id].state == FCS_NOT_MIXOLOGIST_ENABLED) continue;

            /* Check if connected peers need to be timed out */
            else if (mFriendList[librarymixer_id].state == FCS_CONNECTED_TCP ||
                     mFriendList[librarymixer_id].state == FCS_CONNECTED_UDP) {
                if (mFriendList[librarymixer_id].lastheard < timeoutIfOlder) {
                    log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE, QString("Connection with ") + mFriendList[librarymixer_id].name + " has timed out");

                    mFriendList[librarymixer_id].state = FCS_NOT_CONNECTED;
                    mFriendList[librarymixer_id].actions |= PEER_TIMEOUT;
                    mFriendList[librarymixer_id].lastcontact = time(NULL);

                    /* Attempt an immediate reconnect. */
                    friendsToConnect.push_back(librarymixer_id);
                } else continue;
            }

            /* If we have a scheduled try then start it. */
            else if (mFriendList[librarymixer_id].nextTcpTryAt < now) {
                friendsToConnect.push_back(librarymixer_id);
            }
        }
    }

#ifndef NO_AUTO_CONNECTION
    foreach (unsigned int librarymixer_id, friendsToConnect) {
        tryConnectTCP(librarymixer_id);
    }
#endif

}

bool FriendsConnectivityManager::tryConnectTCP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    /* We can now consider this attempt tried so that tick doesn't try to schedule again, and increment the next try time. */
    if (ownConnectivityManager->getConnectionStatus() == CONNECTION_STATUS_SYMMETRIC_NAT)
        connectState->nextTcpTryAt = time(NULL) + TCP_RETRY_PERIOD_SYMMETRIC_NAT;
    else connectState->nextTcpTryAt = time(NULL) + TCP_RETRY_PERIOD;

    if (connectState->state == FCS_CONNECTED_TCP || connectState->state == FCS_CONNECTED_UDP) return true;

#ifndef NO_TCP_CONNECTIONS

    /* If address is valid, on the same subnet, not the same as external address, and not the same as own addresses, add it as a local address to try. */
    if (isValidNet(&connectState->localaddr.sin_addr) &&
        isSameSubnet(&ownConnectivityManager->getOwnLocalAddress()->sin_addr, &connectState->localaddr.sin_addr) &&
        (!isSameAddress(&connectState->localaddr, &connectState->serveraddr)) &&
        (!isSameAddress(ownConnectivityManager->getOwnLocalAddress(), &connectState->localaddr)) &&
        (!isSameAddress(ownConnectivityManager->getOwnExternalAddress(), &connectState->localaddr))) {
        queueConnectionAttempt(connectState, CONNECTION_TYPE_TCP_LOCAL);
    }

    /* Always try external unless address is the same as one of ours. */
    if (isValidNet(&(connectState->serveraddr.sin_addr)) &&
        (!isSameAddress(ownConnectivityManager->getOwnLocalAddress(), &connectState->serveraddr)) &&
        (!isSameAddress(ownConnectivityManager->getOwnExternalAddress(), &connectState->serveraddr))) {
        queueConnectionAttempt(connectState, CONNECTION_TYPE_TCP_EXTERNAL);
    }

    /* If it's already in a connection attempt, it'll automatically use the new addresses. */
    if (connectState->state == FCS_IN_CONNECT_ATTEMPT) return true;

    /* Otherwise, start a connection attempt. */
    if (connectState->queuedConnectionAttempts.size() > 0) {
        connectState->actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }

#endif // NO_TCP_CONNECTIONS

    return true;
}

bool FriendsConnectivityManager::tryConnectBackTCP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    if (connectState->state == FCS_CONNECTED_TCP ||
        connectState->state == FCS_CONNECTED_UDP) return true;

#ifndef NO_TCP_BACK_CONNECTIONS
    queueConnectionAttempt(connectState, CONNECTION_TYPE_TCP_BACK);

    /* If it's already in a connection attempt, it'll automatically use the new addresses. */
    if (connectState->state == FCS_IN_CONNECT_ATTEMPT) return true;

    /* Start a connection attempt. */
    if (connectState->queuedConnectionAttempts.size() > 0) {
        connectState->actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }
#endif
    return true;
}

bool FriendsConnectivityManager::tryConnectUDP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    if (connectState->state == FCS_CONNECTED_TCP ||
        connectState->state == FCS_CONNECTED_UDP) return true;

#ifndef NO_UDP_CONNECTIONS
    queueConnectionAttempt(connectState, CONNECTION_TYPE_UDP);

    /* If it's already in a connection attempt, it'll automatically use the new addresses. */
    if (connectState->state == FCS_IN_CONNECT_ATTEMPT) return true;

    /* Start a connection attempt. */
    if (connectState->queuedConnectionAttempts.size() > 0) {
        connectState->actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }
#endif //NO_UDP_CONNECTIONS

    return true;
}

void FriendsConnectivityManager::receivedUdpTunneler(unsigned int librarymixer_id, QString address, ushort port) {
    {
        QMutexLocker stack(&connMtx);
        if (!friendsManagerEnabled) return;

        if (!mFriendList.contains(librarymixer_id)) {
            log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
                "Received a UDP packet from an unknown user with LibraryMixer ID " + QString::number(librarymixer_id) +
                ", updating friend list");
            librarymixerconnect->downloadFriends();
            return;
        }

        if (addressToString(&mFriendList[librarymixer_id].serveraddr) != (address + ":" + QString::number(port))) {
            librarymixerconnect->downloadFriends();
            return;
        }

        if (ownConnectivityManager->getConnectionStatus() != CONNECTION_STATUS_UDP_HOLE_PUNCHING &&
            ownConnectivityManager->getConnectionStatus() != CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING) {
            log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
                "Received a UDP packet from a friend but do not believe ourselves to be firewalled, requesting TCP connect back");
            tryConnectBackTCP(librarymixer_id);
        }

    }

    tryConnectUDP(librarymixer_id);
}

void FriendsConnectivityManager::receivedUdpConnectionNotice(unsigned int librarymixer_id, QString address, ushort port) {
    {
        /* UdpConnectionNotices should only be received in response to UdpTunnelers we sent.
           Receiving one where any information is out of the ordinary is completely unexpected,
           and likely indicates some sort of suspicious activity.
           We don't even update our friends list for this, because there is no scenario where our friends list is out-of-date
           that would result in this scenario. */
        QMutexLocker stack(&connMtx);
        if (!friendsManagerEnabled) return;

        if (!mFriendList.contains(librarymixer_id)) {
            log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
                "Received a UDP connection notification from an unknown user with LibraryMixer ID " + QString::number(librarymixer_id) +
                ", this is unexpected, ignoring");
            return;
        }

        if (addressToString(&mFriendList[librarymixer_id].serveraddr) != (address + ":" + QString::number(port))) {
            log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
                "Received a UDP connection notification that contains invalid data, ignoring");
            return;
        }
    }

    tryConnectUDP(librarymixer_id);
}

void FriendsConnectivityManager::receivedTcpConnectionRequest(unsigned int librarymixer_id, QString address, ushort port) {
    {
        QMutexLocker stack(&connMtx);
        if (!friendsManagerEnabled) return;

        if (!mFriendList.contains(librarymixer_id)) {
            log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
                "Received a request to connect from an unknown user with LibraryMixer ID " + QString::number(librarymixer_id) +
                ", updating friend list");
            librarymixerconnect->downloadFriends();
            return;
        }

        if (addressToString(&mFriendList[librarymixer_id].serveraddr) != (address + ":" + QString::number(port))) {
            librarymixerconnect->downloadFriends();
            return;
        }
    }
    tryConnectTCP(librarymixer_id);
}

/**********************************************************************************
 * The interface to AggregatedConnectionsToFriends
 **********************************************************************************/

bool FriendsConnectivityManager::getQueuedConnectAttempt(unsigned int librarymixer_id, struct sockaddr_in &addr,
                                                         QueuedConnectionType &queuedConnectionType) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
            QString("Can't make attempt to connect to user, not in friends list, id was: ").append(librarymixer_id));
        return false;
    }

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    if (connectState->queuedConnectionAttempts.size() < 1) {
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
            QString("Can't make attempt to connect to user, have no queued attempts: ").append(connectState->name));
        return false;
    }

    QueuedConnectionAttempt currentAttempt = connectState->queuedConnectionAttempts.front();
    connectState->queuedConnectionAttempts.pop_front();

    /* Test if socket is in use already.
       We let TCP connect back requests through, as they aren't real connection attempts, but rather just one-off packets that we send,
       so they wouldn't interfere with any existing connection attempt.
       Furthermore, TCP connect back requests often come right before a UDP connection attempt, and we don't want any timing issues
       here to move this to behind the UDP connection in the queue, causing us to connect using a UDP connection when a TCP connection may have worked fine. */
    if (currentAttempt.queuedConnectionType != CONNECTION_TYPE_TCP_BACK &&
        usedSockets.contains(addressToString(&currentAttempt.addr))) {
        /* If we have an alternative attempt to try, simply stick this one on the back and request a try for a different attempt in the meantime. */
        if (connectState->queuedConnectionAttempts.size() >= 1) {
            connectState->queuedConnectionAttempts.push_back(currentAttempt);
            connectState->actions |= PEER_CONNECT_REQ;
            mStatusChanged = true;
        } else {
            if (usedSockets[addressToString(&currentAttempt.addr)] == USED_IP_CONNECTED) {
                log(LOG_DEBUG_ALERT, FRIEND_CONNECTIVITY_ZONE,
                    "FriendsConnectivityManager::connectAttempt Can not connect to " + addressToString(&currentAttempt.addr) + " due to existing connection.");
            } else if (usedSockets[addressToString(&currentAttempt.addr)] == USED_IP_CONNECTING) {
                if (currentAttempt.queuedConnectionType == CONNECTION_TYPE_TCP_LOCAL ||
                    currentAttempt.queuedConnectionType == CONNECTION_TYPE_TCP_EXTERNAL) {
                    connectState->nextTcpTryAt = time(NULL) + USED_SOCKET_WAIT_TIME;
                    log(LOG_DEBUG_BASIC, FRIEND_CONNECTIVITY_ZONE,
                        "FriendsConnectivityManager::connectAttempt Waiting to try to connect to " + addressToString(&currentAttempt.addr) + " due to existing attempted connection.");
                } else if (currentAttempt.queuedConnectionType == CONNECTION_TYPE_UDP) {
                    log(LOG_DEBUG_BASIC, FRIEND_CONNECTIVITY_ZONE,
                        "FriendsConnectivityManager::connectAttempt Abandoning attempt to connect over UDP to " + addressToString(&currentAttempt.addr) +
                        " due to existing attempted connection, will try again on next incoming UDP Tunneler.");
                }
            }
        }
        return false;
    }

    /* If we get here, the IP address is good to use, so load it in. */
    connectState->state = FCS_IN_CONNECT_ATTEMPT;
    connectState->currentConnectionAttempt = currentAttempt;

    /* No need to block the socket for TCP connect back connections.
       Moreover, we let these through without regard to pre-existing connection attempts, so don't clobber anything that was already there. */
    if (currentAttempt.queuedConnectionType != CONNECTION_TYPE_TCP_BACK)
        usedSockets[addressToString(&currentAttempt.addr)] = USED_IP_CONNECTING;

    addr = connectState->currentConnectionAttempt.addr;
    queuedConnectionType = connectState->currentConnectionAttempt.queuedConnectionType;

    log(LOG_DEBUG_BASIC, FRIEND_CONNECTIVITY_ZONE,
        QString("FriendsConnectivityManager::connectAttempt Providing information for connection attempt to user: ").append(connectState->name));

    return true;
}

bool FriendsConnectivityManager::reportConnectionUpdate(unsigned int librarymixer_id, int result) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    if (result == 1) {
        /* No longer need any of the queued connection attempts. */
        mFriendList[librarymixer_id].queuedConnectionAttempts.clear();

        /* Mark this socket as used so no other connection attempt can try to use it. */
        usedSockets[addressToString(&mFriendList[librarymixer_id].currentConnectionAttempt.addr)] = USED_IP_CONNECTED;

        log(LOG_DEBUG_BASIC, FRIEND_CONNECTIVITY_ZONE,
            QString("Successfully connected to: ") + mFriendList[librarymixer_id].name +
            " (" + addressToString(&mFriendList[librarymixer_id].currentConnectionAttempt.addr) + ")");

        /* change state */
        if (mFriendList[librarymixer_id].currentConnectionAttempt.queuedConnectionType == CONNECTION_TYPE_TCP_LOCAL ||
            mFriendList[librarymixer_id].currentConnectionAttempt.queuedConnectionType == CONNECTION_TYPE_TCP_EXTERNAL)
            mFriendList[librarymixer_id].state = FCS_CONNECTED_TCP;
        else if (mFriendList[librarymixer_id].currentConnectionAttempt.queuedConnectionType == CONNECTION_TYPE_UDP)
            mFriendList[librarymixer_id].state = FCS_CONNECTED_UDP;
        mFriendList[librarymixer_id].actions |= PEER_CONNECTED;
        mStatusChanged = true;
        mFriendList[librarymixer_id].lastcontact = time(NULL);
        mFriendList[librarymixer_id].lastheard = time(NULL);
    } else {
        log(LOG_DEBUG_BASIC,
            FRIEND_CONNECTIVITY_ZONE,
            QString("Unable to connect to friend: ") + mFriendList[librarymixer_id].name +
            ", over transport layer type: " + QString::number(mFriendList[librarymixer_id].currentConnectionAttempt.queuedConnectionType));

        /* TCP connect back requests don't appear in the usedSockets list. */
        if (mFriendList[librarymixer_id].currentConnectionAttempt.queuedConnectionType != CONNECTION_TYPE_TCP_BACK)
            usedSockets.remove(addressToString(&mFriendList[librarymixer_id].currentConnectionAttempt.addr));

        /* We may receive a failure report if we were connected and the connection failed. */
        if (mFriendList[librarymixer_id].state == FCS_CONNECTED_TCP ||
            mFriendList[librarymixer_id].state == FCS_CONNECTED_UDP) {
            mFriendList[librarymixer_id].lastcontact = time(NULL);
        }

        mFriendList[librarymixer_id].state = FCS_NOT_CONNECTED;

        /* If we have been requested to schedule another TCP connection attempt. */
        if (result == 0) mFriendList[librarymixer_id].nextTcpTryAt = time(NULL) + REQUESTED_RETRY_WAIT_TIME;

        /* If we still have other addresses in the queue to try, signal the aggregatedConnectionsToFriends to try them. */
        if (mFriendList[librarymixer_id].queuedConnectionAttempts.size() > 0) {
            mFriendList[librarymixer_id].actions |= PEER_CONNECT_REQ;
            mStatusChanged = true;
        }
    }

    return true;
}

void FriendsConnectivityManager::queueConnectionAttempt(peerConnectState *connectState, QueuedConnectionType queuedConnectionType) {
    /* Only add this address if there is not already one of that type on there already.
       First check if we're in a current connection attempt with that type.
       Then check if any of the queued connection attempts are for that type. */
    if (connectState->state == FCS_IN_CONNECT_ATTEMPT) {
        if (connectState->currentConnectionAttempt.queuedConnectionType == queuedConnectionType) return;
    }
    foreach (QueuedConnectionAttempt queuedAddress, connectState->queuedConnectionAttempts) {
        if (queuedAddress.queuedConnectionType == queuedConnectionType) return;
    }

    /* If we get here, this is new, add the connection attempt. */
    QueuedConnectionAttempt addressToConnect;
    addressToConnect.queuedConnectionType = queuedConnectionType;
    if (queuedConnectionType == CONNECTION_TYPE_TCP_LOCAL) addressToConnect.addr = connectState->localaddr;
    else addressToConnect.addr = connectState->serveraddr;

    log(LOG_DEBUG_ALERT,
        FRIEND_CONNECTIVITY_ZONE,
        QString("FriendsConnectivityManager::queueConnectionAttempt") +
        " Attempting connection to friend #" + QString::number(connectState->librarymixer_id) +
        " at " + addressToString(&addressToConnect.addr));

    connectState->queuedConnectionAttempts.push_back(addressToConnect);
}

/**********************************************************************************
 * MixologistLib functions, not shared via interface with GUI
 **********************************************************************************/

bool FriendsConnectivityManager::getPeerConnectState(unsigned int librarymixer_id, peerConnectState &state) {
    QMutexLocker stack(&connMtx);

    if (librarymixer_id == peers->getOwnLibraryMixerId()) {
        peerConnectState ownState;
        ownState.localaddr = *ownConnectivityManager->getOwnLocalAddress();
        ownState.serveraddr = *ownConnectivityManager->getOwnExternalAddress();
        ownState.librarymixer_id = peers->getOwnLibraryMixerId();
        ownState.id = peers->getOwnCertId();
        state = ownState;
    } else {
        if (!mFriendList.contains(librarymixer_id)) return false;
        state = mFriendList[librarymixer_id];
    }
    return true;
}

void FriendsConnectivityManager::heardFrom(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);
    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_ERROR, FRIEND_CONNECTIVITY_ZONE, QString("Somehow heard from someone that is not a friend: ").append(librarymixer_id));
        return;
    }
    mFriendList[librarymixer_id].lastheard = time(NULL);
}

void FriendsConnectivityManager::getExternalAddresses(QList<struct sockaddr_in> &toFill) {
    QMutexLocker stack(&connMtx);
    foreach (unsigned int librarymixer_id, mFriendList.keys()) {
        toFill.append(mFriendList[librarymixer_id].serveraddr);
    }
}

/**********************************************************************************
 * Monitors
 **********************************************************************************/

void FriendsConnectivityManager::addMonitor(pqiMonitor *mon) {
    QMutexLocker stack(&connMtx);
    if (monitorListeners.contains(mon)) {
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Internal error", "Initialization after already initialized");
        return;
    }
    monitorListeners.push_back(mon);
}

void FriendsConnectivityManager::monitorsTick() {
    bool doStatusChange = false;
    std::list<pqipeer> actionList;
    QMap<unsigned int, peerConnectState>::iterator it;

    {
        QMutexLocker stack(&connMtx);

        if (mStatusChanged) {
            mStatusChanged = false;
            doStatusChange = true;

            foreach (unsigned int librarymixer_id, mFriendList.keys()) {
                if (mFriendList[librarymixer_id].actions) {
                    /* add in */
                    pqipeer peer;
                    peer.librarymixer_id = librarymixer_id;
                    peer.cert_id = mFriendList[librarymixer_id].id;
                    peer.name = mFriendList[librarymixer_id].name;
                    peer.state = mFriendList[librarymixer_id].state;
                    peer.actions = mFriendList[librarymixer_id].actions;

                    /* reset action */
                    mFriendList[librarymixer_id].actions = 0;

                    actionList.push_back(peer);

                    /* notify GUI */
                    if (peer.actions & PEER_CONNECTED) {
                        pqiNotify *notify = getPqiNotify();
                        if (notify) {
                            notify->AddPopupMessage(POPUP_CONNECT, QString::number(librarymixer_id), "Online: ");
                        }
                    }
                }
            }
        }
    }

    if (doStatusChange) {
        foreach (pqiMonitor *client, monitorListeners) {
            client->statusChange(actionList);
        }
    }
}

/**********************************************************************************
 * Body of public-facing API functions called through p3peers
 **********************************************************************************/
void FriendsConnectivityManager::getOnlineList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        if (mFriendList[friend_id].state == FCS_CONNECTED_TCP ||
            mFriendList[friend_id].state == FCS_CONNECTED_UDP)
            peers.push_back(friend_id);
    }
}

void FriendsConnectivityManager::getSignedUpList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        if (mFriendList[friend_id].state != FCS_NOT_MIXOLOGIST_ENABLED)
            peers.push_back(friend_id);
    }
}

void FriendsConnectivityManager::getFriendList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        peers.push_back(friend_id);
    }
}

bool FriendsConnectivityManager::isFriend(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);
    return mFriendList.contains(librarymixer_id);
}

bool FriendsConnectivityManager::isOnline(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);
    if (mFriendList.contains(librarymixer_id))
        return (mFriendList[librarymixer_id].state == FCS_CONNECTED_TCP ||
                mFriendList[librarymixer_id].state == FCS_CONNECTED_UDP);
    return false;
}

QString FriendsConnectivityManager::getPeerName(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (mFriendList.contains(librarymixer_id)) return mFriendList[librarymixer_id].name;
    else return "";
}

bool FriendsConnectivityManager::addUpdateFriend(unsigned int librarymixer_id, const QString &cert, const QString &name,
                                          const QString &localIP, ushort localPort, const QString &externalIP, ushort externalPort) {
    /* Begin by preparing the addresses we have been passed outside of the mutex. */
    struct sockaddr_in localAddress;
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = htons(localPort);
    inet_aton(localIP.toStdString().c_str(), &localAddress.sin_addr);

    struct sockaddr_in externalAddress;
    externalAddress.sin_family = AF_INET;
    externalAddress.sin_port = htons(externalPort);
    inet_aton(externalIP.toStdString().c_str(), &externalAddress.sin_addr);

    QMutexLocker stack(&connMtx);

    /* First try to insert the certificate. */
    int authResult = authMgr->addUpdateCertificate(cert, librarymixer_id);

    /* Existing friend */
    if (mFriendList.contains(librarymixer_id)) {
        //Update the name
        mFriendList[librarymixer_id].name = name;
        mFriendList[librarymixer_id].id = authMgr->findCertByLibraryMixerId(librarymixer_id);
        mFriendList[librarymixer_id].localaddr = localAddress;
        mFriendList[librarymixer_id].serveraddr = externalAddress;
        //If the cert has been updated
        if (authResult >= 1) {
            if (mFriendList[librarymixer_id].state == FCS_NOT_MIXOLOGIST_ENABLED) {
                mFriendList[librarymixer_id].state = FCS_NOT_CONNECTED;
            }
            /* Note that when we update the peer, we are creating a new ConnectionToFriend while not removing its old ConnectionToFriend.
               This is because we would need to signal to AggregatedConnectionsToFriends that the old one needs to be removed and a new one
               created simultaneously, and we don't have a convenient way to do that yet. */
            mFriendList[librarymixer_id].actions = PEER_NEW;
            mStatusChanged = true;
        }
        return true;
    }
    /* New friend */
    else {
        /* Create the new entry. */
        peerConnectState pstate;

        pstate.librarymixer_id = librarymixer_id;
        pstate.name = name;
        pstate.localaddr = localAddress;
        pstate.serveraddr = externalAddress;

        /* If this is a new friend, but no cert was added. */
        if (authResult <= 0) {
            pstate.id = "";
            pstate.state = FCS_NOT_MIXOLOGIST_ENABLED;
            pstate.actions = 0;
        }
        /* Otherwise this is a successful new friend. */
        else {
            /* Should not be able to reach here with a null Cert. */
            if ((pstate.id = authMgr->findCertByLibraryMixerId(librarymixer_id)).empty()) return false;
            pstate.state = FCS_NOT_CONNECTED;
            pstate.actions = PEER_NEW;
            mStatusChanged = true;
        }
        pstate.lastcontact = 0;

        mFriendList[librarymixer_id] = pstate;

        return true;
    }
}

void FriendsConnectivityManager::tryConnectAll() {
    if (!friendsManagerEnabled) return;

    QList<unsigned int> friends_ids;
    {
        QMutexLocker stack(&connMtx);
        friends_ids = mFriendList.keys();
    }

    foreach (unsigned int librarymixer_id, friends_ids) {
        /* Connect all will always try to make a TCP connection. */
        tryConnectTCP(librarymixer_id);

        /* If we believe ourselves to be connectable via TCP, we also queue TCP connect backs after the TCP connection.
           This will contact any online firewalled friends and tell them to connect back to us. */
        switch (ownConnectivityManager->getConnectionStatus()) {
        case CONNECTION_STATUS_UNFIREWALLED:
        case CONNECTION_STATUS_PORT_FORWARDED:
        case CONNECTION_STATUS_UPNP_IN_USE:
            tryConnectBackTCP(librarymixer_id);
        default:;
        }
    }
}

