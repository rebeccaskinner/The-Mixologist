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

#include <pqi/connectivitymanager.h>
#include <pqi/pqinotify.h>

#include <interface/peers.h>
#include <interface/settings.h>
#include <interface/librarymixer-connect.h>

#include <util/debug.h>

//#define NO_TCP_CONNECTIONS 1
//#define NO_TCP_BACK_CONNECTIONS 1
//#define NO_AUTO_CONNECTION 1
//#define NO_UDP_CONNECTIONS 1
//#define TEST_UDP_LOCAL 1

#define TCP_RETRY_PERIOD 600 //10 minute wait between automatic connection retries
#define UDP_PUNCHING_PERIOD 20 //20 seconds between UDP hole punches
#define USED_SOCKET_WAIT_TIME 5 //5 second wait if tried to connect to an IP that was already in use in a connection attempt
#define REQUESTED_RETRY_WAIT_TIME 10 //10 seconds wait if our connection attempt failed and we were signaled to try again
#define LAST_HEARD_TIMEOUT 300 //5 minute wait if haven't heard from a friend before we mark them as disconnected
#define UPNP_INIT_TIMEOUT 10 //10 second timeout for UPNP to initialize the firewall
#define STUN_TEST_TIMEOUT 4 //3 second timeout from a STUN request for a STUN response to be received
#define FRIENDS_LIST_UPDATE_PERIOD_NORMAL 1800 //30 minute wait between updating friends list
#define FRIENDS_LIST_UPDATE_PERIOD_LIMITED_INBOUND 300 //5 minute wait when inbound connections from friends will have difficulty

QueuedConnectionAttempt::QueuedConnectionAttempt()
    :delay(0) {sockaddr_clear(&addr);}

