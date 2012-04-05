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

#include <services/statusservice.h>

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
#define SENT_TCP_CONNECT_BACK_WAIT_TIME 8 //8 second wait for our TCP connect back to succeed
#define LAST_HEARD_TIMEOUT 300 //5 minute wait if haven't heard from a friend before we mark them as disconnected
#define UDP_PUNCHING_PERIOD 20 //20 seconds between UDP hole punches

friendListing::friendListing()
    :name(""), id(""), librarymixer_id(0),
     lastcontact(0), lastheard(0),
     state(FCS_NOT_MIXOLOGIST_ENABLED), actions(0),
     tryTcpLocal(false), tryTcpExternal(false), tryTcpConnectBackRequest(false), tryUdp(false),
     nextTryDelayedUntil(0) {
    sockaddr_clear(&localaddr);
    sockaddr_clear(&serveraddr);
}

FriendsConnectivityManager::FriendsConnectivityManager()
    :mStatusChanged(false),
     friendsManagerEnabled(false),
     friendsListUpdateTime(0),
     outboundConnectionTryAllTime(0) {
    connect(librarymixerconnect, SIGNAL(downloadedFriends()), this, SLOT(friendsListUpdated()));
    connect(ownConnectivityManager, SIGNAL(ownConnectionReadinessChanged(bool)), this, SLOT(setEnabled(bool)));

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
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE, "Connection set up complete, beginning to look for your friends");
        tryConnectToAll();
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
       All other connections should be able to handle inbound connections, and if necessary their friends list can be updated in response to those. */
    if (!connectionStatusUdpHolePunching(ownConnectivityManager->getConnectionStatus())) return;

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
    bool retryAll = false;
    {
        QMutexLocker stack(&connMtx);

        if (!friendsManagerEnabled) return;

        time_t now = time(NULL);

        /* If applicable, do the UDP hole punching.
           We send to all friends that are disconnected, in case they are online and behind firewalls.
           We also send to all friends we are connected to via UDP, in order to be certain the firewalls don't time out our connections if we are idle.
           Note that we are treating CONNECTION_STATUS_UDP_HOLE_PUNCHING (full-cone NAT hole punching)
           the same as CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING for now.
           The full-cone NAT hole punching actually only requires punching one friend each cycle, but we are punching all friends each cycle for both. */
        if (connectionStatusUdpHolePunching(ownConnectivityManager->getConnectionStatus())) {
            static time_t last_punch = 0;
            if (now - last_punch > UDP_PUNCHING_PERIOD) {
                log(LOG_DEBUG_BASIC, FRIEND_CONNECTIVITY_ZONE, "FriendsConnectivityManager::connectivityTick() Sending UDP Tunnelers");
                last_punch = now;
                foreach (friendListing *currentFriend, friendsConnectivityManager->mFriendList.values()) {
                    if (currentFriend->state == FCS_NOT_CONNECTED) {
                        if (udpMainSocket) {
                            udpMainSocket->sendUdpTunneler(&currentFriend->serveraddr, ownConnectivityManager->getOwnExternalAddress(), peers->getOwnLibraryMixerId());
                        }
                    } else if (currentFriend->state == FCS_CONNECTED_UDP) {
                        statusService->sendKeepAlive(currentFriend->librarymixer_id);
                    }
                }
            }
        }

        time_t timeoutIfOlder = now - LAST_HEARD_TIMEOUT;

        /* Do both the timeout of connected friends, and any scheduled TCP trying now. */
        foreach (friendListing *currentFriend, mFriendList.values()) {
            if (currentFriend->state == FCS_NOT_MIXOLOGIST_ENABLED) continue;

            /* Check if connected peers need to be timed out */
            else if (currentFriend->state == FCS_CONNECTED_TCP ||
                     currentFriend->state == FCS_CONNECTED_UDP) {
                if (currentFriend->lastheard < timeoutIfOlder) {
                    log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE, QString("Connection with ") + currentFriend->name + " has timed out");

                    /* Attempt an immediate reconnect if we were TCP connected and timed out.
                       Don't bother for UDP, as the constant tunnelers pretty much act as constant connection attempts. */
                    if (currentFriend->state == FCS_CONNECTED_TCP) {
                        tryConnectTCP(currentFriend->librarymixer_id);
                        tryConnectBackTCP(currentFriend->librarymixer_id);
                    }
                    currentFriend->actions |= PEER_TIMEOUT;
                    currentFriend->lastcontact = time(NULL);
                    mStatusChanged = true;

                } else continue;
            }

            else if (currentFriend->nextTryDelayedUntil != 0 &&
                     now > currentFriend->nextTryDelayedUntil) {
                log(LOG_DEBUG_BASIC, FRIEND_CONNECTIVITY_ZONE,
                    "FriendsConnectivityManager::connectivityTick() Connectivity with " + QString::number(currentFriend->librarymixer_id) +
                    " done waiting, resuming attempts.");
                currentFriend->nextTryDelayedUntil = 0;
                informMonitorsTryConnect(currentFriend->librarymixer_id);
            }
        }

        /* Do the TCP retry with all friends. */
        if (ownConnectivityManager->getConnectionStatus() == CONNECTION_STATUS_SYMMETRIC_NAT &&
            now - outboundConnectionTryAllTime > TCP_RETRY_PERIOD_SYMMETRIC_NAT)
            retryAll = true;
        else if (now - outboundConnectionTryAllTime > TCP_RETRY_PERIOD)
            retryAll = true;
    }

