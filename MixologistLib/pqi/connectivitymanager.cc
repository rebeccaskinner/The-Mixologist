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

#define NO_TCP_CONNECTIONS 1
/****
 * #define NO_TCP_CONNECTIONS 1
 ***/
#define NO_AUTO_CONNECTION 1
/****
 * #define NO_AUTO_CONNECTION 1
 ***/

#define TCP_RETRY_PERIOD 600 //10 minute wait between automatic connection retries
#define UDP_PUNCHING_PERIOD 20 //20 seconds between UDP hole punches
#define USED_IP_WAIT_TIME 5 //5 second wait if tried to connect to an IP that was already in use in a connection attempt
#define LAST_HEARD_TIMEOUT 300 //5 minute wait if haven't heard from a friend before we mark them as disconnected
#define UPNP_INIT_TIMEOUT 10 //10 second timeout for UPNP to initialize the firewall
#define STUN_TEST_TIMEOUT 3 //3 second timeout from a STUN request for a STUN response to be received

peerConnectAddress::peerConnectAddress()
    :delay(0), period(0) {
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

/**********************************************************************************
 * Connection Setup
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

    int triesRemaining = 100;
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
           connect(udpTestSocket, SIGNAL(receivedStunBindingResponse(QString,QString,int,ushort,QString)),
                   this, SLOT(receivedStunPacket(QString,QString,int,ushort,QString)));
       }
    }
    if (!udpTestSocket) {
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Network failure", "Unable to open a UDP port");
        exit(1);
    }

    udpMainSocket = new UdpSorter(ownState.localaddr);
    TCP_over_UDP_init(udpMainSocket);

    if (!udpMainSocket->okay()) {
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Network failure", "Unable to open main UDP port");
        exit(1);
    } else {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, "Opened UDP main port on " + QString::number(ntohs(ownState.localaddr.sin_port)));
        connect(udpMainSocket, SIGNAL(receivedStunBindingResponse(QString,QString,int,ushort, QString)),
                this, SLOT(receivedStunPacket(QString,QString,int,ushort,QString)));
    }
}

void ConnectivityManager::netTick() {
    QMutexLocker stack(&connMtx);
    if (CONNECTION_STATUS_FINDING_STUN_FRIENDS == connectionStatus) {
        if (connectionSetupStepTimeOutAt == 0) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Initiating automatic connection configuration");
            foreach (unsigned int librarymixer_id, mFriendList.keys()) {
                if (stunServer1 && stunServer2) break;
                sendStunPacket(peers->getPeerName(librarymixer_id), &mFriendList[librarymixer_id].serveraddr, udpTestSocket);
            }
            connectionSetupStepTimeOutAt = time(NULL) + STUN_TEST_TIMEOUT;
        } else if (time(NULL) > connectionSetupStepTimeOutAt) {
            setNewConnectionStatus(CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS);
        }
    } else if (CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS == connectionStatus) {
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
    } else if (CONNECTION_STATUS_STUNNING_INITIAL == connectionStatus) {
        int currentStatus = handleStunStep("Primary STUN server", stunServer1, udpTestSocket, ntohs(ownState.localaddr.sin_port));
        if (currentStatus == 1) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, "Contacting primary STUN server to determine if we are behind a firewall");
        } else if (currentStatus == -1) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, "No response from primary stun server, response likely blocked by firewall");
            setNewConnectionStatus(CONNECTION_STATUS_TRYING_UPNP);
        }
    } else if (CONNECTION_STATUS_TRYING_UPNP == connectionStatus) {
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
                    "Universal Plug and Play successfully configured router");
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
    } else if (CONNECTION_STATUS_STUNNING_UPNP_TEST == connectionStatus) {
        int currentStatus = handleStunStep("Primary STUN Server", stunServer1, udpTestSocket, ntohs(ownState.localaddr.sin_port));
        if (currentStatus == 1) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Contacting primary STUN server to test Universal Plug and Play configuration");
        } else if (currentStatus == -1) {
            mUpnpMgr->shutdown();
            delete mUpnpMgr;
            mUpnpMgr = NULL;
            setNewConnectionStatus(CONNECTION_STATUS_STUNNING_MAIN_PORT);
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Connection test failed despite successfully configured connection, abandoning Universal Plug and Play approach");
        }
    } else if (CONNECTION_STATUS_STUNNING_MAIN_PORT == connectionStatus) {
        int currentStatus = handleStunStep("Secondary STUN Server", stunServer2, udpMainSocket);
        if (currentStatus == 1) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Contacting secondary STUN server to verify whether we have any Internet connection at all");
        }
        if (currentStatus == -1) {
            setNewConnectionStatus(CONNECTION_STATUS_NO_NET);
            /* This is a special case of failure, which is total failure to receive on the main port, along with no UPNP.
               In this case, we have failed without even knowing our own external address yet.
               Therefore, for this type of failure, we'll need to signal to get the external address from LibraryMixer. */
            readyToConnectToFriends = CONNECT_FRIENDS_GET_ADDRESS_FROM_LIBRARYMIXER;
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Total failure to access the Internet using Mixologist port " + QString::number(ntohs(ownState.localaddr.sin_port)));
        }
    } else if (CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST == connectionStatus) {
        int currentStatus = handleStunStep("Primary STUN Server", stunServer1, udpTestSocket, ntohs(ownState.localaddr.sin_port));
        if (currentStatus == 1) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Contacting primary STUN server to test whether we can punch a hole through the firewall using UDP");
        } else if (currentStatus == -1) {
            setNewConnectionStatus(CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST);
        }
    } else if (CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST == connectionStatus) {
        int currentStatus = handleStunStep("Primary STUN Server", stunServer1, udpMainSocket);
        if (currentStatus == 1) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Contacting primary STUN server to further probe the nature of the firewall");
        } else if (currentStatus == -1) {
            setNewConnectionStatus(CONNECTION_STATUS_NO_NET);
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Unexpected inability to contact primary STUN server, automatic connection configuration shutting down");
        }
    } else if (CONNECTION_STATUS_UPNP_IN_USE == connectionStatus) {
        netUpnpMaintenance();
    } else {
/*
    case CONNECTION_STATUS_UDP_HOLE_PUNCHING:
        break;
    case CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING:
        break;
    case CONNECTION_STATUS_UNFIREWALLED:
    case CONNECTION_STATUS_PORT_FORWARDED:
    case CONNECTION_STATUS_SYMMETRIC_NAT:
    case CONNECTION_STATUS_NO_NET: */
    }
}

