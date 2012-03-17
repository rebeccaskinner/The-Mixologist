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
#include "tcponudp/stunbasics.h"

#include <upnp/upnphandler.h>

#include <pqi/connectivitymanager.h>
#include <pqi/pqinotify.h>

#include <interface/peers.h>
#include <interface/settings.h>
#include <interface/librarymixer-connect.h>

#include <util/debug.h>


/****
 * #define NO_TCP_CONNECTIONS 1
 ***/
/****
 * #define NO_AUTO_CONNECTION 1
 ***/

#define MIN_RETRY_PERIOD 600 //10 minute wait between automatic connection retries
#define USED_IP_WAIT_TIME 5 //5 second wait if tried to connect to an IP that was already in use in a connection attempt
#define LAST_HEARD_TIMEOUT 300 //5 minute wait if haven't heard from a friend before we mark them as disconnected
#define UPNP_INIT_TIMEOUT 10 //10 second timeout for UPNP to initialize the firewall
#define STUN_TEST_TIMEOUT 3 //3 second timeout from a STUN request for a STUN response to be received

peerConnectAddress::peerConnectAddress()
    :delay(0), period(0), type(0) {
    sockaddr_clear(&addr);
}

peerConnectState::peerConnectState()
    :name(""),
     id(""),
     lastcontact(0),
     lastattempt(0),
     doubleTried(false),
     schedulednexttry(0),
     state(0), actions(0),
     inConnAttempt(0) {
    sockaddr_clear(&localaddr);
    sockaddr_clear(&serveraddr);
}

ConnectivityManager::ConnectivityManager()
    :mStatusChanged(false),
     connectionStatus(CONNECTION_STATUS_FINDING_STUN_FRIENDS),
     connectionSetupStepTimeOutAt(0),
     udpTestSocket(NULL), udpMainSocket(NULL), mUpnpMgr(NULL),
     stunServer1(NULL), stunServer2(NULL),
     readyToConnectToFriends(CONNECT_FRIENDS_CONNECTION_INITIALIZING) {

    /* Setup basics of own state.
       authMgr must have been initialized before connMgr. */
    ownState.id = authMgr->OwnCertId();
    ownState.librarymixer_id = authMgr->OwnLibraryMixerId();

    connect(librarymixerconnect, SIGNAL(uploadedAddress()), this, SLOT(addressUpdatedOnLibraryMixer()));

    fallbackPublicStunServers.append("stun.selbie.com");
    fallbackPublicStunServers.append("stun.ipns.com");
}

void ConnectivityManager::tick() {
    netTick();
    statusTick();
    monitorsTick();
}

void ConnectivityManager::netTick() {
    QMutexLocker stack(&connMtx);
    switch (connectionStatus) {
    case CONNECTION_STATUS_FINDING_STUN_FRIENDS:
        if (connectionSetupStepTimeOutAt == 0) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Initiating automatic detection of connection status");
            foreach (unsigned int librarymixer_id, mFriendList.keys()) {
                if (stunServer1 && stunServer2) break;
                sendStunPacket(peers->getPeerName(librarymixer_id), &mFriendList[librarymixer_id].serveraddr, udpTestSocket);
            }
            connectionSetupStepTimeOutAt = time(NULL) + STUN_TEST_TIMEOUT;
        } else if (time(NULL) > connectionSetupStepTimeOutAt) {
            setNewConnectionStatus(CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS);
        }
        break;
    case CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS:
        if (connectionSetupStepTimeOutAt == 0) {
            /* We will try one server per tick from our fallback public STUN server list to avoid unnecessary traffic on them.
               tick is called from the main server thread at a rate of once per second. */
            if (!fallbackPublicStunServers.isEmpty()) {
                QString currentServer = fallbackPublicStunServers.front();
                fallbackPublicStunServers.removeFirst();

                struct sockaddr_in* currentAddress = new sockaddr_in;
                sockaddr_clear(currentAddress);
                currentAddress->sin_port = htons(3478);
                if (!LookupDNSAddr(currentServer.toStdString(), currentAddress)) {
                    getPqiNotify()->AddSysMessage(SYS_WARNING, "Network failure", "Unable to lookup server address " + currentServer);
                }
                sendStunPacket(currentServer, currentAddress, udpTestSocket);

            }
            /* We are out of servers to try, wait for the last STUN to timeout before fail. */
            else {
                connectionSetupStepTimeOutAt = time(NULL) + STUN_TEST_TIMEOUT;
            }
        } else if (time(NULL) > connectionSetupStepTimeOutAt) {
            /* If we can't get enough STUN servers to configure just give up and assume the connection is already configured. */
            setNewConnectionStatus(CONNECTION_STATUS_NO_NET);
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Unable to find two STUN servers with which to test our connection state. Automatic connection set up disabled.");
        }
        break;
    case CONNECTION_STATUS_STUNNING_INITIAL:
        if (!handleStunStep("Primary STUN server", stunServer1, udpTestSocket, ntohs(ownState.localaddr.sin_port))) {
            setNewConnectionStatus(CONNECTION_STATUS_TRYING_UPNP);
        }
        break;
    case CONNECTION_STATUS_TRYING_UPNP:
        if (!mUpnpMgr) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Attempting to configure firewall using Universal Plug and Play");
            mUpnpMgr = new upnpHandler();
            mUpnpMgr->setTargetPort(ntohs(ownState.localaddr.sin_port));
            mUpnpMgr->startup();
            connectionSetupStepTimeOutAt = time(NULL) + UPNP_INIT_TIMEOUT;
        }

        {
            upnpHandler::upnpStates currentUpnpState = mUpnpMgr->getUpnpState();
            if (currentUpnpState == upnpHandler::UPNP_STATE_ACTIVE) {
                setNewConnectionStatus(CONNECTION_STATUS_STUNNING_UPNP_TEST);
                log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                    "Universal Plug and Play successfully configured router, testing connection");
            } else {
                if (currentUpnpState == upnpHandler::UPNP_STATE_UNAVAILABLE ||
                    time(NULL) > connectionSetupStepTimeOutAt) {
                    mUpnpMgr->shutdown();
                    delete mUpnpMgr;
                    mUpnpMgr = NULL;
                    setNewConnectionStatus(CONNECTION_STATUS_STUNNING_MAIN_PORT);
                    if (currentUpnpState == upnpHandler::UPNP_STATE_UNAVAILABLE) {
                        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                            "No Universal Plug and Play-compatible router detected");
                    } else {
                        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                            "Unable to configure router using Universal Plug and Play");
                    }
                }
            }
        }
        break;
    case CONNECTION_STATUS_STUNNING_UPNP_TEST:
        if (!handleStunStep("Primary STUN Server", stunServer1, udpTestSocket, ntohs(ownState.localaddr.sin_port))) {
            mUpnpMgr->shutdown();
            delete mUpnpMgr;
            mUpnpMgr = NULL;
            setNewConnectionStatus(CONNECTION_STATUS_STUNNING_MAIN_PORT);
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Connection test failed despite successfully configured connection, abandoning Universal Plug and Play attempt");
        }
        break;
    case CONNECTION_STATUS_STUNNING_MAIN_PORT:
        if (!handleStunStep("Secondary STUN Server", stunServer2, udpMainSocket)) {
            setNewConnectionStatus(CONNECTION_STATUS_NO_NET);
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Total failure to access the Internet using Mixologist port " + QString::number(ntohs(ownState.localaddr.sin_port)));
        }
        break;
    case CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST:
        if (!handleStunStep("Primary STUN Server", stunServer1, udpTestSocket, ntohs(ownState.localaddr.sin_port))) {
            setNewConnectionStatus(CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST);
        }
        break;
    case CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST:
        if (!handleStunStep("Primary STUN Server", stunServer1, udpMainSocket)) {
            setNewConnectionStatus(CONNECTION_STATUS_NO_NET);
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Total failure to access the Internet using Mixologist port " + QString::number(ntohs(ownState.localaddr.sin_port)));
        }
        break;
    case CONNECTION_STATUS_UPNP_IN_USE:
        netUpnpMaintenance();
        break;
    case CONNECTION_STATUS_UDP_HOLE_PUNCHING:
        break;
    case CONNECTION_STATUS_RESTRICTED_CONE:
        break;
    case CONNECTION_STATUS_UNFIREWALLED:
    case CONNECTION_STATUS_PORT_FORWARDED:
    case CONNECTION_STATUS_SYMMETRIC_NAT:
    case CONNECTION_STATUS_NO_NET:
        break;
    }

