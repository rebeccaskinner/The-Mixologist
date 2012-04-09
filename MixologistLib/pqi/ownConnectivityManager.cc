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

#include <pqi/ownConnectivityManager.h>
#include <pqi/friendsConnectivityManager.h>
#include <pqi/pqinotify.h>
#include <pqi/aggregatedConnections.h>

#include <interface/peers.h>
#include <interface/settings.h>
#include <interface/librarymixer-connect.h>

#include <util/debug.h>

#define UDP_PUNCHING_PERIOD 20 //20 seconds between UDP hole punches
#define UPNP_INIT_TIMEOUT 10 //10 second timeout for UPNP to initialize the firewall
#define STUN_TEST_TIMEOUT 5 //5 second timeout from a STUN request for a STUN response to be received
#define STUN_TEST_TERMINAL_TIMEOUT 15 //15 second timeout from a STUN request on terminal steps as there is no real harm in delaying at that point

OwnConnectivityManager::OwnConnectivityManager()
    :connectionStatus(CONNECTION_STATUS_FINDING_STUN_FRIENDS),
     contactLibraryMixerState(CONTACT_LIBRARYMIXER_PENDING),
     connectionSetupStepTimeOutAt(0),
     udpTestSocket(NULL), mUpnpMgr(NULL),
     stunServer1(NULL), stunServer2(NULL) {

    sockaddr_clear(&ownLocalAddress);
    sockaddr_clear(&ownExternalAddress);

    connect(librarymixerconnect, SIGNAL(uploadedAddress()), this, SLOT(addressUpdatedOnLibraryMixer()));
    connect(this, SIGNAL(ownConnectionReadinessChanged(bool)), this, SLOT(checkUpnpMappings(bool)));

    /* Load balance between the servers by picking the order at random. */
    if (rand() % 2 == 0) {
        fallbackStunServers.append("stun1.librarymixer.com");
        fallbackStunServers.append("stun2.librarymixer.com");
    } else {
        fallbackStunServers.append("stun2.librarymixer.com");
        fallbackStunServers.append("stun1.librarymixer.com");
    }
}

void OwnConnectivityManager::tick() {
    netTick();
    contactLibraryMixerTick();
    checkNetInterfacesTick();
}

/**********************************************************************************
 * Connection Setup
 **********************************************************************************/

void OwnConnectivityManager::select_NetInterface_OpenPorts() {
    {
        QMutexLocker stack(&ownConMtx);

        QSettings settings(*mainSettings, QSettings::IniFormat, this);
        autoConfigEnabled = (settings.value("Network/AutoOrPort", DEFAULT_NETWORK_AUTO_OR_PORT) == DEFAULT_NETWORK_AUTO_OR_PORT);

        /* Set the network interface we will use for the Mixologist. */
        networkInterfaceList = getLocalInterfaces();
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
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Unable to open UDP port " + QString::number(ntohs(udpTestAddress.sin_port)));
                continue;
            } else {
               log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Opened UDP test port on " + QString::number(ntohs(udpTestAddress.sin_port)));
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
            log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Opened UDP main port on " + QString::number(ntohs(ownLocalAddress.sin_port)));
            connect(udpMainSocket, SIGNAL(receivedStunBindingResponse(QString,QString,ushort,ushort, QString)),
                    this, SLOT(receivedStunBindingResponse(QString,QString,ushort,ushort,QString)));

            /* This is ugly here, but we can't put this in the constructor for friendsConnectivityManager because this doesn't run until much later. */
            connect(udpMainSocket, SIGNAL(receivedUdpTunneler(uint,QString,ushort)),
                    friendsConnectivityManager, SLOT(receivedUdpTunneler(uint,QString,ushort)));
            connect(udpMainSocket, SIGNAL(receivedUdpConnectionNotice(uint,QString,ushort)),
                    friendsConnectivityManager, SLOT(receivedUdpConnectionNotice(uint,QString,ushort)));
            connect(udpMainSocket, SIGNAL(receivedTcpConnectionRequest(uint,QString,ushort)),
                    friendsConnectivityManager, SLOT(receivedTcpConnectionRequest(uint,QString,ushort)));

        }

        aggregatedConnectionsToFriends->init_listener();

        if (!mUpnpMgr) mUpnpMgr = new upnpHandler();
        mUpnpMgr->setTargetPort(ntohs(ownLocalAddress.sin_port));
    }
}