#ifndef NO_AUTO_CONNECTION
    if (retryAll) {
        log(LOG_DEBUG_BASIC, FRIEND_CONNECTIVITY_ZONE, "FriendsConnectivityManager::connectivityTick() Time to retry TCP connections to all");
        tryConnectToAll();
    }
#endif //NO_AUTO_CONNECTION

}

bool FriendsConnectivityManager::tryConnectTCP(unsigned int librarymixer_id) {
    if (!mFriendList.contains(librarymixer_id)) return false;

    friendListing* currentFriend = mFriendList[librarymixer_id];

    if (currentFriend->state == FCS_CONNECTED_TCP || currentFriend->state == FCS_CONNECTED_UDP) return true;

#ifndef NO_TCP_CONNECTIONS

    /* If address is valid, on the same subnet, not the same as external address, and not the same as own addresses, add it as a local address to try. */
    if (isValidNet(&currentFriend->localaddr.sin_addr) &&
        isSameSubnet(&ownConnectivityManager->getOwnLocalAddress()->sin_addr, &currentFriend->localaddr.sin_addr) &&
        (!isSameAddress(&currentFriend->localaddr, &currentFriend->serveraddr)) &&
        (!isSameAddress(ownConnectivityManager->getOwnLocalAddress(), &currentFriend->localaddr)) &&
        (!isSameAddress(ownConnectivityManager->getOwnExternalAddress(), &currentFriend->localaddr))) {
        currentFriend->tryTcpLocal = true;
    }

    /* Always try external unless address is the same as one of ours. */
    if (isValidNet(&(currentFriend->serveraddr.sin_addr)) &&
        (!isSameAddress(ownConnectivityManager->getOwnLocalAddress(), &currentFriend->serveraddr)) &&
        (!isSameAddress(ownConnectivityManager->getOwnExternalAddress(), &currentFriend->serveraddr))) {
        currentFriend->tryTcpExternal = true;
    }

    informMonitorsTryConnect(librarymixer_id);

#endif // NO_TCP_CONNECTIONS

    return true;
}

bool FriendsConnectivityManager::tryConnectBackTCP(unsigned int librarymixer_id) {
    if (!mFriendList.contains(librarymixer_id)) return false;

    friendListing* currentFriend = mFriendList[librarymixer_id];

    if (currentFriend->state == FCS_CONNECTED_TCP ||
        currentFriend->state == FCS_CONNECTED_UDP) return true;

#ifndef NO_TCP_BACK_CONNECTIONS
    currentFriend->tryTcpConnectBackRequest = true;

    informMonitorsTryConnect(librarymixer_id);
#endif // NO_TCP_BACK_CONNECTIONS
    return true;
}