peerConnectState::peerConnectState()
    :name(""),
     id(""),
     lastcontact(0),
     nextTcpTryAt(0),
     state(FCS_NOT_MIXOLOGIST_ENABLED),
     actions(0),
     inConnectionAttempt(false) {
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

    connect(librarymixerconnect, SIGNAL(uploadedAddress()), this, SLOT(addressUpdatedOnLibraryMixer()));
    connect(librarymixerconnect, SIGNAL(downloadedFriends()), this, SLOT(friendsListUpdated()));

    /* Load balance between the servers by picking the order at random. */
    if (rand() % 2 == 0) {
        fallbackStunServers.append("stun1.librarymixer.com");
        fallbackStunServers.append("stun2.librarymixer.com");
    } else {
        fallbackStunServers.append("stun2.librarymixer.com");
        fallbackStunServers.append("stun1.librarymixer.com");
    }
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
    ownLocalAddress.sin_addr = getPreferredInterface();
    ownLocalAddress.sin_port = htons(getLocalPort());
    ownLocalAddress.sin_family = AF_INET;
    ownExternalAddress.sin_family = AF_INET;

    /* We will open the udpTestSocket on a random port on our chosen interface. */
    struct sockaddr_in udpTestAddress = ownLocalAddress;
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
           connect(udpTestSocket, SIGNAL(receivedStunBindingResponse(QString,QString,ushort,ushort,QString)),
                   this, SLOT(receivedStunBindingResponse(QString,QString,ushort,ushort,QString)));
       }
    }
    if (!udpTestSocket) {
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Network failure", "Unable to open a UDP port");
        exit(1);
    }

    udpMainSocket = new UdpSorter(ownLocalAddress);
    TCP_over_UDP_init(udpMainSocket);

    if (!udpMainSocket->okay()) {
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Network failure", "Unable to open main UDP port");
        exit(1);
    } else {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, "Opened UDP main port on " + QString::number(ntohs(ownLocalAddress.sin_port)));
        connect(udpMainSocket, SIGNAL(receivedStunBindingResponse(QString,QString,ushort,ushort, QString)),
                this, SLOT(receivedStunBindingResponse(QString,QString,ushort,ushort,QString)));
        connect(udpMainSocket, SIGNAL(receivedUdpTunneler(uint,QString,ushort)),
                this, SLOT(receivedUdpTunneler(uint,QString,ushort)));
        connect(udpMainSocket, SIGNAL(receivedUdpConnectionNotice(uint,QString,ushort)),
                this, SLOT(receivedUdpConnectionNotice(uint,QString,ushort)));
        connect(udpMainSocket, SIGNAL(receivedTcpConnectionRequest(uint,QString,ushort)),
                this, SLOT(receivedTcpConnectionRequest(uint,QString,ushort)));
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
            if (!fallbackStunServers.isEmpty()) {
                QString currentServer = fallbackStunServers.front();
                fallbackStunServers.removeFirst();

                struct sockaddr_in* currentAddress = new sockaddr_in;
                sockaddr_clear(currentAddress);
                /* 3478 is the standard STUN protocol port. */
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
            setNewConnectionStatus(CONNECTION_STATUS_UNKNOWN);
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Unable to find two STUN servers with which to test our connection state. Automatic connection set up disabled.");
        }
    } else if (CONNECTION_STATUS_STUNNING_INITIAL == connectionStatus) {
        int currentStatus = handleStunStep("Primary STUN server", stunServer1, udpTestSocket, ntohs(ownLocalAddress.sin_port));
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
            mUpnpMgr->setTargetPort(ntohs(ownLocalAddress.sin_port));
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
        int currentStatus = handleStunStep("Primary STUN Server", stunServer1, udpTestSocket, ntohs(ownLocalAddress.sin_port));
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
            setNewConnectionStatus(CONNECTION_STATUS_UNKNOWN);
            /* This is a special case of failure, which is total failure to receive on the main port, along with no UPNP.
               In this case, we have failed without even knowing our own external address yet.
               Therefore, for this type of failure, we'll need to signal to get the external address from LibraryMixer. */
            readyToConnectToFriends = CONNECT_FRIENDS_GET_ADDRESS_FROM_LIBRARYMIXER;
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Total failure to access the Internet using Mixologist port " + QString::number(ntohs(ownLocalAddress.sin_port)));
        }
    } else if (CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST == connectionStatus) {
        int currentStatus = handleStunStep("Primary STUN Server", stunServer1, udpTestSocket, ntohs(ownLocalAddress.sin_port));
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
            setNewConnectionStatus(CONNECTION_STATUS_UNKNOWN);
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Unexpected inability to contact primary STUN server, automatic connection configuration shutting down");
        }
    } else if (CONNECTION_STATUS_UPNP_IN_USE == connectionStatus) {
        upnpMaintenance();
    } else if (CONNECTION_STATUS_UDP_HOLE_PUNCHING == connectionStatus ||
               CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING == connectionStatus) {
        /* Note that we are treating CONNECTION_STATUS_UDP_HOLE_PUNCHING (full-cone NAT hole punching)
           the same as CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING for now.
           The full-cone NAT hole punching actually only requires punching one friend each cycle, but we are punching all friends each cycle for both. */
        udpHolePunchMaintenance();
    }
    /* For all of the following, we don't need to do anything to maintain, so we don't handle those cases:
       CONNECTION_STATUS_UNFIREWALLEDx
       CONNECTION_STATUS_PORT_FORWARDED
       CONNECTION_STATUS_SYMMETRIC_NAT
       CONNECTION_STATUS_UNKNOWN */
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