void OwnConnectivityManager::shutdown() {
    QMutexLocker stack(&ownConMtx);
    if (mUpnpMgr) mUpnpMgr->shutdown();

    TCP_over_UDP_shutdown();

    delete udpMainSocket;
    udpMainSocket = NULL;
    delete udpTestSocket;
    udpTestSocket = NULL;

    aggregatedConnectionsToFriends->stop_listener();

    sockaddr_clear(&ownLocalAddress);
    sockaddr_clear(&ownExternalAddress);

    connectionStatus = CONNECTION_STATUS_UNKNOWN;
    contactLibraryMixerState = CONTACT_LIBRARYMIXER_PENDING;
    connectionSetupStepTimeOutAt = 0;

    delete stunServer1;
    stunServer1 = NULL;
    delete stunServer2;
    stunServer2 = NULL;
}

void OwnConnectivityManager::netTick() {
    bool connectionStatusChanged = false;
    ConnectionStatus currentStatus;
    {
        QMutexLocker stack(&ownConMtx);
        if (CONNECTION_STATUS_FINDING_STUN_FRIENDS == connectionStatus) {
            if (connectionSetupStepTimeOutAt == 0) {
                if (autoConfigEnabled) log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Initiating automatic connection configuration");
                else log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Testing manual connection configuration");
    
                QList<struct sockaddr_in> friendsToTry;
                friendsConnectivityManager->getExternalAddresses(friendsToTry);
                foreach (struct sockaddr_in address, friendsToTry) {
                    if ((stunServer1 && stunServer2) ||
                        (!autoConfigEnabled && stunServer1))
                        break;
                    sendStunPacket(addressToString(&address), &address, udpTestSocket);
                }
    
                connectionSetupStepTimeOutAt = time(NULL) + STUN_TEST_TIMEOUT;
            } else if (time(NULL) > connectionSetupStepTimeOutAt) {
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS);
            }
        } else if (CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS == connectionStatus) {
            if (connectionSetupStepTimeOutAt == 0) {
                bool dnsFail = false;
    
                foreach (QString currentServer, fallbackStunServers) {
                    struct sockaddr_in* currentAddress = new sockaddr_in;
                    sockaddr_clear(currentAddress);
                    /* 3478 is the standard STUN protocol port. */
                    currentAddress->sin_port = htons(3478);
                    if (!LookupDNSAddr(currentServer.toStdString(), currentAddress)) {
                        dnsFail = true;
                    }
                    sendStunPacket(currentServer, currentAddress, udpTestSocket);
                }
    
                if (dnsFail) getPqiNotify()->AddSysMessage(SYS_INFO, "Network failure", "Unable to reach DNS, you may not have Internet access");
    
                connectionSetupStepTimeOutAt = time(NULL) + STUN_TEST_TERMINAL_TIMEOUT;
            } else if (time(NULL) > connectionSetupStepTimeOutAt) {
                /* If we can't get enough STUN servers to configure just give up and assume the connection is already configured. */
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_UNKNOWN);

                if (autoConfigEnabled) {
                    log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                        "Unable to find two STUN servers with which to test our connection state. Automatic connection set up disabled.");
                } else {
                    log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                        "Unable to find a STUN servers with which to test our connection state.");
                }
            }
        } else if (CONNECTION_STATUS_STUNNING_INITIAL == connectionStatus) {
            int currentStatus;
            if (autoConfigEnabled) currentStatus = handleStunStep("Primary STUN server", stunServer1, udpTestSocket, ntohs(ownLocalAddress.sin_port), false);
            else currentStatus = handleStunStep("Primary STUN server", stunServer1, udpTestSocket, ntohs(ownLocalAddress.sin_port), true);
    
            if (currentStatus == 1) {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Contacting primary STUN server to determine if we are behind a firewall");
            } else if (currentStatus == -1) {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "No response from primary stun server, response likely blocked by firewall");
                if (autoConfigEnabled) connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_TRYING_UPNP);
                else {
                    connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_UNKNOWN);
                    /* This is a special case of failure, in that we have failed without even knowing our own external address yet,
                       and auto-config is disabled so there will be no further ability to find it.
                       Therefore, for this type of failure, we'll need to signal to get the external address from LibraryMixer. */
                    contactLibraryMixerState = CONTACT_LIBRARYMIXER_GET_ADDRESS_FROM_LIBRARYMIXER;
                }
            }
        } else if (CONNECTION_STATUS_TRYING_UPNP == connectionStatus) {
            upnpHandler::upnpStates currentUpnpState = mUpnpMgr->getUpnpState();

            if (currentUpnpState == upnpHandler::UPNP_STATE_UNINITIALIZED) {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                    "Attempting to configure firewall using Universal Plug and Play");
                mUpnpMgr->startup();
                connectionSetupStepTimeOutAt = time(NULL) + UPNP_INIT_TIMEOUT;
            }
    
            if (currentUpnpState == upnpHandler::UPNP_STATE_ACTIVE) {
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_STUNNING_UPNP_TEST);
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                    "Universal Plug and Play successfully configured router");
            } else {
                if (currentUpnpState == upnpHandler::UPNP_STATE_UNAVAILABLE ||
                    currentUpnpState == upnpHandler::UPNP_STATE_FAILED ||
                    time(NULL) > connectionSetupStepTimeOutAt) {
    
                    mUpnpMgr->shutdown();
    
                    connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_STUNNING_MAIN_PORT);
    
                    if (currentUpnpState == upnpHandler::UPNP_STATE_UNAVAILABLE) {
                        log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "No Universal Plug and Play-compatible router detected");
                    } else {
                        log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Unable to configure router using Universal Plug and Play");
                    }
                }
            }
        } else if (CONNECTION_STATUS_STUNNING_UPNP_TEST == connectionStatus) {
            int currentStatus = handleStunStep("Primary STUN Server", stunServer1, udpTestSocket, ntohs(ownLocalAddress.sin_port));
    
            if (currentStatus == 1) {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Contacting primary STUN server to test Universal Plug and Play configuration");
            } else if (currentStatus == -1) {
                mUpnpMgr->shutdown();
    
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_STUNNING_MAIN_PORT);
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                    "Connection test failed despite successfully configured connection, abandoning Universal Plug and Play approach");
            }
        } else if (CONNECTION_STATUS_STUNNING_MAIN_PORT == connectionStatus) {
            int currentStatus = handleStunStep("Secondary STUN Server", stunServer2, udpMainSocket);
            if (currentStatus == 1) {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Contacting secondary STUN server to verify whether we have any Internet connection at all");
            }
            if (currentStatus == -1) {
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_UNKNOWN);
    
                /* This is a special case of failure, which is total failure to receive on the main port, along with no UPNP.
                   In this case, we have failed without even knowing our own external address yet.
                   Therefore, for this type of failure, we'll need to signal to get the external address from LibraryMixer. */
                contactLibraryMixerState = CONTACT_LIBRARYMIXER_GET_ADDRESS_FROM_LIBRARYMIXER;
    
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                    "Total failure to access the Internet using Mixologist port " + QString::number(ntohs(ownLocalAddress.sin_port)));
            }
        } else if (CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST == connectionStatus) {
            int currentStatus = handleStunStep("Primary STUN Server", stunServer1, udpTestSocket, ntohs(ownLocalAddress.sin_port));
            if (currentStatus == 1) {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                    "Contacting primary STUN server to test whether we can punch a hole through the firewall using UDP");
            } else if (currentStatus == -1) {
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST);
            }
        } else if (CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST == connectionStatus) {
            int currentStatus = handleStunStep("Primary STUN Server", stunServer1, udpMainSocket);
            if (currentStatus == 1) {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                    "Contacting primary STUN server to further probe the nature of the firewall");
            } else if (currentStatus == -1) {
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_UNKNOWN);
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                    "Unexpected inability to contact primary STUN server, automatic connection configuration shutting down");
            }
        } else if (CONNECTION_STATUS_UPNP_IN_USE == connectionStatus) {
            upnpMaintenance();
        }
        /* UDP hole punching, is both of the nature of personal connectivity maintenance, as well as friend connectivity,
           becuase each of the UdpTunneler packets that we send to punch a hole in the firewall also serve as connection invitations.
           Because the FriendConnectivityManager holds our friendsList, it's a little simpler to handle UDP hole punching there.
           Therefore, the following states don't require any maintenance in the ownConnectivityManager, so we don't handle those here:
           CONNECTION_STATUS_UDP_HOLE_PUNCHING
           CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING */
        /* For all of the following, we don't need to do anything to maintain, so we don't handle those cases:
           CONNECTION_STATUS_UNFIREWALLED
           CONNECTION_STATUS_PORT_FORWARDED
           CONNECTION_STATUS_SYMMETRIC_NAT
           CONNECTION_STATUS_UNKNOWN */

        currentStatus = connectionStatus;
    }

    if (connectionStatusChanged) sendSignals(currentStatus);
}

