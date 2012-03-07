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

#include "pqi/connectivitymanager.h"
#include "pqi/pqi_base.h"
#include "tcponudp/tou.h"

#include "upnp/upnphandler.h"
#include "pqi/p3dhtmgr.h"
#include "interface/init.h"

#include "util/net.h"
#include "util/debug.h"

#include "pqi/pqinotify.h"

#include "interface/peers.h"

#include <sstream>

#include <interface/settings.h>

/****
 * #define NO_TCP_CONNECTIONS 1
 ***/
/****
 * #define NO_AUTO_CONNECTION 1
 ***/


/* Network setup States */
const uint32_t NET_UNKNOWN = 0x0001; /* Initial startup */
const uint32_t NET_UPNP_INIT = 0x0002; /* Initializing UPNP */
const uint32_t NET_UPNP_SETUP = 0x0003; /* After UPNP init, when we are checking to see if UPNP has been setup, and handle success or failure and continue to UDP_SETUP */
const uint32_t NET_UDP_SETUP = 0x0004; /* The state after UPNP_SETUP, where we set some class variables and move to DONE when the connection works. */
const uint32_t NET_DONE = 0x0005; /* After we have a valid external address, either via UDP or UPNP. */

/* Stun modes (TODO) */
const uint32_t STUN_DHT = 0x0001;
const uint32_t STUN_DONE = 0x0002;
const uint32_t STUN_LIST_MIN = 100;
const uint32_t STUN_FOUND_MIN = 10;

const int MAX_UPNP_INIT = 10; /* 10 second UPnP timeout */

const uint32_t TCP_DEFAULT_DELAY = 2; /* 2 Seconds? is it be enough! */
const uint32_t UDP_DHT_DELAY = DHT_NOTIFY_PERIOD + 60; /* + 1 minute for DHT POST */
const uint32_t UDP_PROXY_DELAY = 30;  /* 30 seconds (NOT IMPLEMENTED YET!) */

#define MIN_RETRY_PERIOD 600 //10 minute wait between automatic connection retries
#define USED_IP_WAIT_TIME 5 //5 second wait if tried to connect to an IP that was already in use in a connection attempt
#define DOUBLE_TRY_DELAY 10 //10 second wait for friend to have updated his friends list before second try
#define LAST_HEARD_TIMEOUT 300 //5 minute wait if haven't heard from a friend before we mark them as disconnected

peerConnectAddress::peerConnectAddress()
    :delay(0), period(0), type(0) {
    sockaddr_clear(&addr);
}

peerAddrInfo::peerAddrInfo()
    :found(false), type(0), ts(0) {
    sockaddr_clear(&laddr);
    sockaddr_clear(&raddr);
}

peerConnectState::peerConnectState()
    :name(""),
     id(""),
     netMode(NET_MODE_UNKNOWN),
     lastcontact(0),
     connecttype(0),
     lastattempt(0),
     doubleTried(false),
     schedulednexttry(0),
     state(0), actions(0),
     source(0),
     inConnAttempt(0) {
    sockaddr_clear(&localaddr);
    sockaddr_clear(&serveraddr);
}

ConnectivityManager::ConnectivityManager()
    :mNetStatus(NET_UNKNOWN),
     mStunStatus(0), mStunFound(0), mStunMoreRequired(true),
     mStatusChanged(false), mUpnpAddrValid(false),
     mStunAddrValid(false), mStunAddrStable(false) {

    /* Setup basics of own state.
       authMgr must have been initialized before connMgr. */
    ownState.id = authMgr->OwnCertId();
    ownState.librarymixer_id = authMgr->OwnLibraryMixerId();
    ownState.netMode = NET_MODE_UDP;

    mUpnpMgr = new upnphandler();

    mDhtMgr = new p3DhtMgr(authMgr->OwnCertId(), this);
    mDhtMgr->start();

    return;
}