void ConnectivityManager::receivedStunBindingResponse(QString transactionId, QString mappedAddress, ushort mappedPort, ushort receivedOnPort, QString receivedFromAddress) {
    QMutexLocker stack(&connMtx);
    if (!pendingStunTransactions.contains(transactionId)) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, QString("Discarding STUN response not matching any current outstanding request."));
        return;
    }
    struct in_addr fromIP;
    inet_aton(receivedFromAddress.toStdString().c_str(), &fromIP);
    if (isSameSubnet(&(ownLocalAddress.sin_addr), &fromIP)) {
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
                "Using LibraryMixer as a fallback for the primary STUN server");
        } else {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Using LibraryMixer as a fallback for the secondary STUN server");
        }

        if (stunServer2) setNewConnectionStatus(CONNECTION_STATUS_STUNNING_INITIAL);
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_INITIAL) {
        inet_aton(mappedAddress.toStdString().c_str(), &ownExternalAddress.sin_addr);
        ownExternalAddress.sin_port = ownLocalAddress.sin_port;
        readyToConnectToFriends = CONNECT_FRIENDS_UPDATE_LIBRARYMIXER;
        if (QString(inet_ntoa(ownLocalAddress.sin_addr)) == mappedAddress) {
            setNewConnectionStatus(CONNECTION_STATUS_UNFIREWALLED);
            log(LOG_WARNING,
                CONNECTIVITY_MANAGER_ZONE,
                "No firewall detected, connection ready to go on " + mappedAddress +
                ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
        } else {
            setNewConnectionStatus(CONNECTION_STATUS_PORT_FORWARDED);
            log(LOG_WARNING,
                CONNECTIVITY_MANAGER_ZONE,
                "Detected firewall with port-forwarding already set up, connection ready to go on " + mappedAddress +
                ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
        }
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_UPNP_TEST) {
        inet_aton(mappedAddress.toStdString().c_str(), &ownExternalAddress.sin_addr);
        ownExternalAddress.sin_port = ownLocalAddress.sin_port;
        readyToConnectToFriends = CONNECT_FRIENDS_UPDATE_LIBRARYMIXER;
        setNewConnectionStatus(CONNECTION_STATUS_UPNP_IN_USE);
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            "Connection test successful, Universal Plug and Play configured connection ready to go on " + mappedAddress +
            ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_MAIN_PORT) {
        inet_aton(mappedAddress.toStdString().c_str(), &ownExternalAddress.sin_addr);
        ownExternalAddress.sin_port = htons(mappedPort);
        readyToConnectToFriends = CONNECT_FRIENDS_UPDATE_LIBRARYMIXER;
        setNewConnectionStatus(CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST);
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            "Internet connection verified on " + mappedAddress +
            ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST) {
        setNewConnectionStatus(CONNECTION_STATUS_UDP_HOLE_PUNCHING);
        log(LOG_WARNING,
            CONNECTIVITY_MANAGER_ZONE,
            "Connection test successful, punching a UDP hole in the firewall on " + mappedAddress +
            ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
    } else if (connectionStatus == CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST) {
        if (mappedPort == ntohs(ownExternalAddress.sin_port)) {
            setNewConnectionStatus(CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING);
            log(LOG_WARNING,
                CONNECTIVITY_MANAGER_ZONE,
                "Restricted cone firewall detected, punching a UDP hole (with reduced efficacy) in the firewall on " + mappedAddress +
                ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
        } else {
            setNewConnectionStatus(CONNECTION_STATUS_SYMMETRIC_NAT);
            log(LOG_WARNING,
                CONNECTIVITY_MANAGER_ZONE,
                "Symmetric NAT firewall detected, manual configuration of the firewall will be necessary to connect to friends over the Internet.");
        }
    }

    pendingStunTransactions.remove(transactionId);
}

#define UPNP_MAINTENANCE_INTERVAL 300
void ConnectivityManager::upnpMaintenance() {
    /* We only need to maintain if we're using UPNP,
       and this variable is only set true after UPNP is successfully configured. */
    static time_t last_call = 0;
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

void ConnectivityManager::udpHolePunchMaintenance() {
    static time_t last_call = 0;
    if (time(NULL) - last_call > UDP_PUNCHING_PERIOD) {
        last_call = time(NULL);
        foreach (peerConnectState currentFriend, mFriendList.values()) {
            if (currentFriend.state == FCS_NOT_CONNECTED) {
#ifdef TEST_UDP_LOCAL
                udpMainSocket->sendUdpTunneler(&currentFriend.localaddr, &ownLocalAddress, peers->getOwnLibraryMixerId());
#else
                udpMainSocket->sendUdpTunneler(&currentFriend.serveraddr, &ownExternalAddress, peers->getOwnLibraryMixerId());
#endif
            }
        }
    }
}

void ConnectivityManager::setNewConnectionStatus(ConnectionStatus newStatus) {
    connectionStatus = newStatus;
    connectionSetupStepTimeOutAt = 0;
    pendingStunTransactions.clear();
    emit connectionStateChanged(newStatus);
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
#define FRIENDS_LIST_DOWNLOAD_TIMEOUT 10
void ConnectivityManager::statusTick() {
    QList<unsigned int> friendsToConnect;
    {
        QMutexLocker stack(&connMtx);

        /* Make sure any initial preparations are completed */
        if (readyToConnectToFriends == CONNECT_FRIENDS_CONNECTION_INITIALIZING) return;
        else if (readyToConnectToFriends == CONNECT_FRIENDS_UPDATE_LIBRARYMIXER) {
            static time_t addressLastUploaded = 0;
            if (time(NULL) > addressLastUploaded + ADDRESS_UPLOAD_TIMEOUT) {
                librarymixerconnect->uploadAddress(inet_ntoa(ownLocalAddress.sin_addr), ntohs(ownLocalAddress.sin_port),
                                                   inet_ntoa(ownExternalAddress.sin_addr), ntohs(ownExternalAddress.sin_port));
                addressLastUploaded = time(NULL);
            }
            return;
        }
        else if (readyToConnectToFriends == CONNECT_FRIENDS_GET_ADDRESS_FROM_LIBRARYMIXER) {
            static time_t addressLastUploaded = 0;
            if (time(NULL) > addressLastUploaded + ADDRESS_UPLOAD_TIMEOUT) {
                /* This is only a desperate guess, but we have no idea what our external port is, so we will guess it is the same. */
                ownExternalAddress.sin_port = ownLocalAddress.sin_port;
                librarymixerconnect->uploadAddress(inet_ntoa(ownLocalAddress.sin_addr), ntohs(ownLocalAddress.sin_port),
                                                   "", ntohs(ownExternalAddress.sin_port));
                addressLastUploaded = time(NULL);
            }
            return;
        }

        time_t now = time(NULL);

        /* Check to see if we need to update our friends list from LibraryMixer.
           For the most part, we rely on friends initially signing online to connect to us, since they'll have the most updated information at that time.
           However, if that gets lost in the Internet somehow, this will give us a periodic chance to re-connect to them.
           That assumes we are able to be readily connected to by friends that come online (the first case of the if statement).
           If our inbound connectivity is somehow limited by a firewall, either by a restricted-cone NAT, where we will only receive connections
           from friends that we know about already, or a symmetric NAT, where we will not receive any inbound connections at all, then we
           need to be greatly increasing the frequency we update our friends list, so that we can try to connect to our friends outbound. */
        int updatePeriod = FRIENDS_LIST_UPDATE_PERIOD_NORMAL;
        if (CONNECTION_STATUS_UNFIREWALLED == connectionStatus ||
            CONNECTION_STATUS_PORT_FORWARDED == connectionStatus ||
            CONNECTION_STATUS_UPNP_IN_USE == connectionStatus ||
            CONNECTION_STATUS_UDP_HOLE_PUNCHING == connectionStatus ||
            CONNECTION_STATUS_UNKNOWN == connectionStatus) {
            updatePeriod = FRIENDS_LIST_UPDATE_PERIOD_NORMAL;
        } else if (CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING == connectionStatus ||
                   CONNECTION_STATUS_SYMMETRIC_NAT) {
            updatePeriod = FRIENDS_LIST_UPDATE_PERIOD_LIMITED_INBOUND;
        }
        static time_t friendsListUpdateAttemptTime = 0;
        if (now - updatePeriod > friendsListUpdateTime) {
            if (now - FRIENDS_LIST_DOWNLOAD_TIMEOUT > friendsListUpdateAttemptTime) {
                friendsListUpdateAttemptTime = time(NULL);
                librarymixerconnect->downloadFriends();
            }
        }

        time_t timeoutIfOlder = now - LAST_HEARD_TIMEOUT;

        /* Can't simply use mFriendList.values() in foreach because that generates copies of the peerConnectState,
           and we need to be able to save in the changes. */
        foreach (unsigned int librarymixer_id, mFriendList.keys()) {
            if (mFriendList[librarymixer_id].state == FCS_NOT_MIXOLOGIST_ENABLED) continue;

            /* Check if connected peers need to be timed out */
            else if (mFriendList[librarymixer_id].state == FCS_CONNECTED) {
                if (mFriendList[librarymixer_id].lastheard < timeoutIfOlder) {
                    log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE, QString("Connection with ") + mFriendList[librarymixer_id].name + " has timed out");

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

void ConnectivityManager::addressUpdatedOnLibraryMixer() {
    QMutexLocker stack(&connMtx);
    readyToConnectToFriends = CONNECT_FRIENDS_READY;
}

void ConnectivityManager::friendsListUpdated() {
    QMutexLocker stack(&connMtx);
    friendsListUpdateTime = time(NULL);
    log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE, "Updated friends list from LibraryMixer, retrying connections to friends.");
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

    if (librarymixer_id == peers->getOwnLibraryMixerId()) {
        peerConnectState ownState;
        ownState.localaddr = ownLocalAddress;
        ownState.serveraddr = ownExternalAddress;
        ownState.librarymixer_id = peers->getOwnLibraryMixerId();
        ownState.id = peers->getOwnCertId();
        state = ownState;
    } else {
        if (!mFriendList.contains(librarymixer_id)) return false;
        state = mFriendList[librarymixer_id];
    }
    return true;
}

bool ConnectivityManager::getQueuedConnectAttempt(unsigned int librarymixer_id, struct sockaddr_in &addr,
                                                  uint32_t &delay, QueuedConnectionType &queuedConnectionType) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
            QString("Can't make attempt to connect to user, not in friends list, id was: ").append(librarymixer_id));
        return false;
    }

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    if (connectState->queuedConnectionAttempts.size() < 1) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
            QString("Can't make attempt to connect to user, have no queued attempts: ").append(connectState->name));
        return false;
    }

    QueuedConnectionAttempt currentAttempt = connectState->queuedConnectionAttempts.front();
    connectState->queuedConnectionAttempts.pop_front();

    /* Test if address is in use already. */
    if (usedSockets.contains(addressToString(currentAttempt.addr))) {
        /* If we have an alternative attempt to try, simply stick this one on the back and request a try for a different attempt in the meantime. */
        if (connectState->queuedConnectionAttempts.size() >= 1) {
            connectState->queuedConnectionAttempts.push_back(currentAttempt);
            connectState->actions |= PEER_CONNECT_REQ;
            mStatusChanged = true;
        } else {
            if (usedSockets[addressToString(currentAttempt.addr)] == USED_IP_CONNECTED) {
                log(LOG_DEBUG_ALERT, CONNECTIVITY_MANAGER_ZONE,
                    "ConnectivityManager::connectAttempt Can not connect to " + addressToString(currentAttempt.addr) + " due to existing connection.");
            } else if (usedSockets[addressToString(currentAttempt.addr)] == USED_IP_CONNECTING) {
                if (connectState->currentConnectionAttempt.queuedConnectionType == CONNECTION_TYPE_TCP_LOCAL ||
                    connectState->currentConnectionAttempt.queuedConnectionType == CONNECTION_TYPE_TCP_EXTERNAL) {
                    connectState->nextTcpTryAt = time(NULL) + USED_SOCKET_WAIT_TIME;
                    log(LOG_DEBUG_BASIC, CONNECTIVITY_MANAGER_ZONE,
                        "ConnectivityManager::connectAttempt Waiting to try to connect to " + addressToString(currentAttempt.addr) + " due to existing attempted connection.");
                } else if (connectState->currentConnectionAttempt.queuedConnectionType == CONNECTION_TYPE_UDP) {
                    log(LOG_DEBUG_BASIC, CONNECTIVITY_MANAGER_ZONE,
                        "ConnectivityManager::connectAttempt Abandoning attempt to connect over UDP to " + addressToString(currentAttempt.addr) +
                        " due to existing attempted connection, will try again on next incoming UDP Tunneler.");
                }
            }
        }
        return false;
    }

    /* If we get here, the IP address is good to use, so load it in. */
    connectState->inConnectionAttempt = true;
    connectState->currentConnectionAttempt = currentAttempt;
    usedSockets[addressToString(currentAttempt.addr)] = USED_IP_CONNECTING;

    addr = connectState->currentConnectionAttempt.addr;
    delay = connectState->currentConnectionAttempt.delay;
    queuedConnectionType = connectState->currentConnectionAttempt.queuedConnectionType;

    log(LOG_DEBUG_BASIC, CONNECTIVITY_MANAGER_ZONE,
        QString("ConnectivityManager::connectAttempt Providing information for connection attempt to user: ").append(connectState->name));

    return true;
}

bool ConnectivityManager::reportConnectionUpdate(unsigned int librarymixer_id, int result, bool tcpConnection) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    mFriendList[librarymixer_id].inConnectionAttempt = false;

    if (result == 1) {
        /* No longer need any of the queued connection attempts. */
        mFriendList[librarymixer_id].queuedConnectionAttempts.clear();

        /* Mark this socket as used so no other connection attempt can try to use it. */
        usedSockets[addressToString(mFriendList[librarymixer_id].currentConnectionAttempt.addr)] = USED_IP_CONNECTED;

        log(LOG_DEBUG_BASIC, CONNECTIVITY_MANAGER_ZONE,
            QString("Successfully connected to: ") + mFriendList[librarymixer_id].name +
            " (" + inet_ntoa(mFriendList[librarymixer_id].currentConnectionAttempt.addr.sin_addr) + ")");

        /* change state */
        mFriendList[librarymixer_id].state = FCS_CONNECTED;
        mFriendList[librarymixer_id].actions |= PEER_CONNECTED;
        mStatusChanged = true;
        mFriendList[librarymixer_id].lastcontact = time(NULL);
        mFriendList[librarymixer_id].lastheard = time(NULL);
    } else {
        log(LOG_DEBUG_BASIC,
            CONNECTIVITY_MANAGER_ZONE,
            QString("Unable to connect to friend: ") + mFriendList[librarymixer_id].name +
            ", over transport layer type: " + QString::number(tcpConnection));

        usedSockets.remove(addressToString(mFriendList[librarymixer_id].currentConnectionAttempt.addr));

        /* We may receive a failure report if we were connected and the connection failed. */
        if (mFriendList[librarymixer_id].state == FCS_CONNECTED) {
            mFriendList[librarymixer_id].lastcontact = time(NULL);
            mFriendList[librarymixer_id].state = FCS_NOT_CONNECTED;
        }

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
    inet_aton(address.toStdString().c_str(), &ownExternalAddress.sin_addr);
}

void ConnectivityManager::queueConnectionAttempt(peerConnectState *connectState, QueuedConnectionType queuedConnectionType) {
    /* Only add this address if there is not already one of that type on there already.
       First check if we're in a current connection attempt with that type.
       Then check if any of the queued connection attempts are for that type. */
    if (connectState->inConnectionAttempt) {
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
        CONNECTIVITY_MANAGER_ZONE,
        QString("ConnectivityManager::queueConnectionAttempt") +
        " Attempting connection to friend #" + QString::number(connectState->librarymixer_id) +
        " with IP: " + addressToString(addressToConnect.addr));

    connectState->queuedConnectionAttempts.push_back(addressToConnect);
}

bool ConnectivityManager::tryConnectTCP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    /* We can now consider this attempt tried so that tick doesn't try to schedule again, and increment the next try time. */
    connectState->nextTcpTryAt = time(NULL) + TCP_RETRY_PERIOD;

    if (connectState->state == FCS_CONNECTED) return true;

#ifndef NO_TCP_CONNECTIONS

    std::string localIP = inet_ntoa(connectState->localaddr.sin_addr);
    std::string externalIP = inet_ntoa(connectState->serveraddr.sin_addr);

    /* If address is valid, on the same subnet, not the same as external address, and not the same as own addresses, add it as a local address to try. */
    if (isValidNet(&(connectState->localaddr.sin_addr)) &&
        isSameSubnet(&(ownLocalAddress.sin_addr), &(connectState->localaddr.sin_addr)) &&
        (localIP != externalIP) &&
        (!isSameAddress(&ownLocalAddress, &connectState->localaddr)) &&
        (!isSameAddress(&ownExternalAddress, &connectState->localaddr))) {
        queueConnectionAttempt(connectState, CONNECTION_TYPE_TCP_LOCAL);
    }

    /* Always try external unless address is the same as one of ours. */
    if (isValidNet(&(connectState->serveraddr.sin_addr)) &&
        (!isSameAddress(&ownLocalAddress, &connectState->serveraddr)) &&
        (!isSameAddress(&ownExternalAddress, &connectState->serveraddr))) {
        queueConnectionAttempt(connectState, CONNECTION_TYPE_TCP_EXTERNAL);
    }

    /* If it's already in a connection attempt, it'll automatically use the new addresses. */
    if (connectState->inConnectionAttempt) return true;

    /* Start a connection attempt. */
    if (connectState->queuedConnectionAttempts.size() > 0) {
        connectState->actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }

#endif // NO_TCP_CONNECTIONS

    return true;
}

bool ConnectivityManager::tryConnectBackTCP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    if (connectState->state == FCS_CONNECTED) return true;

#ifndef NO_TCP_BACK_CONNECTIONS
    queueConnectionAttempt(connectState, CONNECTION_TYPE_TCP_BACK);

    /* If it's already in a connection attempt, it'll automatically use the new addresses. */
    if (connectState->inConnectionAttempt) return true;

    /* Start a connection attempt. */
    if (connectState->queuedConnectionAttempts.size() > 0) {
        connectState->actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }
#endif
    return true;
}

bool ConnectivityManager::tryConnectUDP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) return false;

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    if (connectState->state == FCS_CONNECTED) return true;