int OwnConnectivityManager::handleStunStep(const QString& stunServerName, const sockaddr_in *stunServer, UdpSorter *sendSocket,
                                           ushort returnPort, bool extendedTimeout) {
    if (connectionSetupStepTimeOutAt != 0) {
        if (time(NULL) > connectionSetupStepTimeOutAt) return -1;
    } else {
        if (sendStunPacket(stunServerName, stunServer, sendSocket, returnPort)) {
            if (extendedTimeout) connectionSetupStepTimeOutAt = time(NULL) + STUN_TEST_TERMINAL_TIMEOUT;
            else connectionSetupStepTimeOutAt = time(NULL) + STUN_TEST_TIMEOUT;
            return 1;
        }
    }
    return 0;
}

bool OwnConnectivityManager::sendStunPacket(const QString& stunServerName, const sockaddr_in *stunServer, UdpSorter *sendSocket, ushort returnPort) {
    QString stunTransactionId = UdpStun_generate_transaction_id();
    pendingStunTransaction transaction;
    transaction.returnPort = returnPort;
    transaction.serverAddress = *stunServer;
    transaction.serverName = stunServerName;
    pendingStunTransactions[stunTransactionId] = transaction;
    if (sendSocket) return sendSocket->sendStunBindingRequest(stunServer, stunTransactionId, returnPort);
    else return false;
}