bool FriendsConnectivityManager::tryConnectUDP(unsigned int librarymixer_id) {
    if (!mFriendList.contains(librarymixer_id)) return false;

    friendListing* currentFriend = mFriendList[librarymixer_id];

    if (currentFriend->state == FCS_CONNECTED_TCP ||
        currentFriend->state == FCS_CONNECTED_UDP) return true;

#ifndef NO_UDP_CONNECTIONS
    currentFriend->tryUdp = true;

    informMonitorsTryConnect(librarymixer_id);
#endif // NO_UDP_CONNECTIONS

    return true;
}

void FriendsConnectivityManager::receivedUdpTunneler(unsigned int librarymixer_id, QString address, ushort port) {
    QMutexLocker stack(&connMtx);
    if (!friendsManagerEnabled) return;

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
            "Received a UDP packet from an unknown user with LibraryMixer ID " + QString::number(librarymixer_id) +
            ", updating friend list");
        librarymixerconnect->downloadFriends();
        return;
    }

    if (addressToString(&mFriendList[librarymixer_id]->serveraddr) != (address + ":" + QString::number(port))) {
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
            "Received a UDP packet from friend " + QString::number(librarymixer_id) + " indicating our friend list may be out of date, updating friend list");
        librarymixerconnect->downloadFriends();
        return;
    }

    log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
        QString("Received a packet indicating ") + QString::number(librarymixer_id) +
        " has punched a UDP hole in their firewall for us to connect to at address " + address + ":" + QString::number(port));

    if (connectionStatusGoodConnection(ownConnectivityManager->getConnectionStatus())) {
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
            "Do not believe ourselves to be firewalled, requesting TCP connect back");
        tryConnectBackTCP(librarymixer_id);
    }

    tryConnectUDP(librarymixer_id);
}

void FriendsConnectivityManager::receivedUdpConnectionNotice(unsigned int librarymixer_id, QString address, ushort port) {
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

    if (addressToString(&mFriendList[librarymixer_id]->serveraddr) != (address + ":" + QString::number(port))) {
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE, "Received a UDP connection notification that contains invalid data, ignoring");
        return;
    }

    log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
        QString("Received request to connect via UDP from ") + QString::number(librarymixer_id) +
        " at address " + address + ":" + QString::number(port));

    tryConnectUDP(librarymixer_id);
}

void FriendsConnectivityManager::receivedTcpConnectionRequest(unsigned int librarymixer_id, QString address, ushort port) {
    QMutexLocker stack(&connMtx);
    if (!friendsManagerEnabled) return;

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
            "Received a request to connect from an unknown user with LibraryMixer ID " + QString::number(librarymixer_id) +
            ", updating friend list");
        librarymixerconnect->downloadFriends();
        return;
    }

    if (addressToString(&mFriendList[librarymixer_id]->serveraddr) != (address + ":" + QString::number(port))) {
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
            "Received a UDP packet from friend " + QString::number(librarymixer_id) + " indicating our friend list may be out of date, updating friend list");
        librarymixerconnect->downloadFriends();
        return;
    }

    log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
        QString("Received request to connect back via TCP from ") + QString::number(librarymixer_id) +
        " at address " + address + ":" + QString::number(port));

    tryConnectTCP(librarymixer_id);
}

/**********************************************************************************
 * The interface to AggregatedConnectionsToFriends
 **********************************************************************************/