#ifndef NO_UDP_CONNECTIONS
    queueConnectionAttempt(connectState, CONNECTION_TYPE_UDP);

    /* If it's already in a connection attempt, it'll automatically use the new addresses. */
    if (connectState->inConnectionAttempt) return true;

    /* Start a connection attempt. */
    if (connectState->queuedConnectionAttempts.size() > 0) {
        connectState->actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    }
#endif //NO_UDP_CONNECTIONS

    return true;
}

void ConnectivityManager::receivedUdpTunneler(unsigned int librarymixer_id, QString address, ushort port) {
    {
        QMutexLocker stack(&connMtx);
        if (readyToConnectToFriends != CONNECT_FRIENDS_READY) return;

        if (!mFriendList.contains(librarymixer_id)) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Received a UDP packet from an unknown user with LibraryMixer ID " + QString::number(librarymixer_id) +
                ", updating friend list");
            librarymixerconnect->downloadFriends();
            return;
        }

#ifdef TEST_UDP_LOCAL
        if (QString(inet_ntoa(mFriendList[librarymixer_id].localaddr.sin_addr)) != address ||
            ntohs(mFriendList[librarymixer_id].localaddr.sin_port) != port) {
#else
        if (QString(inet_ntoa(mFriendList[librarymixer_id].serveraddr.sin_addr)) != address ||
            ntohs(mFriendList[librarymixer_id].serveraddr.sin_port) != port) {
#endif
            librarymixerconnect->downloadFriends();
            return;
        }
    }

    if (connectionStatus != CONNECTION_STATUS_UDP_HOLE_PUNCHING &&
        connectionStatus != CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING) {
        log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
            "Received a UDP packet from a friend but do not believe ourselves to be firewalled, requesting TCP connect back");
        queueConnectionAttempt(&mFriendList[librarymixer_id], CONNECTION_TYPE_TCP_BACK);
    }