void ConnectivityManager::checkNetAddress() {
    std::list<std::string> netInterfaceAddresses = getLocalInterfaces();
    std::list<std::string>::iterator it;

    QMutexLocker stack(&connMtx);

    /* Set the network interface we will use for the Mixologist. */
    ownState.localaddr.sin_addr = getPreferredInterface();
    ownState.localaddr.sin_port = htons(getLocalPort());
    ownState.localaddr.sin_family = AF_INET;
    ownState.serveraddr.sin_family = AF_INET;

    if ((isPrivateNet(&(ownState.localaddr.sin_addr))) ||
        (isLoopbackNet(&(ownState.localaddr.sin_addr)))) {
        //TODO We already know that we are firewalled at this point
        //Should we be setting this for loopback? Hardly a clear sign.
    }
}

void ConnectivityManager::netStartup() {
    /* startup stuff */

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
#ifdef false
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
#endif
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

    mNetInitTS = time(NULL);

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
            mNetStatus = NET_UPNP_INIT;
            TCP_over_UDP_set_stunkeepalive(true); //attempts to active tcp over udp tunnel stunning
            break;
    }
}


bool ConnectivityManager::shutdown() { /* blocking shutdown call */
    bool upnpActive;
    {
        QMutexLocker stack(&connMtx);
        upnpActive = ownState.netMode & NET_MODE_UPNP;
    }

    if (upnpActive) mUpnpMgr->shutdown();

    return true;
}

void ConnectivityManager::tick() {
    netTick();
    statusTick();
    monitorsTick();
}