#ifdef false
    switch (netStatus) {
        case NET_UNKNOWN:
            netStartup();
            break;
        case NET_UPNP_SETUP:
            netUpnpCheck();
            break;
        case NET_UDP_SETUP:
            netUdpCheck();
            break;
        case NET_DONE:
            stunCheck(); /* Keep on stunning until its happy */
            netUpnpMaintenance();
        default:
            break;
    }
#endif
}

void ConnectivityManager::statusTick() {
    QList<unsigned int> retryIds;
    {
        QMutexLocker stack(&connMtx);

        if (readyToConnectToFriends == CONNECT_FRIENDS_CONNECTION_INITIALIZING) return;
        if (readyToConnectToFriends == CONNECT_FRIENDS_UPDATE_LIBRARYMIXER) {
            librarymixerconnect->uploadAddress(inet_ntoa(ownState.localaddr.sin_addr), ntohs(ownState.localaddr.sin_port),
                                               inet_ntoa(ownState.serveraddr.sin_addr), ntohs(ownState.serveraddr.sin_port));
            return;
        }

        time_t now = time(NULL);
        time_t retryIfOlder = now - MIN_RETRY_PERIOD;
        time_t timeoutIfOlder = now - LAST_HEARD_TIMEOUT;

        /* Can't simply use mFriendList.values() in foreach because that generates copies of the peerConnectState,
           and we need to be able to save in the changes. */
        foreach (unsigned int librarymixer_id, mFriendList.keys()) {
            if (mFriendList[librarymixer_id].state & PEER_S_NO_CERT) {
                continue;
            }
            /* Check if connected peers need to be timed out */
            else if (mFriendList[librarymixer_id].state & PEER_S_CONNECTED) {
                if (mFriendList[librarymixer_id].lastheard < timeoutIfOlder) {
                    log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, QString("Connection with ") + mFriendList[librarymixer_id].name + " has timed out");

                    mFriendList[librarymixer_id].state &= (~PEER_S_CONNECTED);
                    mFriendList[librarymixer_id].actions |= PEER_TIMEOUT;
                    mFriendList[librarymixer_id].lastcontact = time(NULL);

                    /* Attempt an immediate reconnect. */
                    /* Initialize double try information, as this will be our first of the two tries. */
                    mFriendList[librarymixer_id].doubleTried = false;
                    mFriendList[librarymixer_id].schedulednexttry = 0;

                    retryIds.push_back(librarymixer_id);
                } else continue;
            }
            /* If last attempt was long enough ago, start a whole new attempt. */
            else if (mFriendList[librarymixer_id].lastattempt < retryIfOlder) {
                /* Initialize double try information, as this will be our first of the two tries. */
                mFriendList[librarymixer_id].doubleTried = false;
                mFriendList[librarymixer_id].schedulednexttry = 0;

                retryIds.push_back(librarymixer_id);
            }
            /* If we have a scheduled second try then start it. */
            else if (mFriendList[librarymixer_id].schedulednexttry != 0 && mFriendList[librarymixer_id].schedulednexttry < now) {
                /* We are performing the scheduled next try now, so clear it out. */
                mFriendList[librarymixer_id].schedulednexttry = 0;

                retryIds.push_back(librarymixer_id);
            }
        }
    }

#ifndef NO_AUTO_CONNECTION

    foreach (unsigned int librarymixer_id, retryIds) {
        retryConnectTCP(librarymixer_id);
    }

#endif

}

void ConnectivityManager::monitorsTick() {
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
 * Setup
 **********************************************************************************/

void ConnectivityManager::addMonitor(pqiMonitor *mon) {
    QMutexLocker stack(&connMtx);
    if (monitorListeners.contains(mon)) {
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Internal error", "Initialization after already initialized");
        return;
    }
    monitorListeners.push_back(mon);
}

void ConnectivityManager::connectionSetup() {
    QMutexLocker stack(&connMtx);

    /* Set the network interface we will use for the Mixologist. */
    ownState.localaddr.sin_addr = getPreferredInterface();
    ownState.localaddr.sin_port = htons(getLocalPort());
    ownState.localaddr.sin_family = AF_INET;
    ownState.serveraddr.sin_family = AF_INET;

    /* We will open the udpTestSocket on a random port on our chosen interface. */
    struct sockaddr_in udpTestAddress = ownState.localaddr;
    udpTestAddress.sin_port = htons(getRandomPortNumber());

    int triesRemaining = 10;
    while (!udpTestSocket && triesRemaining > 0) {
        udpTestSocket = new UdpSorter(udpTestAddress);
        if (!udpTestSocket->okay()) {
            delete udpTestSocket;
            udpTestSocket = NULL;
            udpTestAddress.sin_port = htons(getRandomPortNumber());
            triesRemaining--;
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, "Unable to open UDP port " + QString::number(ntohs(udpTestAddress.sin_port)));
            continue;
        } else {
           log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, "Opened UDP test port on " + QString::number(ntohs(udpTestAddress.sin_port)));
           connect(udpTestSocket, SIGNAL(receivedStunBindingResponse(QString,QString,int,ushort)),
                   this, SLOT(receivedStunPacket(QString,QString,int,ushort)));
       }
    }
    if (!udpTestSocket) {
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Network failure", "Unable to open a UDP port");
        exit(1);
    }

    udpMainSocket = new UdpSorter(ownState.localaddr);
    if (!udpMainSocket->okay()) {
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Network failure", "Unable to open main UDP port");
        exit(1);
    } else {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, "Opened UDP main port on " + QString::number(ntohs(ownState.localaddr.sin_port)));
        connect(udpMainSocket, SIGNAL(receivedStunBindingResponse(QString,QString,int,ushort)),
                this, SLOT(receivedStunPacket(QString,QString,int,ushort)));
    }
}