int ConnectivityManager::handleStunStep(const QString& stunServerName, const sockaddr_in *stunServer, UdpSorter *sendSocket, ushort returnPort) {
    if (connectionSetupStepTimeOutAt != 0) {
        if (time(NULL) > connectionSetupStepTimeOutAt) return -1;
    } else {
        if (sendStunPacket(stunServerName, stunServer, sendSocket, returnPort)) {
            connectionSetupStepTimeOutAt = time(NULL) + STUN_TEST_TIMEOUT;
            return 1;
        }
    }
    return 0;
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

void ConnectivityManager::receivedStunPacket(QString transactionId, QString mappedAddress, int mappedPort, ushort receivedOnPort, QString receivedFromAddress) {
    QMutexLocker stack(&connMtx);
    if (!pendingStunTransactions.contains(transactionId)) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, QString("Discarding STUN response not matching any current outstanding request."));
        return;
    }
    struct in_addr fromIP;
    inet_aton(receivedFromAddress.toStdString().c_str(), &fromIP);
    if (isSameSubnet(&(ownState.localaddr.sin_addr), &fromIP)) {
        log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE, "Discarding STUN response received on own subnet");
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
                "Using public server " + pendingStunTransactions[transactionId].serverName + " as a fallback for the primary STUN server");
        } else {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Using public server " + pendingStunTransactions[transactionId].serverName + " as a fallback for the secondary STUN server");
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
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            "Internet connection verified on " + mappedAddress +
            ":" + QString::number(ntohs(ownState.serveraddr.sin_port)));
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST) {
        setNewConnectionStatus(CONNECTION_STATUS_UDP_HOLE_PUNCHING);
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            "Connection test successful, punching a UDP hole in the firewall on " + mappedAddress +
            ":" + QString::number(ntohs(ownState.serveraddr.sin_port)));
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST) {
        if (mappedPort == ntohs(ownState.serveraddr.sin_port)) {
            setNewConnectionStatus(CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING);
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

void ConnectivityManager::setNewConnectionStatus(int newStatus) {
    connectionStatus = (ConnectionStatus) newStatus;
    connectionSetupStepTimeOutAt = 0;
    pendingStunTransactions.clear();
}

bool ConnectivityManager::shutdown() {
    QMutexLocker stack(&connMtx);
    if (mUpnpMgr) mUpnpMgr->shutdown();

    TCP_over_UDP_shutdown();

    return true;
}

/**********************************************************************************
 * Connecting to Friends
 **********************************************************************************/

#define ADDRESS_UPLOAD_TIMEOUT 5
void ConnectivityManager::statusTick() {
    QList<unsigned int> friendsToConnect;
    {
        QMutexLocker stack(&connMtx);

        if (readyToConnectToFriends == CONNECT_FRIENDS_CONNECTION_INITIALIZING) return;
        else if (readyToConnectToFriends == CONNECT_FRIENDS_UPDATE_LIBRARYMIXER) {
            static time_t addressLastUploaded = 0;
            if (time(NULL) > addressLastUploaded + ADDRESS_UPLOAD_TIMEOUT) {
                librarymixerconnect->uploadAddress(inet_ntoa(ownState.localaddr.sin_addr), ntohs(ownState.localaddr.sin_port),
                                                   inet_ntoa(ownState.serveraddr.sin_addr), ntohs(ownState.serveraddr.sin_port));
                addressLastUploaded = time(NULL);
            }
            return;
        }
        else if (readyToConnectToFriends == CONNECT_FRIENDS_GET_ADDRESS_FROM_LIBRARYMIXER) {
            static time_t addressLastUploaded = 0;
            if (time(NULL) > addressLastUploaded + ADDRESS_UPLOAD_TIMEOUT) {
                /* This is only a desperate guess, but we have no idea what our external port is, so we will guess it is the same. */
                ownState.serveraddr.sin_port = ownState.localaddr.sin_port;
                librarymixerconnect->uploadAddress(inet_ntoa(ownState.localaddr.sin_addr), ntohs(ownState.localaddr.sin_port),
                                                   "", ntohs(ownState.serveraddr.sin_port));
                addressLastUploaded = time(NULL);
            }
            return;
        }

        time_t now = time(NULL);
        time_t timeoutIfOlder = now - LAST_HEARD_TIMEOUT;
        time_t retryIfOlder;
        if (connectionStatus == CONNECTION_STATUS_UDP_HOLE_PUNCHING ||
            connectionStatus == CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING)
            retryIfOlder = now - UDP_PUNCHING_PERIOD;
        else retryIfOlder = now - TCP_RETRY_PERIOD;

        /* Can't simply use mFriendList.values() in foreach because that generates copies of the peerConnectState,
           and we need to be able to save in the changes. */
        foreach (unsigned int librarymixer_id, mFriendList.keys()) {
            if (mFriendList[librarymixer_id].state & PEER_S_NO_CERT) continue;

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

                    friendsToConnect.push_back(librarymixer_id);
                } else continue;
            }

            /* If last attempt was long enough ago, start a whole new attempt. */
            else if (mFriendList[librarymixer_id].lastattempt < retryIfOlder) {
                /* Initialize double try information, as this will be our first of the two tries. */
                mFriendList[librarymixer_id].doubleTried = false;
                mFriendList[librarymixer_id].schedulednexttry = 0;

                friendsToConnect.push_back(librarymixer_id);
            }

            /* If we have a scheduled second try then start it. */
            else if (mFriendList[librarymixer_id].schedulednexttry != 0 && mFriendList[librarymixer_id].schedulednexttry < now) {
                /* We are performing the scheduled next try now, so clear it out. */
                mFriendList[librarymixer_id].schedulednexttry = 0;

                friendsToConnect.push_back(librarymixer_id);
            }
        }
    }

#ifndef NO_AUTO_CONNECTION
    foreach (unsigned int librarymixer_id, friendsToConnect) {
        retryConnectTCP(librarymixer_id);
        retryConnectUDP(librarymixer_id);
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

bool ConnectivityManager::getQueuedConnectAttempt(unsigned int librarymixer_id, struct sockaddr_in &addr,
                                                  uint32_t &delay, uint32_t &period, TransportLayerType &transportLayerType) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
            QString("Can't make attempt to connect to user, not in friends list, certificate id was: ").append(librarymixer_id));
        return false;
    }

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    if (connectState->connAddrs.size() < 1) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
            QString("Can't make attempt to connect to user, have no IP address: ").append(connectState->name));
        return false;
    }

    connectState->lastattempt = time(NULL);

    peerConnectAddress address = connectState->connAddrs.front();
    connectState->connAddrs.pop_front();

    /* Test if address is in use already. */
    if (usedIps.contains(addressToString(address.addr))) {
        /* If we have an alternative address to try for this attempt, simply stick this one on the back and request a try for a different address in the meantime. */
        if (connectState->connAddrs.size() >= 1) {
            connectState->connAddrs.push_back(address);
            connectState->actions |= PEER_CONNECT_REQ;
        } else {
            if (usedIps[addressToString(address.addr)] == USED_IP_CONNECTED) {
                log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE,
                    "ConnectivityManager::connectAttempt Can not connect to " + addressToString(address.addr) + " due to existing connection.");
            } else if (usedIps[addressToString(address.addr)] == USED_IP_CONNECTING) {
                connectState->schedulednexttry = time(NULL) + USED_IP_WAIT_TIME;
                log(LOG_DEBUG_BASIC, CONNECTIVITY_MANAGER_ZONE,
                    "ConnectivityManager::connectAttempt Waiting to try to connect to " + addressToString(address.addr) + " due to existing attempted connection.");
            }
        }
        return false;
    }

    /* If we get here, the IP address is good to use, so load it in. */
    connectState->inConnAttempt = true;
    connectState->currentConnAddr = address;
    usedIps[addressToString(address.addr)] = USED_IP_CONNECTING;

    addr = connectState->currentConnAddr.addr;
    delay = connectState->currentConnAddr.delay;
    period = connectState->currentConnAddr.period;
    transportLayerType = connectState->currentConnAddr.transportLayerType;

    log(LOG_DEBUG_BASIC, CONNECTIVITY_MANAGER_ZONE,
        QString("ConnectivityManager::connectAttempt Returning information for connection attempt to user: ").append(connectState->name));

    return true;
}