void ConnectivityManager::netTick() {

    /* Check whether we are stuck on loopback. This happens if starts when
       the computer is not yet connected to the internet. In such a case we
       periodically check for a local net address. */
    if (isLoopbackNet(&(ownState.localaddr.sin_addr))) checkNetAddress();

    uint32_t netStatus;
    {
        QMutexLocker stack(&connMtx);
        netStatus = mNetStatus;
    }

    switch (netStatus) {
        case NET_UNKNOWN:
            netStartup();
            break;
        case NET_UPNP_INIT:
            netUpnpInit();
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

    return;
}

void ConnectivityManager::statusTick() {
    QList<unsigned int> retryIds;

    time_t now = time(NULL);
    time_t retryIfOlder = now - MIN_RETRY_PERIOD;
    time_t timeoutIfOlder = now - LAST_HEARD_TIMEOUT;

    {
        QMutexLocker stack(&connMtx);
        /* Can't simply use mFriendList.values() in foreach because that generates copies of the peerConnectState,
           and we need to be able to save in the changes. */
        foreach (unsigned int librarymixer_id, mFriendList.keys()) {
            if (mFriendList[librarymixer_id].state & PEER_S_NO_CERT) {
                continue;
            }
            /* Check if connected peers need to be timed out */
            else if (mFriendList[librarymixer_id].state & PEER_S_CONNECTED) {
                if (mFriendList[librarymixer_id].lastheard < timeoutIfOlder) {
                    log(LOG_WARNING, CONNECTIONMANAGERZONE, QString("Connection with ") + mFriendList[librarymixer_id].name + " has timed out");

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

void ConnectivityManager::netUpnpInit() {
    uint16_t eport, iport;

    {
        QMutexLocker stack(&connMtx);
        /* get the ports from the configuration */

        mNetStatus = NET_UPNP_SETUP;
        iport = ntohs(ownState.localaddr.sin_port);
        eport = ntohs(ownState.serveraddr.sin_port);
        if ((eport < 1000) || (eport > 30000)) {
            eport = iport;
        }
    }

    mUpnpMgr->setInternalPort(iport);
    mUpnpMgr->setExternalPort(eport);
    QSettings settings(*mainSettings, QSettings::IniFormat);
    mUpnpMgr->enable(settings.value("Network/UPNP", DEFAULT_UPNP).toBool());
}

void ConnectivityManager::netUpnpCheck() {
    /* grab timestamp */
    time_t timeSpent;
    {
        QMutexLocker stack(&connMtx);
        timeSpent = time(NULL) - mNetInitTS;
    }

    struct sockaddr_in extAddr;
    bool upnpActive = mUpnpMgr->getActive();

    if (!upnpActive && (timeSpent > MAX_UPNP_INIT)) {
        /* fallback to UDP startup */
        QMutexLocker stack(&connMtx);

        /* UPnP Failed us! */
        mUpnpAddrValid = false;
        mNetStatus = NET_UDP_SETUP;

        log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, "ConnectivityManager::netUpnpCheck() enabling stunkeepalive() due to UPNP failure");

        TCP_over_UDP_set_stunkeepalive(true);
    } else if (upnpActive && mUpnpMgr->getExternalAddress(extAddr)) {
        /* switch to UDP startup */
        QMutexLocker stack(&connMtx);

        /* Set Net Status flags ....
         * we now have external upnp address. Golden!
         * don't set netOk flag until have seen some traffic.
         */

        netFlagUpnpOk = true;
        netFlagExtOk = true;

        mUpnpAddrValid = true;
        mUpnpExtAddr = extAddr;
        mNetStatus = NET_UDP_SETUP;
        /* Fix netMode & Clear others! */
        ownState.netMode = NET_MODE_TRY_UPNP | NET_MODE_UPNP;

        log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, "ConnectivityManager::netUpnpCheck() disabling stunkeepalive() due to UPNP success");

        TCP_over_UDP_set_stunkeepalive(false);
        mStunMoreRequired = false; /* only need to validate address (UPNP) */
    }
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

                        notify->AddSysMessage(0, SYS_WARNING, title, msg);
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

void ConnectivityManager::netUpnpMaintenance() {
    /* We only need to maintain if we're using UPNP,
       and this variable is only set true after UPNP is successfully configured. */
    static time_t last_call;
    bool notTooFrequent = (time(NULL) - last_call > 300);
    if (mUpnpAddrValid && notTooFrequent) {
        last_call = time(NULL);
        if (!mUpnpMgr->getActive()) {
            mUpnpMgr->restart();
            /* Return netTick loop to waiting for connection to be setup.
               Reset the clock so it has another full time-out to fix the connection. */
            mNetStatus = NET_UPNP_SETUP;
            mNetInitTS = time(NULL);
        }
    }
}

/********************************  Network Status  *********************************
 * Configuration Loading / Saving.
 */


void ConnectivityManager::addMonitor(pqiMonitor *mon) {
    QMutexLocker stack(&connMtx);
    if (monitorListeners.contains(mon)) exit(-1);
    monitorListeners.push_back(mon);
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
            CONNECTIONMANAGERZONE,
            QString("Can't make attempt to connect to user, not in friends list, certificate id was: ").append(librarymixer_id));
        return false;
    }

    peerConnectState* connectState = &mFriendList[librarymixer_id];

    if (connectState->connAddrs.size() < 1) {
        log(LOG_WARNING,
            CONNECTIONMANAGERZONE,
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
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, "ConnectivityManager::connectAttempt Can not connect to " + addressToString(address.addr) + " due to existing connection.");
            } else if (ipit.value() == USED_IP_CONNECTING) {
                connectState->schedulednexttry = time(NULL) + USED_IP_WAIT_TIME;
                log(LOG_DEBUG_BASIC, CONNECTIONMANAGERZONE, "ConnectivityManager::connectAttempt Waiting to try to connect to " + addressToString(address.addr) + " due to existing attempted connection.");
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
            CONNECTIONMANAGERZONE,
            QString("ConnectivityManager::connectAttempt Returning information for connection attempt to user: ").append(connectState->name));

        return true;
    }
}

bool ConnectivityManager::reportConnectionUpdate(unsigned int librarymixer_id, bool success, uint32_t flags) {
    QMutexLocker stack(&connMtx);

    if (!mFriendList.contains(librarymixer_id)) {
        log(LOG_WARNING,
            CONNECTIONMANAGERZONE,
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
            CONNECTIONMANAGERZONE,
            QString("Successfully connected to: ") + connectState->name +
            " (" + inet_ntoa(connectState->currentConnAddr.addr.sin_addr) + ")");

        /* change state */
        connectState->state |= PEER_S_CONNECTED;
        connectState->actions |= PEER_CONNECTED;
        mStatusChanged = true;
        connectState->lastcontact = time(NULL);
        connectState->lastheard = time(NULL);
        connectState->connecttype = flags;

        return true;
    }

    log(LOG_DEBUG_BASIC,
        CONNECTIONMANAGERZONE,
        QString("Unable to connect to friend: ") + connectState->name +
        ", flags: " + QString::number(flags));

    usedIps.remove(addressToString(connectState->currentConnAddr.addr));

    /* if currently connected -> flag as failed */
    if (connectState->state & PEER_S_CONNECTED) {
        connectState->state &= (~PEER_S_CONNECTED);
        connectState->actions |= PEER_DISCONNECTED;

        connectState->lastcontact = time(NULL);  /* time of disconnect */

        mDhtMgr->findPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));
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
        log(LOG_ERROR, CONNECTIONMANAGERZONE, QString("Somehow heard from someone that is not a friend: ").append(librarymixer_id));
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
            CONNECTIONMANAGERZONE,
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

    if (!(ownState.netMode & NET_MODE_UNREACHABLE)) {
        log(LOG_DEBUG_ALERT,
            CONNECTIONMANAGERZONE,
            QString("ConnectivityManager::retryConnectUDP()") +
            " Notifying friend " + QString::number(librarymixer_id));

        /* attempt UDP connection */
        mDhtMgr->notifyPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));
    }

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