bool ConnectivityManager::shutdown() { /* blocking shutdown call */
    QMutexLocker stack(&connMtx);
    if (mUpnpMgr) mUpnpMgr->shutdown();

    return true;
}

void ConnectivityManager::setNewConnectionStatus(int newStatus) {
    connectionStatus = (ConnectionStatus) newStatus;
    connectionSetupStepTimeOutAt = 0;
    pendingStunTransactions.clear();
}

bool ConnectivityManager::handleStunStep(const QString& stunServerName, const sockaddr_in *stunServer, UdpSorter *sendSocket, ushort returnPort) {
    if (connectionSetupStepTimeOutAt != 0) {
        if (time(NULL) > connectionSetupStepTimeOutAt) return false;
    } else {
        if (sendStunPacket(stunServerName, stunServer, sendSocket, returnPort)) connectionSetupStepTimeOutAt = time(NULL) + STUN_TEST_TIMEOUT;
    }
    return true;
}

bool ConnectivityManager::sendStunPacket(const QString& stunServerName, const sockaddr_in *stunServer, UdpSorter *sendSocket, ushort returnPort) {
    QString stunTransactionId = UdpStun_generate_transaction_id();
    pendingStunTransaction transaction;
    transaction.returnPort = returnPort;
    transaction.serverAddress = *stunServer;
    transaction.serverName = stunServerName;
    pendingStunTransactions[stunTransactionId] = transaction;
    return sendSocket->sendStunBindingRequest(stunServer, stunTransactionId, returnPort);
}

void ConnectivityManager::receivedStunPacket(QString transactionId, QString mappedAddress, int mappedPort, ushort receivedOnPort) {
    QMutexLocker stack(&connMtx);
    if (!pendingStunTransactions.contains(transactionId)) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, QString("Discarding STUN response not matching any current outstanding request."));
        return;
    }

    /* For the error of receiving on the wrong port, it should be generally a safe assumption the only time this could occur
       would be if the STUN server ignores the response-port attribute.
       If the expected port is set to 0, then we can assume that this is on the correct port.
       Furthermore, we know this will be stunServer1, because stunServer2 is never sent any requests with the response-port attribute. */
    if (pendingStunTransactions[transactionId].returnPort != receivedOnPort &&
        pendingStunTransactions[transactionId].returnPort != 0) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
            "Error in STUN, STUN server (" + QString(inet_ntoa(stunServer1->sin_addr)) + ") ignored the response-port attribute.");
        return;
    }

    if (connectionStatus == CONNECTION_STATUS_FINDING_STUN_FRIENDS) {
        struct sockaddr_in** targetStunServer;
        if (!stunServer1) targetStunServer = &stunServer1;
        else if (!stunServer2) targetStunServer = &stunServer2;
        else return;

        *targetStunServer = new sockaddr_in;
        **targetStunServer = pendingStunTransactions[transactionId].serverAddress;

        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
            "Using " + pendingStunTransactions[transactionId].serverName + " as a STUN server");

        if (stunServer2) setNewConnectionStatus(CONNECTION_STATUS_STUNNING_INITIAL);
    } else if (connectionStatus == CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS) {
        struct sockaddr_in** targetStunServer;
        if (!stunServer1) targetStunServer = &stunServer1;
        else if (!stunServer2) targetStunServer = &stunServer2;
        else return;

        *targetStunServer = new sockaddr_in;
        **targetStunServer = pendingStunTransactions[transactionId].serverAddress;

        if (targetStunServer == &stunServer1) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Using fallback public server " + pendingStunTransactions[transactionId].serverName + " as the primary STUN server");
        } else {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Using fallback public server " + pendingStunTransactions[transactionId].serverName + " as the secondary STUN server");
        }

        if (stunServer2) setNewConnectionStatus(CONNECTION_STATUS_STUNNING_INITIAL);
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_INITIAL) {
        inet_aton(mappedAddress.toStdString().c_str(), &ownState.serveraddr.sin_addr);
        ownState.serveraddr.sin_port = ownState.localaddr.sin_port;
        readyToConnectToFriends = CONNECT_FRIENDS_UPDATE_LIBRARYMIXER;
        if (QString(inet_ntoa(ownState.localaddr.sin_addr)) == mappedAddress) {
            setNewConnectionStatus(CONNECTION_STATUS_UNFIREWALLED);
            log(LOG_WARNING,
                CONNECTIVITY_MANAGER_ZONE,
                "No firewall detected, connection ready to go on " + mappedAddress +
                ":" + QString::number(ntohs(ownState.serveraddr.sin_port)));
        } else {
            setNewConnectionStatus(CONNECTION_STATUS_PORT_FORWARDED);
            log(LOG_WARNING,
                CONNECTIVITY_MANAGER_ZONE,
                "Detected firewall with port-forwarding already set up, connection ready to go on " + mappedAddress +
                ":" + QString::number(ntohs(ownState.serveraddr.sin_port)));
        }
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_UPNP_TEST) {
        inet_aton(mappedAddress.toStdString().c_str(), &ownState.serveraddr.sin_addr);
        ownState.serveraddr.sin_port = ownState.localaddr.sin_port;
        readyToConnectToFriends = CONNECT_FRIENDS_UPDATE_LIBRARYMIXER;
        setNewConnectionStatus(CONNECTION_STATUS_UPNP_IN_USE);
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            "Connection test successful, Universal Plug and Play configured connection ready to go on " + mappedAddress +
            ":" + QString::number(ntohs(ownState.serveraddr.sin_port)));
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_MAIN_PORT) {
        inet_aton(mappedAddress.toStdString().c_str(), &ownState.serveraddr.sin_addr);
        ownState.serveraddr.sin_port = htons(mappedPort);
        readyToConnectToFriends = CONNECT_FRIENDS_UPDATE_LIBRARYMIXER;
        setNewConnectionStatus(CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST);
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST) {
        setNewConnectionStatus(CONNECTION_STATUS_UDP_HOLE_PUNCHING);
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            "Connection test successful, punching a UDP hole in the firewall on " + mappedAddress +
            ":" + QString::number(ntohs(ownState.serveraddr.sin_port)));
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST) {
        if (mappedPort == ntohs(ownState.serveraddr.sin_port)) {
            setNewConnectionStatus(CONNECTION_STATUS_RESTRICTED_CONE);
            log(LOG_WARNING,
                CONNECTIVITY_MANAGER_ZONE,
                "Restricted cone firewall detected, punching a UDP hole (with reduced efficacy) in the firewall on " + mappedAddress +
                ":" + QString::number(ntohs(ownState.serveraddr.sin_port)));
        } else {
            setNewConnectionStatus(CONNECTION_STATUS_SYMMETRIC_NAT);
            log(LOG_WARNING,
                CONNECTIVITY_MANAGER_ZONE,
                "Symmetric NAT firewall detected, manual configuration of the firewall will be necessary to connect to friends over the Internet.");
        }
    }

    pendingStunTransactions.remove(transactionId);
}