void OwnConnectivityManager::receivedStunBindingResponse(QString transactionId, QString mappedAddress, ushort mappedPort,
                                                         ushort receivedOnPort, QString receivedFromAddress) {
    ConnectionStatus currentStatus;
    bool connectionStatusChanged = false;
    {
        QMutexLocker stack(&ownConMtx);
        if (!pendingStunTransactions.contains(transactionId)) {
            log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                QString("Discarding STUN response not matching any current outstanding request from " + receivedFromAddress));
            return;
        }
        struct in_addr fromIP;
        inet_aton(receivedFromAddress.toStdString().c_str(), &fromIP);
        if (isSameSubnet(&(ownLocalAddress.sin_addr), &fromIP)) {
            log(LOG_DEBUG_ALERT, OWN_CONNECTIVITY_ZONE, "Discarding STUN response received on own subnet");
            return;
        }

        /* For the error of receiving on the wrong port, it should be generally a safe assumption the only time this could occur
           would be if the STUN server ignores the response-port attribute.
           If the expected port is set to 0, then we can assume that this is on the correct port.
           Furthermore, we know this will be stunServer1, because stunServer2 is never sent any requests with the response-port attribute. */
        if (pendingStunTransactions[transactionId].returnPort != receivedOnPort &&
            pendingStunTransactions[transactionId].returnPort != 0) {
            log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                "Error in STUN, STUN server (" + addressToString(stunServer1) + ") ignored the response-port attribute.");
            return;
        }

        if (connectionStatus == CONNECTION_STATUS_FINDING_STUN_FRIENDS) {
            struct sockaddr_in** targetStunServer;
            if (!stunServer1) targetStunServer = &stunServer1;
            else if (autoConfigEnabled && !stunServer2) targetStunServer = &stunServer2;
            else return;

            *targetStunServer = new sockaddr_in;
            **targetStunServer = pendingStunTransactions[transactionId].serverAddress;

            log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                "Using " + pendingStunTransactions[transactionId].serverName + " as a STUN server");

            if (stunServer2 ||
                (stunServer1 && !autoConfigEnabled)) connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_STUNNING_INITIAL);
        } else if (connectionStatus == CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS) {
            struct sockaddr_in** targetStunServer;
            if (!stunServer1) targetStunServer = &stunServer1;
            else if (autoConfigEnabled && !stunServer2) targetStunServer = &stunServer2;
            else return;

            *targetStunServer = new sockaddr_in;
            **targetStunServer = pendingStunTransactions[transactionId].serverAddress;

            if (targetStunServer == &stunServer1) {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                    "Using LibraryMixer as a fallback for the primary STUN server");
            } else {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE,
                    "Using LibraryMixer as a fallback for the secondary STUN server");
            }

            if (stunServer2 ||
                (stunServer1 && !autoConfigEnabled)) connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_STUNNING_INITIAL);
        } else if (connectionStatus == CONNECTION_STATUS_STUNNING_INITIAL) {
            inet_aton(mappedAddress.toStdString().c_str(), &ownExternalAddress.sin_addr);
            ownExternalAddress.sin_port = ownLocalAddress.sin_port;
            //We now know our own address definitively, and should update LibraryMixer with it
            contactLibraryMixerState = CONTACT_LIBRARYMIXER_UPDATE;
            if (QString(inet_ntoa(ownLocalAddress.sin_addr)) == mappedAddress) {
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_UNFIREWALLED);
                log(LOG_WARNING,
                    OWN_CONNECTIVITY_ZONE,
                    "No firewall detected, connection ready to go on " + mappedAddress +
                    ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
            } else {
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_PORT_FORWARDED);
                log(LOG_WARNING,
                    OWN_CONNECTIVITY_ZONE,
                    "Detected firewall with port-forwarding already set up, connection ready to go on " + mappedAddress +
                    ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
            }
        } else if (connectionStatus == CONNECTION_STATUS_STUNNING_UPNP_TEST) {
            inet_aton(mappedAddress.toStdString().c_str(), &ownExternalAddress.sin_addr);
            ownExternalAddress.sin_port = ownLocalAddress.sin_port;
            //We now know our own address definitively, and should update LibraryMixer with it
            contactLibraryMixerState = CONTACT_LIBRARYMIXER_UPDATE;
            connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_UPNP_IN_USE);
            log(LOG_WARNING,
                OWN_CONNECTIVITY_ZONE,
                "Connection test successful, Universal Plug and Play configured connection ready to go on " + mappedAddress +
                ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
        } else if (connectionStatus == CONNECTION_STATUS_STUNNING_MAIN_PORT) {
            inet_aton(mappedAddress.toStdString().c_str(), &ownExternalAddress.sin_addr);
            ownExternalAddress.sin_port = htons(mappedPort);
            //We now know our own address definitively, and should update LibraryMixer with it
            contactLibraryMixerState = CONTACT_LIBRARYMIXER_UPDATE;
            connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST);
            log(LOG_WARNING,
                OWN_CONNECTIVITY_ZONE,
                "Internet connection verified on " + mappedAddress +
                ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
        } else if (connectionStatus == CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST) {
            connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_UDP_HOLE_PUNCHING);
            log(LOG_WARNING,
                OWN_CONNECTIVITY_ZONE,
                "Connection test successful, punching a UDP hole in the firewall on " + mappedAddress +
                ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
        } else if (connectionStatus == CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST) {
            if (mappedPort == ntohs(ownExternalAddress.sin_port)) {
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING);
                log(LOG_WARNING,
                    OWN_CONNECTIVITY_ZONE,
                    "Restricted cone firewall detected, punching a UDP hole (with reduced efficacy) in the firewall on " + mappedAddress +
                    ":" + QString::number(ntohs(ownExternalAddress.sin_port)));
            } else {
                connectionStatusChanged = setNewConnectionStatus(CONNECTION_STATUS_SYMMETRIC_NAT);
                log(LOG_WARNING,
                    OWN_CONNECTIVITY_ZONE,
                    "Symmetric NAT firewall detected, manual configuration of the firewall will be necessary to connect to friends over the Internet.");
            }
        }

        pendingStunTransactions.remove(transactionId);

        currentStatus = connectionStatus;
    }

    if (connectionStatusChanged) sendSignals(currentStatus);
}