bool ConnectivityManager::addUpdateFriend(unsigned int librarymixer_id, QString cert, QString name) {
    QMutexLocker stack(&connMtx);

    //First try to insert the certificate
    int authResult = authMgr->addUpdateCertificate(cert, librarymixer_id);

    //Existing friend
    if (mFriendList.contains(librarymixer_id)) {
        //Update the name
        mFriendList[librarymixer_id].name = name;
        mFriendList[librarymixer_id].id = authMgr->findCertByLibraryMixerId(librarymixer_id);
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
    } else { //New friend
        /* create a new entry */
        peerConnectState pstate;

        pstate.librarymixer_id = librarymixer_id;
        pstate.name = name;

        //If this is a new friend, but no cert was added
        if (authResult <= 0) {
            pstate.id = "";
            pstate.state = PEER_S_NO_CERT;
            pstate.actions = 0;
        } else { //If cert was added
            //Should not be able to reach here with a null Cert
            if ((pstate.id = authMgr->findCertByLibraryMixerId(librarymixer_id)).empty()) return false;
            pstate.state = 0;
            //Only have pqiperson built if we actually got a key
            pstate.actions = PEER_NEW;
            mStatusChanged = true;
        }
        pstate.netMode = NET_MODE_UDP;
        pstate.lastcontact = 0;

        /* addr & timestamps -> auto cleared */

        mFriendList[librarymixer_id] = pstate;

        /* expect it to be a standard DHT */
        //mDhtMgr->findPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));

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

bool ConnectivityManager::setLocalAddress(unsigned int librarymixer_id, struct sockaddr_in addr) {
    QMutexLocker stack(&connMtx);

    if (librarymixer_id == ownState.librarymixer_id) {
        ownState.localaddr = addr;
        return true;
    }

    /* check if it is a friend */
    QMap<unsigned int, peerConnectState>::iterator it;
    if (mFriendList.end() == (it = mFriendList.find(librarymixer_id))) return false;

    /* "it" points to peer */
    it.value().localaddr = addr;

    return true;
}

bool ConnectivityManager::setExtAddress(unsigned int librarymixer_id, struct sockaddr_in addr) {
    QMutexLocker stack(&connMtx);


    if (librarymixer_id == ownState.librarymixer_id) {
        ownState.serveraddr = addr;
        return true;
    }

    /* check if it is a friend */
    QMap<unsigned int, peerConnectState>::iterator it;
    if (mFriendList.end() == (it = mFriendList.find(librarymixer_id))) return false;

    /* "it" points to peer */
    it.value().serveraddr = addr;

    return true;
}

bool ConnectivityManager::setNetworkMode(unsigned int librarymixer_id, uint32_t netMode) {
    if (librarymixer_id == authMgr->OwnLibraryMixerId()) {
        /* only change TRY flags */
        ownState.netMode &= ~(NET_MODE_TRYMODE);

        switch (netMode & NET_MODE_ACTUAL) {
            case NET_MODE_EXT:
                ownState.netMode |= NET_MODE_TRY_EXT;
                break;
            case NET_MODE_UPNP:
                ownState.netMode |= NET_MODE_TRY_UPNP;
                break;
            default:
            case NET_MODE_UDP:
                ownState.netMode |= NET_MODE_TRY_UDP;
                break;
        }

        /* if we've started up - then tweak Dht On/Off */
        if (mNetStatus != NET_UNKNOWN) {
            mDhtMgr->enable(false);
        }

        return true;
    }

    QMutexLocker stack(&connMtx);

    /* check if it is a friend */
    QMap<unsigned int, peerConnectState>::iterator it;
    if (mFriendList.end() == (it = mFriendList.find(librarymixer_id))) return false;

    /* "it" points to peer */
    it.value().netMode = netMode;

    return false;
}

/******* overloaded from pqiConnectCb *************/
//3 functions are related to DHT
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
            log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
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
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
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
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
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
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
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

void ConnectivityManager::peerConnectRequest(std::string id, struct sockaddr_in raddr, uint32_t source) {
    {
        /* Log */
        std::ostringstream out;
        out << "ConnectivityManager::peerConnectRequest()";
        out << " id: " << id;
        out << " raddr: " << inet_ntoa(raddr.sin_addr);
        out << ":" << ntohs(raddr.sin_port);
        out << " source: " << source;
        log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
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
            log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
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

/*******************************  UDP MAINTAINANCE  ********************************
 * Interaction with the UDP is mainly for determining the External Port.
 *
 */

bool ConnectivityManager::udpInternalAddress(struct sockaddr_in) {
    return false;
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

void ConnectivityManager::udpStunPeer(std::string id, struct sockaddr_in &addr) {
    /* add it into udp stun list */
    TCP_over_UDP_add_stunpeer((struct sockaddr *) &addr, sizeof(addr), id.c_str());
}

bool ConnectivityManager::stunCheck() {
    /* check if we've got a Stun result */
    bool stunOk = false;

    {
        QMutexLocker stack(&connMtx);

        /* if DONE -> return */
        if (mStunStatus == STUN_DONE) {
            return true;
        }

        if (mStunFound >= STUN_FOUND_MIN) {
            mStunMoreRequired = false;
        }
        stunOk = (!mStunMoreRequired);
    }


    if (udpExtAddressCheck() && (stunOk)) {
        /* set external UDP address */
        mDhtMgr->enableStun(false);

        QMutexLocker stack(&connMtx);

        mStunStatus = STUN_DONE;

        return true;
    }
    return false;
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

/* FLAGS

ONLINE
EXT
UPNP
UDP
FRIEND
FRIEND_OF_FRIEND
OTHER

*/

void ConnectivityManager::stunCollect(std::string id, struct sockaddr_in, uint32_t flags) {
    QMutexLocker stack(&connMtx);

    std::list<std::string>::iterator it;
    it = std::find(mStunList.begin(), mStunList.end(), id);
    if (it == mStunList.end()) {
        /* add it in:
         * if FRIEND / ONLINE or if list is short.
         */
        if ((flags & STUN_ONLINE) || (flags & STUN_FRIEND)) {
            /* push to the front */
            mStunList.push_front(id);

        } else if (mStunList.size() < STUN_LIST_MIN) {
            /* push to the front */
            mStunList.push_back(id);
        }
    } else {
        /* if they're online ... move to the front
         */
        if (flags & STUN_ONLINE) {
            /* move to front */
            mStunList.erase(it);
            mStunList.push_front(id);
        }
    }

}


/**********************************************************************************
 * Utility methods
 **********************************************************************************/

#define MIN_RAND_PORT 10000
#define MAX_RAND_PORT 30000

int ConnectivityManager::getLocalPort() {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    if (settings.contains("Network/PortNumber")) {
        int storedPort = settings.value("Network/PortNumber", DEFAULT_PORT).toInt();
        if (storedPort != SET_TO_RANDOMIZED_PORT &&
            storedPort > Peers::MIN_PORT &&
            storedPort < Peers::MAX_PORT) return storedPort;
        srand(time(NULL));
        return rand() % (MAX_RAND_PORT - MIN_RAND_PORT + 1) + MIN_RAND_PORT;
    } else {
        srand(time(NULL));
        int randomPort = rand() % (MAX_RAND_PORT - MIN_RAND_PORT + 1) + MIN_RAND_PORT;
        settings.setValue("Network/PortNumber", randomPort);
        return randomPort;
    }
}

QString ConnectivityManager::addressToString(struct sockaddr_in address) {
    return QString(inet_ntoa(address.sin_addr)) + ":" + QString::number(ntohs(address.sin_port));
}