#ifdef TEST_UDP_LOCAL
    udpMainSocket->sendUdpConnectionNotice(&mFriendList[librarymixer_id].localaddr, &ownLocalAddress, peers->getOwnLibraryMixerId());
#else
    udpMainSocket->sendUdpConnectionNotice(&mFriendList[librarymixer_id].serveraddr, &ownExternalAddress, peers->getOwnLibraryMixerId());
#endif
    tryConnectUDP(librarymixer_id);
}

void ConnectivityManager::receivedUdpConnectionNotice(unsigned int librarymixer_id, QString address, ushort port) {
    {
        /* UdpConnectionNotices should only be received in response to UdpTunnelers we sent.
           Receiving one where any information is out of the ordinary is completely unexpected,
           and likely indicates some sort of suspicious activity.
           We don't even update our friends list for this, because there is no scenario where our friends list is out-of-date
           that would result in this scenario. */
        QMutexLocker stack(&connMtx);
        if (readyToConnectToFriends != CONNECT_FRIENDS_READY) return;

        if (!mFriendList.contains(librarymixer_id)) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Received a UDP connection notification from an unknown user with LibraryMixer ID " + QString::number(librarymixer_id) +
                ", this is unexpected, ignoring");
            return;
        }

#ifdef TEST_UDP_LOCAL
        if (QString(inet_ntoa(mFriendList[librarymixer_id].localaddr.sin_addr)) != address ||
            ntohs(mFriendList[librarymixer_id].localaddr.sin_port) != port) {
#else
        if (QString(inet_ntoa(mFriendList[librarymixer_id].serveraddr.sin_addr)) != address ||
            ntohs(mFriendList[librarymixer_id].serveraddr.sin_port) != port) {
#endif
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Received a UDP connection notification that contains invalid data, ignoring");
            return;
        }
    }

    tryConnectUDP(librarymixer_id);
}