#define UPNP_MAINTENANCE_INTERVAL 300
void OwnConnectivityManager::upnpMaintenance() {
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

bool OwnConnectivityManager::setNewConnectionStatus(ConnectionStatus newStatus) {
    connectionStatus = newStatus;
    connectionSetupStepTimeOutAt = 0;
    pendingStunTransactions.clear();

    return true;
}

void OwnConnectivityManager::sendSignals(ConnectionStatus newStatus) {
    bool connectionReady = false;
    {
        QMutexLocker stack(&ownConMtx);
        /* Our connection is not ready when we're both in a final state and we've updated LibraryMixer with our address.
           Therefore, whenever either of these becomes true, we should check if the other is true and we need to emit the signal. */
        if (connectionStatusInFinalState(connectionStatus)) {
            if (contactLibraryMixerState == CONTACT_LIBRARYMIXER_DONE) connectionReady = true;
        }
    }
    emit connectionStateChanged(newStatus, autoConfigEnabled);
    if (connectionReady) emit ownConnectionReadinessChanged(true);
}

void OwnConnectivityManager::addressUpdatedOnLibraryMixer() {
    bool connectionReady = false;
    {
        QMutexLocker stack(&ownConMtx);
        contactLibraryMixerState = CONTACT_LIBRARYMIXER_DONE;

        /* Our connection is not ready when we're both in a final state and we've updated LibraryMixer with our address.
           Therefore, whenever either of these becomes true, we should check if the other is true and we need to emit the signal. */
        if (connectionStatusInFinalState(connectionStatus)) {
            connectionReady = true;
        }
    }
    if (connectionReady) emit ownConnectionReadinessChanged(true);
}

#define ADDRESS_UPLOAD_TIMEOUT 15
void OwnConnectivityManager::contactLibraryMixerTick() {
    QMutexLocker stack(&ownConMtx);
    if (contactLibraryMixerState == CONTACT_LIBRARYMIXER_UPDATE) {
        static time_t addressLastUploaded = 0;
        if (time(NULL) > addressLastUploaded + ADDRESS_UPLOAD_TIMEOUT) {
            librarymixerconnect->uploadAddress(inet_ntoa(ownLocalAddress.sin_addr), ntohs(ownLocalAddress.sin_port),
                                               inet_ntoa(ownExternalAddress.sin_addr), ntohs(ownExternalAddress.sin_port));
            addressLastUploaded = time(NULL);
        }
    }
    else if (contactLibraryMixerState == CONTACT_LIBRARYMIXER_GET_ADDRESS_FROM_LIBRARYMIXER) {
        static time_t addressLastUploaded = 0;
        if (time(NULL) > addressLastUploaded + ADDRESS_UPLOAD_TIMEOUT) {
            /* This is only a desperate guess, but we have no idea what our external port is, so we will guess it is the same. */
            ownExternalAddress.sin_port = ownLocalAddress.sin_port;
            librarymixerconnect->uploadAddress(inet_ntoa(ownLocalAddress.sin_addr), ntohs(ownLocalAddress.sin_port),
                                               "", ntohs(ownExternalAddress.sin_port));
            addressLastUploaded = time(NULL);
        }
    }
}

void OwnConnectivityManager::setFallbackExternalIP(QString address) {
    QMutexLocker stack(&ownConMtx);
    log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Setting external IP address based on what LibraryMixer sees");
    inet_aton(address.toStdString().c_str(), &ownExternalAddress.sin_addr);
}

void OwnConnectivityManager::checkUpnpMappings(bool check) {
    if (!check) return;
    QMutexLocker stack(&ownConMtx);
    if (mUpnpMgr) mUpnpMgr->checkMappings();
}

#define NET_INTERFACE_CHECK_INTERVAL 10 //Every 10 seconds
#define SLEEP_MODE_DETECTION_SENSITIVITY 30 //30 second gap will indicate we were in sleep mode
void OwnConnectivityManager::checkNetInterfacesTick() {
    static time_t lastChecked = time(NULL);
    static time_t lastTick = time(NULL);
    time_t now = time(NULL);

    bool resetNeeded = false;
    {
        QMutexLocker stack(&ownConMtx);

        if (now > lastTick + SLEEP_MODE_DETECTION_SENSITIVITY) {
            log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Detected computer was asleep, re-detecting connection and updating friends list");
            resetNeeded = true;
        }

        if (now > lastChecked + NET_INTERFACE_CHECK_INTERVAL)  {
            lastChecked = now;

            /* Don't do anything if we have an empty local address, which indicates we are in a connectivity shutdown state. */
            if (ownLocalAddress.sin_addr.s_addr == 0) return;

            /* If the network interface list has changed and we have a bad address already, or our local address is no longer present on this computer,
               shutdown our connections, and then restart everything from scratch. */
            if (networkInterfaceList != getLocalInterfaces()) {
                log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Detected change in network interface list");
                if (isLoopbackNet(&ownLocalAddress.sin_addr) || !interfaceStillExists(&ownLocalAddress.sin_addr))
                    resetNeeded = true;
            }
        }

        lastTick = now;
    }

    if (resetNeeded) restartOwnConnection();
}

/**********************************************************************************
 * Body of public-facing API functions called through p3peers
 **********************************************************************************/
bool OwnConnectivityManager::getConnectionReadiness() {
    QMutexLocker stack(&ownConMtx);
    return (connectionStatusInFinalState(connectionStatus) && (contactLibraryMixerState == CONTACT_LIBRARYMIXER_DONE));
}

bool OwnConnectivityManager::getConnectionAutoConfigEnabled() {
    QMutexLocker stack(&ownConMtx);
    return autoConfigEnabled;
}

ConnectionStatus OwnConnectivityManager::getConnectionStatus() {
    QMutexLocker stack(&ownConMtx);
    return connectionStatus;
}

void OwnConnectivityManager::restartOwnConnection() {
    log(LOG_WARNING, OWN_CONNECTIVITY_ZONE, "Reseting network configuration");
    friendsConnectivityManager->disconnectAllFriends();
    shutdown();
    {
        QMutexLocker stack(&ownConMtx);
        setNewConnectionStatus(CONNECTION_STATUS_FINDING_STUN_FRIENDS);
    }
    emit connectionStateChanged(CONNECTION_STATUS_FINDING_STUN_FRIENDS, autoConfigEnabled);
    emit ownConnectionReadinessChanged(false);
    select_NetInterface_OpenPorts();
}


/**********************************************************************************
 * Utility methods
 **********************************************************************************/

int OwnConnectivityManager::getLocalPort() {
    if (!autoConfigEnabled) {
        QSettings settings(*mainSettings, QSettings::IniFormat, this);
        int storedPort = settings.value("Network/AutoOrPort", DEFAULT_NETWORK_AUTO_OR_PORT).toInt();
        if (storedPort > Peers::MIN_PORT &&
            storedPort < Peers::MAX_PORT) return storedPort;
    }

    int randomPort = getRandomPortNumber();
    return randomPort;
}

#define MIN_RAND_PORT 10000
#define MAX_RAND_PORT 30000
int OwnConnectivityManager::getRandomPortNumber() const {
    return ((qrand() % (MAX_RAND_PORT - MIN_RAND_PORT)) + MIN_RAND_PORT);
}