void ConnectivityManager::addressUpdatedOnLibraryMixer() {
    QMutexLocker stack(&connMtx);
    readyToConnectToFriends = CONNECT_FRIENDS_READY;
}

#define UPNP_MAINTENANCE_INTERVAL 300
void ConnectivityManager::netUpnpMaintenance() {
    /* We only need to maintain if we're using UPNP,
       and this variable is only set true after UPNP is successfully configured. */
    static time_t last_call;
    if (time(NULL) - last_call > UPNP_MAINTENANCE_INTERVAL) {
        last_call = time(NULL);
        if (mUpnpMgr->getUpnpState() == upnpHandler::UPNP_STATE_FAILED) {
            mUpnpMgr->restart();
            /* Return netTick loop to waiting for connection to be setup.
               Reset the clock so it has another full time-out to fix the connection. */
            setNewConnectionStatus(CONNECTION_STATUS_TRYING_UPNP);
            connectionSetupStepTimeOutAt = time(NULL) + UPNP_INIT_TIMEOUT;
        }
    }
}

bool ConnectivityManager::getPeerConnectState(unsigned int librarymixer_id, peerConnectState &state) {
    QMutexLocker stack(&connMtx);

    if (librarymixer_id == ownState.librarymixer_id) {
        state = ownState;
    } else {
        if (!mFriendList.contains(librarymixer_id)) return false;
        state = mFriendList[librarymixer_id];
    }
    return true;
}

bool ConnectivityManager::getQueuedConnectAttempt(unsigned int librarymixer_id, struct sockaddr_in &addr, uint32_t &delay, uint32_t &period, uint32_t &type) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            QString("Can't make attempt to connect to user, not in friends list, certificate id was: ").append(librarymixer_id));
        return false;
    }

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    if (connectState->connAddrs.size() < 1) {
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            QString("Can't make attempt to connect to user, have no IP address: ").append(connectState->name));
        return false;
    }

    connectState->lastattempt = time(NULL);

    peerConnectAddress address = connectState->connAddrs.front();
    connectState->connAddrs.pop_front();

    /* Test if address is in use already. */
    QMap<QString, UsedIPState>::iterator ipit;
    ipit = usedIps.find(addressToString(address.addr));
    /* If that IP is already used:
           if there are other addresses put this one on the back and wait for the next try which will use a different address
           if there are no other addresses:
               if it's already used in a successful connection, stop trying with this address and return false
               if it's only being used for an attempted connection, put it on the back and set a special retry timer to try again soon. */
    if (ipit != usedIps.end()) {
        if (connectState->connAddrs.size() >= 1) {
            connectState->connAddrs.push_back(address);
            connectState->actions |= PEER_CONNECT_REQ;
            return false;
        } else {
            if (ipit.value() == USED_IP_CONNECTED) {
                log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE, "ConnectivityManager::connectAttempt Can not connect to " + addressToString(address.addr) + " due to existing connection.");
            } else if (ipit.value() == USED_IP_CONNECTING) {
                connectState->schedulednexttry = time(NULL) + USED_IP_WAIT_TIME;
                log(LOG_DEBUG_BASIC, CONNECTIVITY_MANAGER_ZONE, "ConnectivityManager::connectAttempt Waiting to try to connect to " + addressToString(address.addr) + " due to existing attempted connection.");
            }
            return false;
        }
    } else {
        /* If we get here, the IP address is good to use, so load it in. */
        connectState->inConnAttempt = true;
        connectState->currentConnAddr = address;
        usedIps[addressToString(address.addr)] = USED_IP_CONNECTING;

        addr = connectState->currentConnAddr.addr;
        delay = connectState->currentConnAddr.delay;
        period = connectState->currentConnAddr.period;
        type = connectState->currentConnAddr.type;

        log(LOG_DEBUG_BASIC,
            CONNECTIVITY_MANAGER_ZONE,
            QString("ConnectivityManager::connectAttempt Returning information for connection attempt to user: ").append(connectState->name));

        return true;
    }
}

bool ConnectivityManager::reportConnectionUpdate(unsigned int librarymixer_id, bool success, uint32_t flags) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            QString("Failed to connect to user, not in friends list, friend number was: ").append(QString::number(librarymixer_id)));
        return false;
    }

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    connectState->inConnAttempt = false;

    if (success) {
        /* remove other attempts */
        connectState->connAddrs.clear();
        //mDhtMgr->dropPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));
        usedIps[addressToString(connectState->currentConnAddr.addr)] = USED_IP_CONNECTED;

        /* update address (will come through from DISC) */

        log(LOG_DEBUG_BASIC,
            CONNECTIVITY_MANAGER_ZONE,
            QString("Successfully connected to: ") + connectState->name +
            " (" + inet_ntoa(connectState->currentConnAddr.addr.sin_addr) + ")");

        /* change state */
        connectState->state |= PEER_S_CONNECTED;
        connectState->actions |= PEER_CONNECTED;
        mStatusChanged = true;
        connectState->lastcontact = time(NULL);
        connectState->lastheard = time(NULL);
        //connectState->connecttype = flags;

        return true;
    }

    log(LOG_DEBUG_BASIC,
        CONNECTIVITY_MANAGER_ZONE,
        QString("Unable to connect to friend: ") + connectState->name +
        ", flags: " + QString::number(flags));

    usedIps.remove(addressToString(connectState->currentConnAddr.addr));

    /* if currently connected -> flag as failed */
    if (connectState->state & PEER_S_CONNECTED) {
        connectState->state &= (~PEER_S_CONNECTED);
        connectState->actions |= PEER_DISCONNECTED;

        connectState->lastcontact = time(NULL);  /* time of disconnect */

        //mDhtMgr->findPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));
    }

    /* If there are no addresses left to try, we're done. */
    if (connectState->connAddrs.size() < 1) {
        if (connectState->doubleTried) return true;
        else {
            connectState->doubleTried = true;
            connectState->schedulednexttry = time(NULL) + USED_IP_WAIT_TIME;
            return true;
        }
    }

    /* Otherwise flag for additional attempts. */
    connectState->actions |= PEER_CONNECT_REQ;
    mStatusChanged = true;

    return true;
}