bool FriendsConnectivityManager::getQueuedConnectAttempt(unsigned int librarymixer_id, struct sockaddr_in &targetAddress, QueuedConnectionType &queuedConnectionType) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING, FRIEND_CONNECTIVITY_ZONE,
            QString("Can't make attempt to connect to user, not in friends list, id was: ").append(librarymixer_id));
        return false;
    }

    friendListing* currentFriend = mFriendList[librarymixer_id];

    /* Get a queued attempt in order of priority. */
    bool* currentTryFlag;
    if (currentFriend->tryTcpLocal) {
        currentTryFlag = &currentFriend->tryTcpLocal;
        currentFriend->tryTcpLocal = false;
        queuedConnectionType = CONNECTION_TYPE_TCP_LOCAL;
        targetAddress = currentFriend->localaddr;
    } else if (currentFriend->tryTcpExternal) {
        currentTryFlag = &currentFriend->tryTcpExternal;
        currentFriend->tryTcpExternal = false;
        queuedConnectionType = CONNECTION_TYPE_TCP_EXTERNAL;
        targetAddress = currentFriend->serveraddr;
    } else if (currentFriend->tryTcpConnectBackRequest) {
        currentTryFlag = &currentFriend->tryTcpConnectBackRequest;
        currentFriend->tryTcpConnectBackRequest = false;
        queuedConnectionType = CONNECTION_TYPE_TCP_BACK;
        targetAddress = currentFriend->serveraddr;
    } else if (currentFriend->tryUdp) {
        currentTryFlag = &currentFriend->tryUdp;
        currentFriend->tryUdp = false;
        queuedConnectionType = CONNECTION_TYPE_UDP;
        targetAddress = currentFriend->serveraddr;
    } else {
        log(LOG_ERROR, FRIEND_CONNECTIVITY_ZONE,
            QString("No connection types available for connecting to : ").append(librarymixer_id));
        return false;
    }

    /* Test if socket is in use already.
       If it is due to a mere connection attempt, set the current attempt to queued again, and just schedule a delayed try.
       If it is due to a connection, then clear out all attempts that use that socket. */
    if (usedSockets.contains(addressToString(&targetAddress))) {
        if (usedSockets[addressToString(&targetAddress)] == USED_IP_CONNECTING) {
            *currentTryFlag = true;
            currentFriend->nextTryDelayedUntil = time(NULL) + USED_SOCKET_WAIT_TIME;
        } else if (usedSockets[addressToString(&targetAddress)] == USED_IP_CONNECTED) {
            if (queuedConnectionType == CONNECTION_TYPE_TCP_LOCAL) {
                currentFriend->tryTcpLocal = false;
            } else {
                currentFriend->tryTcpExternal = false;
                currentFriend->tryTcpConnectBackRequest = false;
                currentFriend->tryUdp = false;
            }
            informMonitorsTryConnect(librarymixer_id);
        }
        return false;
    }

    currentFriend->state = FCS_IN_CONNECT_ATTEMPT;
    currentFriend->currentlyTrying = queuedConnectionType;

    usedSockets[addressToString(&targetAddress)] = USED_IP_CONNECTING;

    log(LOG_DEBUG_BASIC, FRIEND_CONNECTIVITY_ZONE,
        QString("FriendsConnectivityManager::connectAttempt Providing information for connection attempt to user: ").append(currentFriend->name));

    return true;
}

/* A note about why we request type and remoteAddress as arguments:
   It may be tempting to think we can just pull that information from currentlyTrying, but that is only the outbound connection try, and fails for inbound. */