bool ConnectivityManager::reportConnectionUpdate(unsigned int librarymixer_id, bool success, uint32_t flags) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
            QString("Failed to connect to user, not in friends list, friend number was: ").append(QString::number(librarymixer_id)));
        return false;
    }

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    connectState->inConnAttempt = false;

    if (success) {
        /* remove other attempts */
        connectState->connAddrs.clear();

        usedIps[addressToString(connectState->currentConnAddr.addr)] = USED_IP_CONNECTED;

        log(LOG_DEBUG_BASIC, CONNECTIVITY_MANAGER_ZONE,
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

void ConnectivityManager::setFallbackExternalIP(QString address) {
    QMutexLocker stack(&connMtx);
    log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, "Setting IP address based on what LibraryMixer sees, guessing external port, connection may not work");
    inet_aton(address.toStdString().c_str(), &ownState.serveraddr.sin_addr);
}

void ConnectivityManager::queueConnectionAttempt(peerConnectState *connectState, TransportLayerType transportLayerType, TCPLocalOrExternal localOrExternal) {
    //TODO remove testing code
    if (connectState->librarymixer_id != 11 &&
        connectState->librarymixer_id != 13) return;

    /* Only add this address if there is not already one of that type on there already.
       First check if we're in a current connection attempt with that type.
       Then check if any of the queued connection attempts are for that type. */
    if (connectState->inConnAttempt) {
        if (connectState->currentConnAddr.transportLayerType == transportLayerType) {
            if (transportLayerType == CONNECTION_UDP_TRANSPORT) return;
            else if (transportLayerType == CONNECTION_TCP_TRANSPORT) {
                if (connectState->currentConnAddr.tcpLocalOrExternal == localOrExternal) return;
            }
        }
    }
    foreach (peerConnectAddress queuedAddress, connectState->connAddrs) {
        if (queuedAddress.transportLayerType == transportLayerType) {
            if (transportLayerType == CONNECTION_UDP_TRANSPORT) return;
            else if (transportLayerType == CONNECTION_TCP_TRANSPORT) {
                if (queuedAddress.tcpLocalOrExternal == localOrExternal) return;
            }
        }
    }

    /* If we get here, this is new, add the connection attempt. */
    peerConnectAddress addressToConnect;
    addressToConnect.transportLayerType = transportLayerType;
    addressToConnect.tcpLocalOrExternal = localOrExternal;
    if (localOrExternal == TCP_LOCAL_ADDRESS)
        addressToConnect.addr = connectState->localaddr;
    else if (localOrExternal == TCP_EXTERNAL_ADDRESS)
        addressToConnect.addr = connectState->serveraddr;
    //TODO FIX: In shipping version, we must use serveraddr instead of localaddr
    else if (transportLayerType == CONNECTION_UDP_TRANSPORT) {
        addressToConnect.addr = connectState->localaddr;
        addressToConnect.period = 300; //5 minutes
    }

    log(LOG_DEBUG_ALERT,
        CONNECTIVITY_MANAGER_ZONE,
        QString("ConnectivityManager::queueConnectionAttempt") +
        " Attempting connection to friend #" + QString::number(connectState->librarymixer_id) +
        " with IP: " + addressToString(addressToConnect.addr));

    connectState->connAddrs.push_back(addressToConnect);
}

bool ConnectivityManager::retryConnectTCP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

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
        queueConnectionAttempt(connectState, CONNECTION_TCP_TRANSPORT, TCP_LOCAL_ADDRESS);
    }

    /* Always try external unless address is the same as one of ours. */
    if (isValidNet(&(connectState->serveraddr.sin_addr)) &&
        (!isSameAddress(&ownState.localaddr, &connectState->serveraddr)) &&
        (!isSameAddress(&ownState.serveraddr, &connectState->serveraddr))) {
        queueConnectionAttempt(connectState, CONNECTION_TCP_TRANSPORT, TCP_EXTERNAL_ADDRESS);
    }

    /* Update the lastattempt to connect time to now. */
    connectState->lastattempt = time(NULL);

    /* If it's already in a connection attempt, it'll automatically use the new addresses. */
    if (connectState->inConnAttempt) return true;

    /* Start a connection attempt. */
    if (connectState->connAddrs.size() > 0) {
        connectState->actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }

#endif // NO_TCP_CONNECTIONS

    return true;
}

bool ConnectivityManager::retryConnectUDP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    /* if already connected -> done */
    if (connectState->state & PEER_S_CONNECTED) return true;


    queueConnectionAttempt(connectState, CONNECTION_UDP_TRANSPORT, NOT_TCP);

    /* Update the lastattempt to connect time to now. */
    connectState->lastattempt = time(NULL);

    /* If it's already in a connection attempt, it'll automatically use the new addresses. */
    if (connectState->inConnAttempt) return true;

    /* Start a connection attempt. */
    if (connectState->connAddrs.size() > 0) {
        connectState->actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }

    /* Update the lastattempt to connect time to now. */
    connectState->lastattempt = time(NULL);

    return true;
}

/**********************************************************************************
 * Body of public-facing API functions called through p3peers
 **********************************************************************************/
void ConnectivityManager::getOnlineList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        if (mFriendList[friend_id].state & PEER_S_CONNECTED)
            peers.push_back(friend_id);
    }
}

void ConnectivityManager::getSignedUpList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        if (mFriendList[friend_id].state != PEER_S_NO_CERT)
            peers.push_back(friend_id);
    }
}

void ConnectivityManager::getFriendList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        peers.push_back(friend_id);
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
    if (mFriendList.contains(librarymixer_id)) return mFriendList[librarymixer_id].name;
    else return "";
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

#ifdef false
        //TODO FIX: In shipping version, we must use serveraddr instead of localaddr
        int socket = tou_socket(NULL, NULL, NULL);
        if (socket != -1) {
            tou_listenfor(socket, (struct sockaddr *) &localAddress, sizeof(localAddress));
        }
#endif

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
        mFriendList[librarymixer_id].schedulednexttry = 0;
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