void ConnectivityManager::heardFrom(unsigned int librarymixer_id) {
    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_ERROR, CONNECTIVITY_MANAGER_ZONE, QString("Somehow heard from someone that is not a friend: ").append(librarymixer_id));
        return;
    }
    mFriendList[librarymixer_id].lastheard = time(NULL);
}

bool ConnectivityManager::retryConnectTCP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    /* if already connected -> done */
    if (connectState->state & PEER_S_CONNECTED) return true;

#ifndef NO_TCP_CONNECTIONS

    std::string localIP = inet_ntoa(connectState->localaddr.sin_addr);
    std::string externalIP = inet_ntoa(connectState->serveraddr.sin_addr);

    /* If address is valid, on the same subnet, not the same as external address, and not the same as own addresses, add it as a local address to try. */
    if (isValidNet(&(connectState->localaddr.sin_addr)) &&
        isSameSubnet(&(ownState.localaddr.sin_addr), &(connectState->localaddr.sin_addr)) &&
        (localIP != externalIP) &&
        (!isSameAddress(&ownState.localaddr, &connectState->localaddr)) &&
        (!isSameAddress(&ownState.serveraddr, &connectState->localaddr))) {
        queueConnectionAttempt(connectState, NET_CONN_TCP_LOCAL);
    }

    /* Always try external unless address is the same as one of ours. */
    if (isValidNet(&(connectState->serveraddr.sin_addr)) &&
        (!isSameAddress(&ownState.localaddr, &connectState->serveraddr)) &&
        (!isSameAddress(&ownState.serveraddr, &connectState->serveraddr))) {
        queueConnectionAttempt(connectState, NET_CONN_TCP_EXTERNAL);
    }

#endif // NO_TCP_CONNECTIONS

    /* Update the lastattempt to connect time to now. */
    connectState->lastattempt = time(NULL);

    /* If it's already in a connection attempt, it'll automatically use the new addresses. */
    if (connectState->inConnAttempt) return true;

    /* Start a connection attempt. */
    if (connectState->connAddrs.size() > 0) {
        connectState->actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }
    return true;
}

void ConnectivityManager::queueConnectionAttempt(peerConnectState *connectState, uint32_t connectionType) {
    /* Only add this address if there is not already one of that type on there already.
       First check if we're in a current connection attempt with that type.
       Then check if any of the queued connection attempts are for that type. */
    bool preexistingAddress = false;
    if ((connectState->inConnAttempt) && (connectState->currentConnAddr.type == connectionType)) preexistingAddress = true;
    foreach (peerConnectAddress queuedAddress, connectState->connAddrs) {
        if (queuedAddress.type == connectionType) preexistingAddress = true;
    }

    if (!preexistingAddress) {
        /* add the address */
        peerConnectAddress addressToConnect;
        addressToConnect.type = connectionType;
        if (connectionType == NET_CONN_TCP_LOCAL)
            addressToConnect.addr = connectState->localaddr;
        else if (connectionType == NET_CONN_TCP_EXTERNAL)
            addressToConnect.addr = connectState->serveraddr;

        log(LOG_DEBUG_ALERT,
            CONNECTIVITY_MANAGER_ZONE,
            QString("ConnectivityManager::queueConnectionAttempt") +
            " Attempting connection to friend #" + QString::number(connectState->librarymixer_id) +
            " with type set to " + QString::number(connectionType) +
            " with IP: " + inet_ntoa(addressToConnect.addr.sin_addr) +
            ":" + QString::number(ntohs(addressToConnect.addr.sin_port)));

        connectState->connAddrs.push_back(addressToConnect);
    }
}


bool ConnectivityManager::retryConnectUDP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    /* if already connected -> done */
    if (connectState->state & PEER_S_CONNECTED) return true;

    /* Update the lastattempt to connect time to now. */
    connectState->lastattempt = time(NULL);

    return true;
}

/**********************************************************************************
 * Body of public-facing API functions called through p3peers
 **********************************************************************************/
void ConnectivityManager::getOnlineList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    /* check for existing */
    QMap<unsigned int, peerConnectState>::iterator it;
    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        if (it.value().state & PEER_S_CONNECTED) {
            peers.push_back(it.key());
        }
    }
}

void ConnectivityManager::getSignedUpList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    /* check for existing */
    QMap<unsigned int, peerConnectState>::iterator it;
    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        if (it.value().state != PEER_S_NO_CERT) {
            peers.push_back(it.key());
        }
    }
}

void ConnectivityManager::getFriendList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    /* check for existing */
    QMap<unsigned int, peerConnectState>::iterator it;
    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        peers.push_back(it.key());
    }
}

bool ConnectivityManager::isFriend(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);
    return mFriendList.contains(librarymixer_id);
}

bool ConnectivityManager::isOnline(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);
    if (mFriendList.contains(librarymixer_id))
        return (mFriendList[librarymixer_id].state == PEER_S_CONNECTED);
    return false;
}

QString ConnectivityManager::getPeerName(unsigned int librarymixer_id) {
    QMap<unsigned int, peerConnectState>::iterator it;
    it = mFriendList.find(librarymixer_id);
    if (it == mFriendList.end()) return "";
    else return it.value().name;
}

bool ConnectivityManager::addUpdateFriend(unsigned int librarymixer_id, const QString &cert, const QString &name,
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
            if (mFriendList[librarymixer_id].state == PEER_S_NO_CERT) {
                mFriendList[librarymixer_id].state = 0;
            }
            /* Note that when we update the peer, we are creating a new pqiperson while not removing its old pqiperson.
               This is because we would need to signal to pqipersongrp that the old one needs to be removed and a new one
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
            pstate.state = PEER_S_NO_CERT;
            pstate.actions = 0;
        }
        /* Otherwise this is a successful new friend. */
        else {
            /* Should not be able to reach here with a null Cert. */
            if ((pstate.id = authMgr->findCertByLibraryMixerId(librarymixer_id)).empty()) return false;
            pstate.state = 0;
            pstate.actions = PEER_NEW;
            mStatusChanged = true;
        }
        pstate.lastcontact = 0;

        mFriendList[librarymixer_id] = pstate;

        return true;
    }
}