bool FriendsConnectivityManager::reportConnectionUpdate(unsigned int librarymixer_id, int result, ConnectionType type, struct sockaddr_in *remoteAddress) {
    bool signalFriendsChanged = false;

    {
        QMutexLocker stack(&connMtx);

        if (!mFriendList.contains(librarymixer_id)) return false;

        friendListing* currentFriend = mFriendList[librarymixer_id];

        if (result == 1) {
            /* No longer need any of the queued connection attempts, reset them all. */
            currentFriend->tryTcpLocal = false;
            currentFriend->tryTcpExternal = false;
            currentFriend->tryTcpConnectBackRequest = false;
            currentFriend->tryUdp = false;
            currentFriend->nextTryDelayedUntil = 0;

            /* Mark this socket as used so no other connection attempt can try to use it. */
            usedSockets[addressToString(remoteAddress)] = USED_IP_CONNECTED;

            log(LOG_DEBUG_BASIC, FRIEND_CONNECTIVITY_ZONE, QString("Successfully connected to: ") + currentFriend->name);

            /* Change state.
               We can't simply have done without the type argument and used currentlyTrying because currentlyTrying isn't set for incoming. */
            if (type == TCP_CONNECTION) currentFriend->state = FCS_CONNECTED_TCP;
            else if (type == UDP_CONNECTION) currentFriend->state = FCS_CONNECTED_UDP;

            currentFriend->actions |= PEER_CONNECTED;
            mStatusChanged = true;
            currentFriend->lastcontact = time(NULL);
            currentFriend->lastheard = time(NULL);
            signalFriendsChanged = true;
        } else {
            log(LOG_DEBUG_BASIC,
                FRIEND_CONNECTIVITY_ZONE,
                QString("Connection failure with friend: ") + currentFriend->name + ", over transport layer type: " + QString::number(type));

            /* We can receive failure reports from either connected friends, or connection attempt friends. */
            if (currentFriend->state == FCS_CONNECTED_TCP ||
                currentFriend->state == FCS_CONNECTED_UDP) {
                currentFriend->lastcontact = time(NULL);
                usedSockets.remove(addressToString(remoteAddress));
                signalFriendsChanged = true;
            } else {
                /* The idea here is that we want to remove the hold we have on the socket from usedSockets.
                   However, an inbound connection could have snuck in and taken this over as a connected address, so we only remove if it looks like ours. */
                if (usedSockets[addressToString(remoteAddress)] == USED_IP_CONNECTING) usedSockets.remove(addressToString(remoteAddress));

                /* If we just sent a TCP connect back request, give it some time to work before doing anything else. */
                if (currentFriend->currentlyTrying == CONNECTION_TYPE_TCP_BACK)
                    currentFriend->nextTryDelayedUntil = time(NULL) + SENT_TCP_CONNECT_BACK_WAIT_TIME;

                /* If we have been requested to schedule another TCP connection attempt. */
                if (result == 0) {
                    currentFriend->nextTryDelayedUntil = time(NULL) + REQUESTED_RETRY_WAIT_TIME;
                    if (currentFriend->currentlyTrying == CONNECTION_TYPE_TCP_LOCAL)
                        currentFriend->tryTcpLocal = true;
                    else if (currentFriend->currentlyTrying == CONNECTION_TYPE_TCP_EXTERNAL ||
                             currentFriend->currentlyTrying == CONNECTION_TYPE_TCP_BACK)
                        currentFriend->tryTcpExternal = true;
                }
            }

            currentFriend->state = FCS_NOT_CONNECTED;

            /* See if there are any pending connection attempts that need to be triggered now. */
            informMonitorsTryConnect(librarymixer_id);
        }
    }

    if (signalFriendsChanged) emit friendsChanged();

    return true;
}

/**********************************************************************************
 * MixologistLib functions, not shared via interface with GUI
 **********************************************************************************/

friendListing* FriendsConnectivityManager::getFriendListing(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    return mFriendList[librarymixer_id];
}

void FriendsConnectivityManager::heardFrom(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);
    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_ERROR, FRIEND_CONNECTIVITY_ZONE, QString("Somehow heard from someone that is not a friend: ").append(librarymixer_id));
        return;
    }
    mFriendList[librarymixer_id]->lastheard = time(NULL);
}

void FriendsConnectivityManager::getExternalAddresses(QList<struct sockaddr_in> &toFill) {
    QMutexLocker stack(&connMtx);
    foreach (friendListing* currentFriend, mFriendList.values()) {
        toFill.append(currentFriend->serveraddr);
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
    QMap<unsigned int, friendListing>::iterator it;

    {
        QMutexLocker stack(&connMtx);

        if (mStatusChanged) {
            mStatusChanged = false;
            doStatusChange = true;

            foreach (friendListing *currentFriend, mFriendList.values()) {
                if (currentFriend->actions) {
                    /* add in */
                    pqipeer peer;
                    peer.librarymixer_id = currentFriend->librarymixer_id;
                    peer.cert_id = currentFriend->id;
                    peer.name = currentFriend->name;
                    peer.state = currentFriend->state;
                    peer.actions = currentFriend->actions;

                    /* reset action */
                    currentFriend->actions = 0;

                    actionList.push_back(peer);

                    /* notify GUI */
                    if (peer.actions & PEER_CONNECTED) {
                        pqiNotify *notify = getPqiNotify();
                        if (notify) {
                            notify->AddPopupMessage(POPUP_CONNECT, QString::number(currentFriend->librarymixer_id), "Online: ");
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

void FriendsConnectivityManager::informMonitorsTryConnect(unsigned int librarymixer_id) {
    if (!mFriendList.contains(librarymixer_id)) return;

    friendListing *currentFriend = mFriendList[librarymixer_id];

    if (currentFriend->state == FCS_IN_CONNECT_ATTEMPT) return;

    if (currentFriend->nextTryDelayedUntil != 0) return;

    if (!currentFriend->tryTcpLocal &&
        !currentFriend->tryTcpExternal &&
        !currentFriend->tryTcpConnectBackRequest &&
        !currentFriend->tryUdp) return;

    currentFriend->actions |= PEER_CONNECT_REQ;
    mStatusChanged = true;
}

/**********************************************************************************
 * Body of public-facing API functions called through p3peers
 **********************************************************************************/
void FriendsConnectivityManager::getOnlineList(QList<unsigned int> &friend_ids) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        if (mFriendList[friend_id]->state == FCS_CONNECTED_TCP ||
            mFriendList[friend_id]->state == FCS_CONNECTED_UDP)
            friend_ids.append(friend_id);
    }
}

void FriendsConnectivityManager::getSignedUpList(QList<unsigned int> &friend_ids) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        if (mFriendList[friend_id]->state != FCS_NOT_MIXOLOGIST_ENABLED)
            friend_ids.append(friend_id);
    }
}

void FriendsConnectivityManager::getFriendList(QList<unsigned int> &friend_ids) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        friend_ids.append(friend_id);
    }
}

bool FriendsConnectivityManager::isFriend(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);
    return mFriendList.contains(librarymixer_id);
}

bool FriendsConnectivityManager::isOnline(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);
    if (mFriendList.contains(librarymixer_id))
        return (mFriendList[librarymixer_id]->state == FCS_CONNECTED_TCP ||
                mFriendList[librarymixer_id]->state == FCS_CONNECTED_UDP);
    return false;
}