void ConnectivityManager::receivedTcpConnectionRequest(unsigned int librarymixer_id, QString address, ushort port) {
    {
        QMutexLocker stack(&connMtx);
        if (readyToConnectToFriends != CONNECT_FRIENDS_READY) return;

        if (!mFriendList.contains(librarymixer_id)) {
            log(LOG_WARNING, CONNECTIVITY_MANAGER_ZONE,
                "Received a request to connect from an unknown user with LibraryMixer ID " + QString::number(librarymixer_id) +
                ", updating friend list");
            librarymixerconnect->downloadFriends();
            return;
        }

        if (QString(inet_ntoa(mFriendList[librarymixer_id].serveraddr.sin_addr)) != address ||
            ntohs(mFriendList[librarymixer_id].serveraddr.sin_port) != port) {
            librarymixerconnect->downloadFriends();
            return;
        }
    }
    tryConnectTCP(librarymixer_id);
}

/**********************************************************************************
 * Body of public-facing API functions called through p3peers
 **********************************************************************************/
void ConnectivityManager::getOnlineList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        if (mFriendList[friend_id].state == FCS_CONNECTED)
            peers.push_back(friend_id);
    }
}

void ConnectivityManager::getSignedUpList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    foreach (unsigned int friend_id, mFriendList.keys()) {
        if (mFriendList[friend_id].state != FCS_NOT_MIXOLOGIST_ENABLED)
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
        return (mFriendList[librarymixer_id].state == FCS_CONNECTED);
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
        peer.state = FCS_NOT_CONNECTED;
        //      peer.actions = PEER_MOVED;
        peer.inConnectionAttempt = false;
        mStatusChanged = true;
        authMgr->RemoveCertificate(id);

        success = true;
    }

    return success;
}
#endif

void ConnectivityManager::tryConnectAll() {
    if (readyToConnectToFriends != CONNECT_FRIENDS_READY) return;

    foreach (unsigned int librarymixer_id, mFriendList.keys()) {
        tryConnectTCP(librarymixer_id);
        tryConnectBackTCP(librarymixer_id);
    }
}

/**********************************************************************************
 * Utility methods
 **********************************************************************************/

int ConnectivityManager::getLocalPort() {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    if (settings.contains("Network/PortNumber")) {
        int storedPort = settings.value("Network/PortNumber", DEFAULT_PORT).toInt();
        if (storedPort > Peers::MIN_PORT &&
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