#ifdef false
bool ConnectivityManager::removeFriend(std::string id) {

    mDhtMgr->dropPeer(id);

    QMutexLocker stack(&connMtx);

    /* move to othersList */
    bool success = false;
    QMap<unsigned int, peerConnectState>::iterator it;
    if (mFriendList.end() != (it = mFriendList.find(id))) {

        peerConnectState peer = it.value();

        mFriendList.erase(it);

        //Why do we have to modify peer after it's already erased?
        peer.state &= (~PEER_S_CONNECTED);
        //      peer.actions = PEER_MOVED;
        peer.inConnAttempt = false;
        mStatusChanged = true;
        authMgr->RemoveCertificate(id);

        success = true;
    }

    return success;
}
#endif

void ConnectivityManager::retryConnect(unsigned int librarymixer_id) {
    if (mFriendList.contains(librarymixer_id)) {
        mFriendList[librarymixer_id].doubleTried = false;
        retryConnectTCP(librarymixer_id);
        retryConnectUDP(librarymixer_id);
    }
}

void ConnectivityManager::retryConnectAll() {
    foreach (unsigned int librarymixer_id, mFriendList.keys()) {
        retryConnect(librarymixer_id);
    }
}

/**********************************************************************************
 * Utility methods
 **********************************************************************************/

int ConnectivityManager::getLocalPort() {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    if (settings.contains("Network/PortNumber")) {
        int storedPort = settings.value("Network/PortNumber", DEFAULT_PORT).toInt();
        if (storedPort != SET_TO_RANDOMIZED_PORT &&
            storedPort > Peers::MIN_PORT &&
            storedPort < Peers::MAX_PORT) return storedPort;
        else return getRandomPortNumber();
    } else {
        int randomPort = getRandomPortNumber();
        settings.setValue("Network/PortNumber", randomPort);
        return randomPort;
    }
}

#define MIN_RAND_PORT 10000
#define MAX_RAND_PORT 30000
int ConnectivityManager::getRandomPortNumber() const {
    qsrand(time(NULL));
    return (qrand() % (MAX_RAND_PORT - MIN_RAND_PORT) + MIN_RAND_PORT);
}

QString ConnectivityManager::addressToString(struct sockaddr_in address) {
    return QString(inet_ntoa(address.sin_addr)) + ":" + QString::number(ntohs(address.sin_port));
}



#ifdef false
const uint32_t STUN_DHT = 0x0001;

void ConnectivityManager::netStartup() {
    /* StunInit gets a list of peers, and asks the DHT to find them...
     * This is needed for all systems so startup straight away
     */
    //dht init
    {
        QMutexLocker stack(&connMtx);
        mDhtMgr->enable(false); //enable dht unless vis set to no dht
    }

    //udp init
    struct sockaddr_in iaddr;
    {
        QMutexLocker stack(&connMtx);
        iaddr = ownState.localaddr;
    }
    TCP_over_UDP_init((struct sockaddr *) &iaddr, sizeof(iaddr));

    //stun init
    /********************************** STUN SERVERS ***********************************
     * We maintain a list of stun servers. This is initialised with a set of random keys.
     *
     * This is gradually rolled over with time. We update with friends/friends of friends,
     * and the lists that they provide (part of AutoDisc).
     *
     * max 100 entries?
     */
    {
        QMutexLocker stack(&connMtx);

        mDhtMgr->enableStun(true);
        /* push stun list to DHT */
        std::list<std::string>::iterator it;
        for (it = mStunList.begin(); it != mStunList.end(); it++) {
            mDhtMgr->addStun(*it);
        }
        mStunStatus = STUN_DHT;
        mStunFound = 0;
        mStunMoreRequired = true;
    }
    //reset net status
    netFlagOk = true;
    netFlagUpnpOk = false;
    netFlagDhtOk = false;
    netFlagExtOk = false;
    netFlagUdpOk = false;
    netFlagTcpOk = false;
    netFlagResetReq = false;

    /* decide which net setup mode we're going into  */

    QMutexLocker stack(&connMtx);

    connectionSetupStepTimeOutAt = time(NULL);

    /* Mask to remove the actual state bits, as any previous state is no longer useful. */
    ownState.netMode &= ~(NET_MODE_ACTUAL);

    switch (ownState.netMode & NET_MODE_TRYMODE) {
        /* These top two netmodes are only set by the ServerConfig window, and that's disabled.
           Therefore, we've been just going straight to default. */
        case NET_MODE_TRY_EXT:  /* v similar to UDP */
            ownState.netMode |= NET_MODE_EXT;
            mNetStatus = NET_UDP_SETUP;
            TCP_over_UDP_set_stunkeepalive(false);
            mStunMoreRequired = false; /* only need to validate address (EXT) */

            break;

        case NET_MODE_TRY_UDP:
            ownState.netMode |= NET_MODE_UDP;
            mNetStatus = NET_UDP_SETUP;
            TCP_over_UDP_set_stunkeepalive(true); //attempts to active tcp over udp tunnel stunning
            break;

        case NET_MODE_TRY_UPNP:
        default:
            /* Force it here (could be default!) */
            ownState.netMode |= NET_MODE_UDP;      /* set to UDP, upgraded is UPnP is Okay */
            //mNetStatus = NET_UPNP_INIT;
            TCP_over_UDP_set_stunkeepalive(true); //attempts to active tcp over udp tunnel stunning
            break;
    }
}

bool ConnectivityManager::udpExtAddressCheck() {
    /* three possibilities:
     * (1) not found yet.
     * (2) Found!
     * (3) bad udp (port switching).
     */
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    uint8_t stable;

    if (0 < TCP_over_UDP_read_extaddr((struct sockaddr *) &addr, &len, &stable)) {
        QMutexLocker stack(&connMtx);


        /* update UDP information */
        mStunExtAddr = addr;
        mStunAddrValid = true;
        mStunAddrStable = (stable != 0);

        /* update net Status flags ....
         * we've got stun information via udp...
         * so up is okay, and ext address is known stable or not.
         */

        if (mStunAddrStable)
            netFlagExtOk = true;
        netFlagUdpOk = true;

        return true;
    }
    return false;
}

class peerAddrInfo {
public:
    peerAddrInfo()
        :found(false), type(0), ts(0) {
        sockaddr_clear(&laddr);
        sockaddr_clear(&raddr);
    }

    bool found;
    uint32_t type;
    struct sockaddr_in laddr, raddr;
    time_t ts;
};