QString FriendsConnectivityManager::getPeerName(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (mFriendList.contains(librarymixer_id)) return mFriendList[librarymixer_id]->name;
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

    bool signalFriendsChanged = false;
    {
        QMutexLocker stack(&connMtx);

        /* First try to insert the certificate. */
        int authResult = authMgr->addUpdateCertificate(cert, librarymixer_id);

        /* Existing friend */
        if (mFriendList.contains(librarymixer_id)) {
            //Update the name
            mFriendList[librarymixer_id]->name = name;
            mFriendList[librarymixer_id]->id = authMgr->findCertByLibraryMixerId(librarymixer_id);
            mFriendList[librarymixer_id]->localaddr = localAddress;
            mFriendList[librarymixer_id]->serveraddr = externalAddress;
            //If the cert has been updated
            if (authResult >= 1) {
                if (mFriendList[librarymixer_id]->state == FCS_NOT_MIXOLOGIST_ENABLED) {
                    mFriendList[librarymixer_id]->state = FCS_NOT_CONNECTED;
                }
                mFriendList[librarymixer_id]->actions = PEER_CERT_AND_ADDRESS_UPDATED;
                mStatusChanged = true;
            }
        }
        /* New friend */
        else {
            friendListing *newFriend = new friendListing();

            newFriend->librarymixer_id = librarymixer_id;
            newFriend->name = name;
            newFriend->localaddr = localAddress;
            newFriend->serveraddr = externalAddress;

            /* If this is a new friend, but no cert was added. */
            if (authResult > 0) {
                /* Should not be able to reach here with a null Cert. */
                if ((newFriend->id = authMgr->findCertByLibraryMixerId(librarymixer_id)).empty()) return false;
                newFriend->state = FCS_NOT_CONNECTED;
                newFriend->actions = PEER_NEW;
                mStatusChanged = true;
            }

            mFriendList[librarymixer_id] = newFriend;

            signalFriendsChanged = true;
        }
    }

    if (signalFriendsChanged) emit friendsChanged();

    return true;
}

void FriendsConnectivityManager::tryConnectToAll() {
    QMutexLocker stack(&connMtx);

    if (!friendsManagerEnabled) return;

    outboundConnectionTryAllTime = time(NULL);

    foreach (unsigned int librarymixer_id, mFriendList.keys()) {
        /* Connect all will always try to make a TCP connection. */
        tryConnectTCP(librarymixer_id);

        /* If we believe ourselves to be connectable via TCP, we also queue TCP connect backs after the TCP connection.
           This will contact any online firewalled friends and tell them to connect back to us. */
        if (connectionStatusGoodConnection(ownConnectivityManager->getConnectionStatus()))
            tryConnectBackTCP(librarymixer_id);
    }
}