/******* overloaded from pqiConnectCb *************/
//3 functions are related to DHT
const uint32_t TCP_DEFAULT_DELAY = 2; /* 2 Seconds? is it be enough! */
void ConnectivityManager::peerStatus (std::string cert_id, struct sockaddr_in laddr, struct sockaddr_in raddr,
                                      uint32_t type, uint32_t flags, uint32_t source) {
    QMap<unsigned int, peerConnectState>::iterator it;

    time_t now = time(NULL);

    peerAddrInfo details;
    details.type    = type;
    details.found   = true;
    details.laddr   = laddr;
    details.raddr   = raddr;
    details.ts      = now;


    {
        QMutexLocker stack(&connMtx);
        {
            /* Log */
            std::ostringstream out;
            out << "ConnectivityManager::peerStatus()";
            out << " id: " << cert_id;
            out << " laddr: " << inet_ntoa(laddr.sin_addr);
            out << " lport: " << ntohs(laddr.sin_port);
            out << " raddr: " << inet_ntoa(raddr.sin_addr);
            out << " rport: " << ntohs(raddr.sin_port);
            out << " type: " << type;
            out << " flags: " << flags;
            out << " source: " << source;
            log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE, out.str().c_str());
        }

        /* look up the id */
        it = mFriendList.find(authMgr->findLibraryMixerByCertId(cert_id));
        if (it == mFriendList.end()) {
            /* not found - ignore */
            return;
        }

        /* update the status */

        /* if source is DHT */
        if (source == CB_DHT) {
            /* DHT can tell us about
             * 1) connect type (UDP/TCP/etc)
             * 2) local/external address
             */
            it.value().source = CB_DHT;
            it.value().dht = details;

            /* if we are recieving these - the dht is definitely up.
             */

            netFlagDhtOk = true;
        } else if (source == CB_PERSON) {
            /* PERSON can tell us about
             * 1) online / offline
             * 2) connect address
             * -> update all!
             */

            it.value().source = CB_PERSON;
            it.value().peer = details;

            it.value().localaddr = laddr;
            it.value().serveraddr = raddr;

            /* must be online to recv info (should be connected too!)
             * but no need for action as should be connected already
             */

            it.value().netMode &= (~NET_MODE_ACTUAL); /* clear actual flags */
            if (flags & NET_FLAGS_EXTERNAL_ADDR) {
                it.value().netMode = NET_MODE_EXT;
            } else if (flags & NET_FLAGS_STABLE_UDP) {
                it.value().netMode = NET_MODE_UDP;
            } else {
                it.value().netMode = NET_MODE_UNREACHABLE;
            }
        }

        /* Determine Reachability (only advisory) */
        if (ownState.netMode & NET_MODE_UDP) {
            if ((details.type & NET_CONN_UDP_DHT_SYNC) ||
                    (details.type & NET_CONN_TCP_EXTERNAL)) {
                /* reachable! */
                it.value().state &= (~PEER_S_UNREACHABLE);
            } else {
                /* unreachable */
                it.value().state |= PEER_S_UNREACHABLE;
            }
        } else if (ownState.netMode & NET_MODE_UNREACHABLE) {
            if (details.type & NET_CONN_TCP_EXTERNAL) {
                /* reachable! */
                it.value().state &= (~PEER_S_UNREACHABLE);
            } else {
                /* unreachable */
                it.value().state |= PEER_S_UNREACHABLE;
            }
        } else {
            it.value().state &= (~PEER_S_UNREACHABLE);
        }

        /* if already connected -> done */
        if (it.value().state & PEER_S_CONNECTED) {
            {
                /* Log */
                std::ostringstream out;
                out << "ConnectivityManager::peerStatus() NO CONNECT (already connected!)";
                log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE, out.str().c_str());
            }

            return;
        }


        /* are the addresses different? */

#ifndef NO_AUTO_CONNECTION

#ifndef NO_TCP_CONNECTIONS

        /* add in attempts ... local(TCP), remote(TCP)
         * udp must come from notify
         */

        /* determine delay (for TCP connections)
         * this is to ensure that simultaneous connections don't occur
         * (which can fail).
         * easest way is to compare ids ... and delay one of them
         */

        uint32_t tcp_delay = 0;
        if (cert_id > ownState.id) {
            tcp_delay = TCP_DEFAULT_DELAY;
        }

        /* if address is same -> try local */
        if ((isValidNet(&(details.laddr.sin_addr))) &&
            (isSameAddress(&(ownState.localaddr), &(details.laddr))))

        {
            /* add the local address */
            peerConnectAddress pca;
            pca.delay = tcp_delay;
            pca.period = 0;
            pca.type = NET_CONN_TCP_LOCAL;
            pca.addr = details.laddr;

            {
                /* Log */
                std::ostringstream out;
                out << "ConnectivityManager::peerStatus() PushBack Local TCP Address: ";
                out << " id: " << cert_id;
                out << " laddr: " << inet_ntoa(pca.addr.sin_addr);
                out << ":" << ntohs(pca.addr.sin_port);
                out << " type: " << pca.type;
                out << " delay: " << pca.delay;
                out << " period: " << pca.period;
                out << " source: " << source;
                log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE, out.str().c_str());
            }

            it.value().connAddrs.push_back(pca);
        }

        if ((details.type & NET_CONN_TCP_EXTERNAL) &&
                (isValidNet(&(details.raddr.sin_addr))))

        {
            /* add the remote address */
            peerConnectAddress pca;
            pca.delay = tcp_delay;
            pca.period = 0;
            pca.type = NET_CONN_TCP_EXTERNAL;
            pca.addr = details.raddr;

            {
                /* Log */
                std::ostringstream out;
                out << "ConnectivityManager::peerStatus() PushBack Remote TCP Address: ";
                out << " id: " << cert_id;
                out << " raddr: " << inet_ntoa(pca.addr.sin_addr);
                out << ":" << ntohs(pca.addr.sin_port);
                out << " type: " << pca.type;
                out << " delay: " << pca.delay;
                out << " period: " << pca.period;
                out << " source: " << source;
                log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE, out.str().c_str());
            }

            it.value().connAddrs.push_back(pca);
        }

#endif  // NO_TCP_CONNECTIONS

    } /****** STACK UNLOCK MUTEX *******/

    /* notify if they say we can, or we cannot connect ! */
    if (details.type & NET_CONN_UDP_DHT_SYNC) {
        retryConnectUDP(authMgr->findLibraryMixerByCertId(cert_id));
    }
#else
    } // NO_AUTO_CONNECTION /****** STACK UNLOCK MUTEX *******/
#endif  // NO_AUTO_CONNECTION

    QMutexLocker stack(&connMtx);

    if (it.value().inConnAttempt) return;

    /* start a connection attempt */
    if (it.value().connAddrs.size() > 0) {
        it.value().actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }
}

const uint32_t UDP_DHT_DELAY = DHT_NOTIFY_PERIOD + 60; /* + 1 minute for DHT POST */
const uint32_t UDP_PROXY_DELAY = 30;  /* 30 seconds (NOT IMPLEMENTED YET!) */

void ConnectivityManager::peerConnectRequest(std::string id, struct sockaddr_in raddr, uint32_t source) {
    {
        /* Log */
        std::ostringstream out;
        out << "ConnectivityManager::peerConnectRequest()";
        out << " id: " << id;
        out << " raddr: " << inet_ntoa(raddr.sin_addr);
        out << ":" << ntohs(raddr.sin_port);
        out << " source: " << source;
        log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE, out.str().c_str());
    }

    /******************** TCP PART *****************************/
    retryConnectTCP(authMgr->findLibraryMixerByCertId(id));

    /******************** UDP PART *****************************/

    QMutexLocker stack(&connMtx);

    if (ownState.netMode & NET_MODE_UNREACHABLE) return;

    /* look up the id */
    QMap<unsigned int, peerConnectState>::iterator it;
    it = mFriendList.find(authMgr->findLibraryMixerByCertId(id));
    if (it == mFriendList.end()) return;

    /* if already connected -> done */
    if (it.value().state & PEER_S_CONNECTED) return;

    /* this is a UDP connection request (DHT only for the moment!) */
    if (isValidNet(&(raddr.sin_addr))) {
        /* add the remote address */
        peerConnectAddress pca;
        pca.type = NET_CONN_UDP_DHT_SYNC;
        pca.delay = 0;

        if (source == CB_DHT) {
            pca.period = UDP_DHT_DELAY;
        } else if (source == CB_PROXY) {
            pca.period = UDP_PROXY_DELAY;
        } else {
            /* error! */
            pca.period = UDP_PROXY_DELAY;
        }

        pca.addr = raddr;

        {
            /* Log */
            std::ostringstream out;
            out << "ConnectivityManager::peerConnectRequest() PushBack UDP Address: ";
            out << " id: " << id;
            out << " raddr: " << inet_ntoa(pca.addr.sin_addr);
            out << ":" << ntohs(pca.addr.sin_port);
            out << " type: " << pca.type;
            out << " delay: " << pca.delay;
            out << " period: " << pca.period;
            log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE, out.str().c_str());
        }

        /* push to the back ... TCP ones should be tried first */
        it.value().connAddrs.push_back(pca);
    }

    if (it.value().inConnAttempt) {
        /*  -> it'll automatically use the addresses */
        return;
    }

    /* start a connection attempt */
    if (it.value().connAddrs.size() > 0) {
        it.value().actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }
}

void ConnectivityManager::stunStatus(std::string id, struct sockaddr_in raddr, uint32_t type, uint32_t flags) {
    bool stillStunning;
    {
        QMutexLocker stack(&connMtx);

        stillStunning = (mStunStatus == STUN_DHT);
    }

    /* only useful if they have an exposed TCP/UDP port */
    if (type & NET_CONN_TCP_EXTERNAL) {
        if (stillStunning) {
            {
                QMutexLocker stack(&connMtx);
                mStunFound++;
            }

            /* push to the UDP */
            udpStunPeer(id, raddr);

        }

        /* push to the stunCollect */
        stunCollect(id, raddr, flags);
    }
}

void ConnectivityManager::udpStunPeer(std::string id, struct sockaddr_in &addr) {
    /* add it into udp stun list */
    TCP_over_UDP_add_stunpeer((struct sockaddr *) &addr, sizeof(addr), id.c_str());
}

void ConnectivityManager::netUdpCheck() {
    if (udpExtAddressCheck() || (mUpnpAddrValid)) {
        bool extValid = false;
        bool extAddrStable = false;
        struct sockaddr_in iaddr;
        struct sockaddr_in extAddr;
        uint32_t mode = 0;

        {
            QMutexLocker stack(&connMtx);
            mNetStatus = NET_DONE;

            /* get the addr from the configuration */
            iaddr = ownState.localaddr;

            if (mUpnpAddrValid) {
                extValid = true;
                extAddr = mUpnpExtAddr;
                extAddrStable = true;
            } else if (mStunAddrValid) {
                extValid = true;
                extAddr = mStunExtAddr;
                extAddrStable = mStunAddrStable;
            }

            if (extValid) {
                ownState.serveraddr = extAddr;
                mode = NET_CONN_TCP_LOCAL;

                if (!extAddrStable) {
                    ownState.netMode &= ~(NET_MODE_ACTUAL);
                    ownState.netMode |= NET_MODE_UNREACHABLE;
                    TCP_over_UDP_set_stunkeepalive(false);
                    mStunMoreRequired = false; /* no point -> unreachable (EXT) */

                    /* send a system warning message */
                    pqiNotify *notify = getPqiNotify();
                    if (notify) {
                        QString title =
                            "Warning: Bad Firewall Configuration";

                        QString msg;
                        msg +=  "               **** WARNING ****     \n";
                        msg +=  "Mixologist has detected that you are behind";
                        msg +=  " a restrictive Firewall\n";
                        msg +=  "\n";
                        msg +=  "You cannot connect to other firewalled peers\n";
                        msg +=  "\n";
                        msg +=  "You can fix this by:\n";
                        msg +=  "   (1) opening an External Port\n";
                        msg +=  "   (2) enabling UPnP, or\n";
                        msg +=  "   (3) get a newer Firewall/Router\n";

                        notify->AddSysMessage(SYS_WARNING, title, msg);
                    }

                } else if (mUpnpAddrValid  || (ownState.netMode & NET_MODE_EXT)) {
                    mode |= NET_CONN_TCP_EXTERNAL;
                    mode |= NET_CONN_UDP_DHT_SYNC;
                } else { // if (extAddrStable)
                    /* Check if extAddr == intAddr (Not Firewalled) */
                    if ((0 == inaddr_cmp(iaddr, extAddr)) &&
                            isExternalNet(&(extAddr.sin_addr))) {
                        mode |= NET_CONN_TCP_EXTERNAL;
                    }

                    mode |= NET_CONN_UDP_DHT_SYNC;
                }
            }

        }

        mDhtMgr->setExternalInterface(iaddr, extAddr, mode);

        /* flag unreachables! */
        if ((extValid) && (!extAddrStable)) {
            netUnreachableCheck();
        }
    }
}

void ConnectivityManager::netUnreachableCheck() {
    QMap<unsigned int, peerConnectState>::iterator it;

    QMutexLocker stack(&connMtx);

    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        /* get last contact detail */
        if (it.value().state & PEER_S_CONNECTED) {
            continue;
        }

        peerAddrInfo details;
        switch (it.value().source) {
            case CB_DHT:
                details = it.value().dht;
                break;
            case CB_PERSON:
                details = it.value().peer;
                break;
            default:
                continue;
                break;
        }

        std::cerr << "NUC() Peer: " << it.key() << std::endl;

        /* Determine Reachability (only advisory) */
        // if (ownState.netMode == NET_MODE_UNREACHABLE) // MUST BE TRUE!
        {
            if (details.type & NET_CONN_TCP_EXTERNAL) {
                /* reachable! */
                it.value().state &= (~PEER_S_UNREACHABLE);
            } else {
                /* unreachable */
                it.value().state |= PEER_S_UNREACHABLE;
            }
        }
    }

}
#endif

