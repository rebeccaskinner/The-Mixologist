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

#include "pqi/pqi_base.h"
#include "pqi/p3connmgr.h"
#include "pqi/p3dhtmgr.h" // Only need it for constants.
#include "tcponudp/tou.h"

#include "util/net.h"
#include "util/print.h"
#include "util/debug.h"

#include "pqi/pqinotify.h"

#include "interface/peers.h"

#include <sstream>
#include <QSettings>

#include <interface/settings.h>

/****
 * #define CONN_DEBUG 1
 ***/
/****
 * #define P3CONNMGR_NO_TCP_CONNECTIONS 1
 ***/
/****
 * #define P3CONNMGR_NO_AUTO_CONNECTION 1
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

const uint32_t P3CONNMGR_TCP_DEFAULT_DELAY = 2; /* 2 Seconds? is it be enough! */
const uint32_t P3CONNMGR_UDP_DHT_DELAY = DHT_NOTIFY_PERIOD + 60; /* + 1 minute for DHT POST */
const uint32_t P3CONNMGR_UDP_PROXY_DELAY = 30;  /* 30 seconds (NOT IMPLEMENTED YET!) */

#define MIN_RETRY_PERIOD 600 //10 minute wait between connection retries
#define USED_IP_WAIT_TIME 5 //5 second wait if tried to connect to an IP that was used
#define DOUBLE_TRY_DELAY 10 //10 second wait for friend to have updated his friends list before second try
#define LAST_HEARD_TIMEOUT 300 //5 minute wait if haven't heard from friend

peerConnectAddress::peerConnectAddress()
    :delay(0), period(0), type(0), ts(0) {
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
     netMode(NET_MODE_UNKNOWN), visState(VIS_STATE_STD),
     lastcontact(0),
     connecttype(0),
     lastattempt(0),
     schedulednexttry(0),
     doubleTried(false),
     state(0), actions(0),
     source(0),
     inConnAttempt(0) {
    sockaddr_clear(&localaddr);
    sockaddr_clear(&serveraddr);
}


p3ConnectMgr::p3ConnectMgr(QString name)
    :mNetStatus(NET_UNKNOWN),
     mStunStatus(0), mStunFound(0), mStunMoreRequired(true),
     mStatusChanged(false), mUpnpAddrValid(false),
     mStunAddrValid(false), mStunAddrStable(false) {

    /* setup basics of own state */
    if (authMgr) {
        ownState.id = authMgr->OwnCertId();
        ownState.librarymixer_id = authMgr->OwnLibraryMixerId();
        ownState.name = name;
        ownState.netMode = NET_MODE_UDP;
    }

    return;
}

void p3ConnectMgr::setOwnNetConfig(uint32_t netMode, uint32_t visState) {
    /* only change TRY flags */

#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::setOwnNetConfig()" << std::endl;
    std::cerr << "Existing netMode: " << ownState.netMode << " vis: " << ownState.visState;
    std::cerr << std::endl;
    std::cerr << "Input netMode: " << netMode << " vis: " << visState;
    std::cerr << std::endl;
#endif
    ownState.netMode &= ~(NET_MODE_TRYMODE);

#ifdef CONN_DEBUG
    std::cerr << "After Clear netMode: " << ownState.netMode << " vis: " << ownState.visState;
    std::cerr << std::endl;
#endif

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

    ownState.visState = visState;

#ifdef CONN_DEBUG
    std::cerr << "Final netMode: " << ownState.netMode << " vis: " << ownState.visState;
    std::cerr << std::endl;
#endif

    /* if we've started up - then tweak Dht On/Off */
    if (mNetStatus != NET_UNKNOWN) {
        //mDhtMgr->enable(!(ownState.visState & VIS_STATE_NODHT));
        mDhtMgr->enable(false);
    }
}

void p3ConnectMgr::netStartup() {
    /* startup stuff */

    /* StunInit gets a list of peers, and asks the DHT to find them...
     * This is needed for all systems so startup straight away
     */
#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::netStartup()" << std::endl;
#endif
    //dht init
    connMtx.lock();
    uint32_t vs = ownState.visState;
    connMtx.unlock();
    mDhtMgr->enable(!(vs & VIS_STATE_NODHT)); //enable dht unless vis set to no dht

    //udp init
    connMtx.lock();
    struct sockaddr_in iaddr = ownState.localaddr;
    connMtx.unlock();
    /* open our udp port */
    tou_init((struct sockaddr *) &iaddr, sizeof(iaddr));

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
    connMtx.lock();
    mDhtMgr->enableStun(true);
    /* push stun list to DHT */
    std::list<std::string>::iterator it;
    for (it = mStunList.begin(); it != mStunList.end(); it++) {
        mDhtMgr->addStun(*it);
    }
    mStunStatus = STUN_DHT;
    mStunFound = 0;
    mStunMoreRequired = true;
    connMtx.unlock();
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

#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::netStartup() tou_stunkeepalive() enabled" << std::endl;
#endif
    //tou_stunkeepalive(1); move into switch statement seems more logical right?

    ownState.netMode &= ~(NET_MODE_ACTUAL); //mask the actual state bits, we want to see the netMode try bits

    switch (ownState.netMode & NET_MODE_TRYMODE) {

        case NET_MODE_TRY_EXT:  /* v similar to UDP */
            ownState.netMode |= NET_MODE_EXT;
            mNetStatus = NET_UDP_SETUP;
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::netStartup() disabling stunkeepalive() cos EXT" << std::endl;
#endif
            tou_stunkeepalive(0);
            mStunMoreRequired = false; /* only need to validate address (EXT) */

            break;

        case NET_MODE_TRY_UDP:
            ownState.netMode |= NET_MODE_UDP;
            mNetStatus = NET_UDP_SETUP;
            tou_stunkeepalive(1); //attempts to active tcp over udp tunnel stunning
            break;

        case NET_MODE_TRY_UPNP:
        default:
            /* Force it here (could be default!) */
            //ownState.netMode |= NET_MODE_TRY_UPNP; logically already true right?
            ownState.netMode |= NET_MODE_UDP;      /* set to UDP, upgraded is UPnP is Okay */
            mNetStatus = NET_UPNP_INIT;
            tou_stunkeepalive(1); //attempts to active tcp over udp tunnel stunning
            break;
    }
}


bool p3ConnectMgr::shutdown() { /* blocking shutdown call */
    connMtx.lock();

    bool upnpActive = ownState.netMode & NET_MODE_UPNP;

    connMtx.unlock();

    if (upnpActive) mUpnpMgr->shutdown();
    mDhtMgr->shutdown();

    return true;
}

void p3ConnectMgr::tick() {
    netTick();
    statusTick();
    tickMonitors();
}

void p3ConnectMgr::netTick() {

    // Check whether we are stuck on loopback. This happens if starts when
    // the computer is not yet connected to the internet. In such a case we
    // periodically check for a local net address.
    //
    if (isLoopbackNet(&(ownState.localaddr.sin_addr)))
        checkNetAddress() ;

    connMtx.lock();

    uint32_t netStatus = mNetStatus;

    connMtx.unlock();

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

void    p3ConnectMgr::statusTick() {
    std::list<int> retryIds;
    std::list<int>::iterator retryIdsIt;

    time_t now = time(NULL);
    time_t retryIfOlder = now - MIN_RETRY_PERIOD;
    time_t timeoutIfOlder = now - LAST_HEARD_TIMEOUT;

    {
        QMutexLocker stack(&connMtx);  /******   LOCK MUTEX ******/
        std::map<int, peerConnectState>::iterator it;
        for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
            /* Do nothing for no cert peers */
            if (it->second.state & PEER_S_NO_CERT) {
                continue;
            }
            /* Check if connected peers need to be timed out */
            else if (it->second.state & PEER_S_CONNECTED) {
                if (it->second.lastheard < timeoutIfOlder) {
                    QString out("Connection with ");
                    out.append(it->second.name);
                    out.append(" has timed out");
                    log(LOG_WARNING, CONNECTIONMANAGERZONE, out);

                    it->second.state &= (~PEER_S_CONNECTED);
                    it->second.actions |= PEER_TIMEOUT;
                    it->second.lastcontact = time(NULL);  /* time of disconnect */

                    it->second.doubleTried = false;
                    it->second.schedulednexttry = 0;
                    retryIds.push_back(it->first);
                } else continue;
            }
            /* If last attempt was long enough ago, start a whole new attempt.
               If we have a scheduled retry then start it.
               The timer based retry represents a whole new connection attempt, so doubleTried is reset.
               doubleTried is not reset under scheduled because it represents a continuation of an
               existing attempt, and indeed, may actually be the double retry itself.*/
            else if (it->second.lastattempt < retryIfOlder) {
                it->second.doubleTried = false;
                it->second.schedulednexttry = 0;
                retryIds.push_back(it->first);
            } else if (it->second.schedulednexttry != 0 &&
                       it->second.schedulednexttry < now) {
                it->second.schedulednexttry = 0;
                retryIds.push_back(it->first);
            }
        }
    }

#ifndef P3CONNMGR_NO_AUTO_CONNECTION

    for (retryIdsIt = retryIds.begin(); retryIdsIt != retryIds.end(); retryIdsIt++) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::statusTick() RETRY TIMEOUT for: ";
        std::cerr << *it2;
        std::cerr << std::endl;
#endif
        /* retry it! */
        retryConnectTCP(*retryIdsIt);
    }

#endif

}

void p3ConnectMgr::tickMonitors() {
    bool doStatusChange = false;
    std::list<pqipeer> actionList;
    std::map<int, peerConnectState>::iterator it;

    {
        QMutexLocker stack(&connMtx);

        if (mStatusChanged) {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::tickMonitors() StatusChanged! List:" << std::endl;
#endif
            /* assemble list */
            for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
                if (it->second.actions) {
                    /* add in */
                    pqipeer peer;
                    peer.librarymixer_id = it->second.librarymixer_id;
                    peer.cert_id = it->second.id;
                    peer.name = it->second.name;
                    peer.state = it->second.state;
                    peer.actions = it->second.actions;

                    /* reset action */
                    it->second.actions = 0;

                    actionList.push_back(peer);

#ifdef CONN_DEBUG
                    std::cerr << "Friend: " << peer.name.toStdString();
                    std::cerr << " Id: " << peer.id;
                    std::cerr << " State: " << peer.state;
                    if (peer.state & PEER_S_CONNECTED)
                        std::cerr << " S:PEER_S_CONNECTED";
                    std::cerr << " Actions: " << peer.actions;
                    if (peer.actions & PEER_NEW)
                        std::cerr << " A:PEER_NEW";
                    if (peer.actions & PEER_CONNECTED)
                        std::cerr << " A:PEER_CONNECTED";
                    if (peer.actions & PEER_DISCONNECTED)
                        std::cerr << " A:PEER_DISCONNECTED";
                    if (peer.actions & PEER_CONNECT_REQ)
                        std::cerr << " A:PEER_CONNECT_REQ";

                    std::cerr << std::endl;
#endif

                    /* notify GUI */
                    if (peer.actions & PEER_CONNECTED) {
                        pqiNotify *notify = getPqiNotify();
                        if (notify) {
                            notify->AddPopupMessage(POPUP_CONNECT,
                                                    QString::number(peer.librarymixer_id), "Online: ");
                        }
                    }
                }
            }
            mStatusChanged = false;
            doStatusChange = true;

        }
    } /****** UNLOCK STACK MUTEX ******/

    /* NOTE - clients is accessed without mutex protection!!!!
     * At the moment this is okay - as they are only added at the start.
     * IF this changes ---- must fix with second Mutex.
     */

    if (doStatusChange) {
#ifdef CONN_DEBUG
        std::cerr << "Sending to " << clients.size() << " monitorClients";
        std::cerr << std::endl;
#endif

        /* send to all monitors */
        std::list<pqiMonitor *>::iterator mit;
        for (mit = clients.begin(); mit != clients.end(); mit++) {
            (*mit)->statusChange(actionList);
        }
    }
}

void p3ConnectMgr::netUpnpInit() {
#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::netUpnpInit()" << std::endl;
#endif
    uint16_t eport, iport;

    connMtx.lock();

    /* get the ports from the configuration */

    mNetStatus = NET_UPNP_SETUP;
    iport = ntohs(ownState.localaddr.sin_port);
    eport = ntohs(ownState.serveraddr.sin_port);
    if ((eport < 1000) || (eport > 30000)) {
        eport = iport;
    }

    connMtx.unlock();

    mUpnpMgr->setInternalPort(iport);
    mUpnpMgr->setExternalPort(eport);
    QSettings settings(*mainSettings, QSettings::IniFormat);
    mUpnpMgr->enable(settings.value("Network/UPNP", DEFAULT_UPNP).toBool());
}

void p3ConnectMgr::netUpnpCheck() {
    /* grab timestamp */
    connMtx.lock();

    time_t timeSpent = time(NULL) - mNetInitTS;

    connMtx.unlock();

    struct sockaddr_in extAddr;
    bool upnpActive = mUpnpMgr->getActive();

    if (!upnpActive && (timeSpent > MAX_UPNP_INIT)) {
        /* fallback to UDP startup */
        connMtx.lock();

        /* UPnP Failed us! */
        mUpnpAddrValid = false;
        mNetStatus = NET_UDP_SETUP;

        log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, "p3ConnectMgr::netUpnpCheck() enabling stunkeepalive() due to UPNP failure");

        tou_stunkeepalive(1);

        connMtx.unlock();
    } else if (upnpActive && mUpnpMgr->getExternalAddress(extAddr)) {
        /* switch to UDP startup */
        connMtx.lock();

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

        log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, "p3ConnectMgr::netUpnpCheck() disabling stunkeepalive() due to UPNP success");

        tou_stunkeepalive(0);
        mStunMoreRequired = false; /* only need to validate address (UPNP) */

        connMtx.unlock();
    }
}

void p3ConnectMgr::netUdpCheck() {
    if (udpExtAddressCheck() || (mUpnpAddrValid)) {
        bool extValid = false;
        bool extAddrStable = false;
        struct sockaddr_in extAddr;
        uint32_t mode = 0;

        connMtx.lock();

        mNetStatus = NET_DONE;

        /* get the addr from the configuration */
        struct sockaddr_in iaddr = ownState.localaddr;

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
#ifdef CONN_DEBUG
                std::cerr << "p3ConnectMgr::netUdpCheck() UDP Unstable :( ";
                std::cerr <<  std::endl;
                std::cerr << "p3ConnectMgr::netUdpCheck() We are unreachable";
                std::cerr <<  std::endl;
                std::cerr << "netMode =>  NET_MODE_UNREACHABLE";
                std::cerr <<  std::endl;
#endif
                ownState.netMode &= ~(NET_MODE_ACTUAL);
                ownState.netMode |= NET_MODE_UNREACHABLE;
                tou_stunkeepalive(0);
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

        connMtx.unlock();

        mDhtMgr->setExternalInterface(iaddr, extAddr, mode);

        /* flag unreachables! */
        if ((extValid) && (!extAddrStable)) {
            netUnreachableCheck();
        }
    }
}

void p3ConnectMgr::netUnreachableCheck() {
#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::netUnreachableCheck()" << std::endl;
#endif
    std::map<int, peerConnectState>::iterator it;

    QMutexLocker stack(&connMtx);

    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        /* get last contact detail */
        if (it->second.state & PEER_S_CONNECTED) {
#ifdef CONN_DEBUG
            std::cerr << "NUC() Ignoring Connected Peer" << std::endl;
#endif
            continue;
        }

        peerAddrInfo details;
        switch (it->second.source) {
            case CB_DHT:
                details = it->second.dht;
#ifdef CONN_DEBUG
                std::cerr << "NUC() Using DHT data" << std::endl;
#endif
                break;
            case CB_PERSON:
                details = it->second.peer;
#ifdef CONN_DEBUG
                std::cerr << "NUC() Using PEER data" << std::endl;
#endif
                break;
            default:
                continue;
                break;
        }

        std::cerr << "NUC() Peer: " << it->first << std::endl;

        /* Determine Reachability (only advisory) */
        // if (ownState.netMode == NET_MODE_UNREACHABLE) // MUST BE TRUE!
        {
            if (details.type & NET_CONN_TCP_EXTERNAL) {
                /* reachable! */
                it->second.state &= (~PEER_S_UNREACHABLE);
#ifdef CONN_DEBUG
                std::cerr << "NUC() Peer EXT TCP - reachable" << std::endl;
#endif
            } else {
                /* unreachable */
                it->second.state |= PEER_S_UNREACHABLE;
#ifdef CONN_DEBUG
                std::cerr << "NUC() Peer !EXT TCP - unreachable" << std::endl;
#endif
            }
        }
    }

}

void p3ConnectMgr::netUpnpMaintenance() {
    /* We only need to maintain if we're using UPNP,
       and this variable is only set true after UPNP is successfully configured. */
    static time_t last_call;
    bool notTooFrequent = (time(NULL) - last_call > 60);
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

/*******************************  UDP MAINTAINANCE  ********************************
 * Interaction with the UDP is mainly for determining the External Port.
 *
 */

bool p3ConnectMgr::udpInternalAddress(struct sockaddr_in) {
    return false;
}

bool p3ConnectMgr::udpExtAddressCheck() {
    /* three possibilities:
     * (1) not found yet.
     * (2) Found!
     * (3) bad udp (port switching).
     */
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    uint8_t stable;

    if (0 < tou_extaddr((struct sockaddr *) &addr, &len, &stable)) {
        QMutexLocker stack(&connMtx);


        /* update UDP information */
        mStunExtAddr = addr;
        mStunAddrValid = true;
        mStunAddrStable = (stable != 0);

#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::udpExtAddressCheck() Got ";
        std::cerr << " addr: " << inet_ntoa(mStunExtAddr.sin_addr);
        std::cerr << ":" << ntohs(mStunExtAddr.sin_port);
        std::cerr << " stable: " << mStunAddrStable;
        std::cerr << std::endl;
#endif

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

void p3ConnectMgr::udpStunPeer(std::string id, struct sockaddr_in &addr) {
#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::udpStunPeer()" << std::endl;
#endif
    /* add it into udp stun list */
    tou_stunpeer((struct sockaddr *) &addr, sizeof(addr), id.c_str());
}

bool p3ConnectMgr::stunCheck() {
    /* check if we've got a Stun result */
    bool stunOk = false;

#ifdef CONN_DEBUG
    //std::cerr << "p3ConnectMgr::stunCheck()" << std::endl;
#endif

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

void    p3ConnectMgr::stunStatus(std::string id, struct sockaddr_in raddr, uint32_t type, uint32_t flags) {
#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::stunStatus()";
    std::cerr << " id: " << Util::BinToHex(id) << " raddr: " << inet_ntoa(raddr.sin_addr);
    std::cerr << ":" << ntohs(raddr.sin_port);
    std::cerr << std::endl;
#endif

    connMtx.lock();

    bool stillStunning = (mStunStatus == STUN_DHT);

    connMtx.unlock();

    /* only useful if they have an exposed TCP/UDP port */
    if (type & NET_CONN_TCP_EXTERNAL) {
        if (stillStunning) {
            connMtx.lock();
            mStunFound++;
            connMtx.unlock();

#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::stunStatus() Sending to UDP" << std::endl;
#endif
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

void p3ConnectMgr::stunCollect(std::string id, struct sockaddr_in, uint32_t flags) {
    QMutexLocker stack(&connMtx);

#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::stunCollect() id: " << Util::BinToHex(id) << std::endl;
#endif

    std::list<std::string>::iterator it;
    it = std::find(mStunList.begin(), mStunList.end(), id);
    if (it == mStunList.end()) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::stunCollect() Id not in List" << std::endl;
#endif
        /* add it in:
         * if FRIEND / ONLINE or if list is short.
         */
        if ((flags & STUN_ONLINE) || (flags & STUN_FRIEND)) {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::stunCollect() Id added to Front" << std::endl;
#endif
            /* push to the front */
            mStunList.push_front(id);

        } else if (mStunList.size() < STUN_LIST_MIN) {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::stunCollect() Id added to Back" << std::endl;
#endif
            /* push to the front */
            mStunList.push_back(id);
        }
    } else {
        /* if they're online ... move to the front
         */
        if (flags & STUN_ONLINE) {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::stunCollect() Id moved to Front" << std::endl;
#endif
            /* move to front */
            mStunList.erase(it);
            mStunList.push_front(id);
        }
    }

}

/********************************  Network Status  *********************************
 * Configuration Loading / Saving.
 */


void p3ConnectMgr::addMonitor(pqiMonitor *mon) {
    QMutexLocker stack(&connMtx);

    std::list<pqiMonitor *>::iterator it;
    it = std::find(clients.begin(), clients.end(), mon);
    //Should not happen that a monitor is added twice
    if (it != clients.end()) exit(-1);

    clients.push_back(mon);
    return;
}

const std::string p3ConnectMgr::getOwnCertId() {
    if (authMgr) {
        return authMgr->OwnCertId();
    } else {
        std::string nullStr;
        return nullStr;
    }
}

bool p3ConnectMgr::isFriend(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);
    return (mFriendList.end() != mFriendList.find(librarymixer_id));
}

bool p3ConnectMgr::isOnline(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    std::map<int, peerConnectState>::iterator it;
    if (mFriendList.end() != (it = mFriendList.find(librarymixer_id))) {
        return (it->second.state & PEER_S_CONNECTED);
    } else {
        /* not a friend */
        return false;
    }
}

QString p3ConnectMgr::getFriendName(unsigned int librarymixer_id) {
    std::map<int, peerConnectState>::iterator it;
    it = mFriendList.find(librarymixer_id);
    if (it == mFriendList.end()) return "";
    else return it->second.name;
}

QString p3ConnectMgr::getFriendName(std::string cert_id) {
    std::map<int, peerConnectState>::iterator it;
    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        if (it->second.id == cert_id) return it->second.name;
    }
    return "";
}

bool p3ConnectMgr::getPeerConnectState(unsigned int librarymixer_id, peerConnectState &state) {
    QMutexLocker stack(&connMtx);

    if (librarymixer_id == ownState.librarymixer_id) {
        state = ownState;
    } else {
        /* check for existing */
        std::map<int, peerConnectState>::iterator it;
        it = mFriendList.find(librarymixer_id);
        if (it == mFriendList.end()) {
            return false;
        }

        state = it->second;
    }
    return true;
}

QString p3ConnectMgr::getOwnName(){
    return ownState.name;
}

void p3ConnectMgr::getOnlineList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    /* check for existing */
    std::map<int, peerConnectState>::iterator it;
    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        if (it->second.state & PEER_S_CONNECTED) {
            peers.push_back(it->first);
        }
    }
    return;
}

void p3ConnectMgr::getSignedUpList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    /* check for existing */
    std::map<int, peerConnectState>::iterator it;
    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        if (it->second.state != PEER_S_NO_CERT) {
            peers.push_back(it->first);
        }
    }
    return;
}

void p3ConnectMgr::getFriendList(std::list<int> &peers) {
    QMutexLocker stack(&connMtx);

    /* check for existing */
    std::map<int, peerConnectState>::iterator it;
    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        peers.push_back(it->first);
    }
    return;
}

bool p3ConnectMgr::connectAttempt(unsigned int librarymixer_id, struct sockaddr_in &addr,
                                  uint32_t &delay, uint32_t &period, uint32_t &type)

{
    QMutexLocker stack(&connMtx);

    /* check for existing */
    std::map<int, peerConnectState>::iterator it;
    it = mFriendList.find(librarymixer_id);
    if (it == mFriendList.end()) {
        log(LOG_WARNING, CONNECTIONMANAGERZONE, QString("Can't make attempt to connect to user, not in friends list, certificate id was: ").append(librarymixer_id));
        return false;
    }

    if (it->second.connAddrs.size() < 1) {
        log(LOG_WARNING, CONNECTIONMANAGERZONE, QString("Can't make attempt to connect to user, have no IP address: ").append(it->second.name));
        return false;
    }

    it->second.lastattempt = time(NULL);

    //Test if address is in use already
    peerConnectAddress address = it->second.connAddrs.front();
    it->second.connAddrs.pop_front();
    std::map<std::string, int>::iterator ipit;
    ipit = usedIps.find(address_to_string(address.addr));
    //if that IP is already used,
    //if there are other addresses put this one on the back and try again
    //if there are no other addresses,
    //if it's already used in a successful connection, stop trying with this address and return false
    //if it's only being used for an attempted connection, put it on the back and set a special retry timer to try again soon
    if (ipit != usedIps.end()) {
        if (it->second.connAddrs.size() >= 1) {
            it->second.connAddrs.push_back(address);
            it->second.actions |= PEER_CONNECT_REQ;
            return false;
        } else {
            if (ipit->second == USED_IP_CONNECTED) {
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, "p3ConnectMgr::connectAttempt Can not connect to " + QString(address_to_string(address.addr).c_str()) + " due to existing connection.");
                return false;
            } else if (ipit->second == USED_IP_CONNECTING) {
                it->second.schedulednexttry = time(NULL) + USED_IP_WAIT_TIME;
                log(LOG_DEBUG_BASIC, CONNECTIONMANAGERZONE, "p3ConnectMgr::connectAttempt Waiting to try to connect to " + QString(address_to_string(address.addr).c_str()) + " due to existing attempted connection.");
                return false;
            }
        }
    }
    it->second.inConnAttempt = true;
    it->second.currentConnAddr = address;
    usedIps[address_to_string(address.addr)] = USED_IP_CONNECTING;

    addr = it->second.currentConnAddr.addr;
    delay = it->second.currentConnAddr.delay;
    period = it->second.currentConnAddr.period;
    type = it->second.currentConnAddr.type;

    log(LOG_DEBUG_BASIC, CONNECTIONMANAGERZONE, QString("p3ConnectMgr::connectAttempt Returning information for connection attempt to user: ").append(it->second.name));

    return true;
}


/****************************
 * Update state,
 * trigger retry if necessary,
 *
 * remove from DHT?
 *
 */

bool p3ConnectMgr::connectResult(unsigned int librarymixer_id, bool success, uint32_t flags) {
    QMutexLocker stack(&connMtx);

    /* check for existing */
    std::map<int, peerConnectState>::iterator it;
    it = mFriendList.find(librarymixer_id);
    if (it == mFriendList.end()) {
        log(LOG_WARNING, CONNECTIONMANAGERZONE, QString("Failed to connect to user, not in friends list, friend number was: ").append(QString::number(librarymixer_id)));
        return false;
    }

    it->second.inConnAttempt = false;

    if (success) {
        /* remove other attempts */
        it->second.connAddrs.clear();
        mDhtMgr->dropPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));
        usedIps[address_to_string(it->second.currentConnAddr.addr)] = USED_IP_CONNECTED;

        /* update address (will come through from DISC) */

        {
            QString toLog("Successfully connected to: ");
            toLog.append(it->second.name);
            toLog.append(" (");
            toLog.append(inet_ntoa(it->second.currentConnAddr.addr.sin_addr));
            toLog.append(")");
            log(LOG_DEBUG_BASIC, CONNECTIONMANAGERZONE, toLog);
        }

        /* change state */
        it->second.state |= PEER_S_CONNECTED;
        it->second.actions |= PEER_CONNECTED;
        mStatusChanged = true;
        it->second.lastcontact = time(NULL);  /* time of connect */
        it->second.lastheard = time(NULL);
        it->second.connecttype = flags;

        return true;
    }

    log(LOG_DEBUG_BASIC, CONNECTIONMANAGERZONE, QString("Unable to connect to user: ").append(it->second.name).append(", flags: ").append(QString::number(flags)));

    usedIps.erase(address_to_string(it->second.currentConnAddr.addr));

    /* if currently connected -> flag as failed */
    if (it->second.state & PEER_S_CONNECTED) {
        it->second.state &= (~PEER_S_CONNECTED);
        it->second.actions |= PEER_DISCONNECTED;

        it->second.lastcontact = time(NULL);  /* time of disconnect */

        mDhtMgr->findPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));
        if (it->second.visState & VIS_STATE_NODHT) {
            /* hidden from DHT world */
        } else {
            //netAssistFriend(id, true);
        }
    }

    //If there are no addresses left to try, we're done
    if (it->second.connAddrs.size() < 1) {
        if (it->second.doubleTried)
            return true;
        else {
            it->second.doubleTried = true;

            it->second.schedulednexttry = time(NULL) + USED_IP_WAIT_TIME;
            return true;
        }
    }

    //Otherwise flag for additional attempts
    it->second.actions |= PEER_CONNECT_REQ;
    mStatusChanged = true;

    return true;
}

void    p3ConnectMgr::heardFrom(unsigned int librarymixer_id) {
    std::map<int, peerConnectState>::iterator it;
    it = mFriendList.find(librarymixer_id);
    if (it == mFriendList.end()) {
        log(LOG_ERROR, CONNECTIONMANAGERZONE, QString("Somehow heard from someone that is not a friend: ").append(librarymixer_id));
        return;
    }
    it->second.lastheard = time(NULL);
}



/******************************** Feedback ......  *********************************
 * From various sources
 */


void    p3ConnectMgr::peerStatus(std::string cert_id,
                                 struct sockaddr_in laddr, struct sockaddr_in raddr,
                                 uint32_t type, uint32_t flags, uint32_t source) {
    std::map<int, peerConnectState>::iterator it;

    time_t now = time(NULL);

    peerAddrInfo details;
    details.type    = type;
    details.found   = true;
    details.laddr   = laddr;
    details.raddr   = raddr;
    details.ts      = now;


    {
        QMutexLocker stack(&connMtx);

#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerStatus()";
        std::cerr << " id: " << id;
        std::cerr << " laddr: " << inet_ntoa(laddr.sin_addr);
        std::cerr << " lport: " << ntohs(laddr.sin_port);
        std::cerr << " raddr: " << inet_ntoa(raddr.sin_addr);
        std::cerr << " rport: " << ntohs(raddr.sin_port);
        std::cerr << " type: " << type;
        std::cerr << " flags: " << flags;
        std::cerr << " source: " << source;
        std::cerr << std::endl;
#endif
        {
            /* Log */
            std::ostringstream out;
            out << "p3ConnectMgr::peerStatus()";
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
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::peerStatus() Peer Not Found - Ignore";
            std::cerr << std::endl;
#endif
            return;
        }

        /* update the status */

        /* if source is DHT */
        if (source == CB_DHT) {
            /* DHT can tell us about
             * 1) connect type (UDP/TCP/etc)
             * 2) local/external address
             */
            it->second.source = CB_DHT;
            it->second.dht = details;

            /* if we are recieving these - the dht is definitely up.
             */

            netFlagDhtOk = true;
        } else if (source == CB_PERSON) {
            /* PERSON can tell us about
             * 1) online / offline
             * 2) connect address
             * -> update all!
             */

            it->second.source = CB_PERSON;
            it->second.peer = details;

            it->second.localaddr = laddr;
            it->second.serveraddr = raddr;

            /* must be online to recv info (should be connected too!)
             * but no need for action as should be connected already
             */

            it->second.netMode &= (~NET_MODE_ACTUAL); /* clear actual flags */
            if (flags & NET_FLAGS_EXTERNAL_ADDR) {
                it->second.netMode = NET_MODE_EXT;
            } else if (flags & NET_FLAGS_STABLE_UDP) {
                it->second.netMode = NET_MODE_UDP;
            } else {
                it->second.netMode = NET_MODE_UNREACHABLE;
            }


            /* always update VIS status */
            if (flags & NET_FLAGS_USE_DISC) {
                it->second.visState &= (~VIS_STATE_NODISC);
            } else {
                it->second.visState |= VIS_STATE_NODISC;
            }

            if (flags & NET_FLAGS_USE_DHT) {
                it->second.visState &= (~VIS_STATE_NODHT);
            } else {
                it->second.visState |= VIS_STATE_NODHT;
            }
        }

        /* Determine Reachability (only advisory) */
        if (ownState.netMode & NET_MODE_UDP) {
            if ((details.type & NET_CONN_UDP_DHT_SYNC) ||
                    (details.type & NET_CONN_TCP_EXTERNAL)) {
                /* reachable! */
                it->second.state &= (~PEER_S_UNREACHABLE);
            } else {
                /* unreachable */
                it->second.state |= PEER_S_UNREACHABLE;
            }
        } else if (ownState.netMode & NET_MODE_UNREACHABLE) {
            if (details.type & NET_CONN_TCP_EXTERNAL) {
                /* reachable! */
                it->second.state &= (~PEER_S_UNREACHABLE);
            } else {
                /* unreachable */
                it->second.state |= PEER_S_UNREACHABLE;
            }
        } else {
            it->second.state &= (~PEER_S_UNREACHABLE);
        }

        /* if already connected -> done */
        if (it->second.state & PEER_S_CONNECTED) {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::peerStatus() PEER ONLINE ALREADY ";
            std::cerr << " id: " << id;
            std::cerr << std::endl;
#endif
            {
                /* Log */
                std::ostringstream out;
                out << "p3ConnectMgr::peerStatus() NO CONNECT (already connected!)";
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
            }

            return;
        }


        /* are the addresses different? */

#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerStatus()";
        std::cerr << " id: " << id;
        std::cerr << " laddr: " << inet_ntoa(laddr.sin_addr);
        std::cerr << " lport: " << ntohs(laddr.sin_port);
        std::cerr << " raddr: " << inet_ntoa(raddr.sin_addr);
        std::cerr << " rport: " << ntohs(raddr.sin_port);
        std::cerr << " type: " << type;
        std::cerr << " flags: " << flags;
        std::cerr << " source: " << source;
        std::cerr << std::endl;
#endif

#ifndef P3CONNMGR_NO_AUTO_CONNECTION

#ifndef P3CONNMGR_NO_TCP_CONNECTIONS

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
            tcp_delay = P3CONNMGR_TCP_DEFAULT_DELAY;
        }

        /* if address is same -> try local */
        if ((isValidNet(&(details.laddr.sin_addr))) &&
                (sameNet(&(ownState.localaddr.sin_addr), &(details.laddr.sin_addr))))

        {
            /* add the local address */
            peerConnectAddress pca;
            pca.ts = now;
            pca.delay = tcp_delay;
            pca.period = 0;
            pca.type = NET_CONN_TCP_LOCAL;
            pca.addr = details.laddr;

#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::peerStatus() ADDING TCP_LOCAL ADDR: ";
            std::cerr << " id: " << id;
            std::cerr << " laddr: " << inet_ntoa(pca.addr.sin_addr);
            std::cerr << " lport: " << ntohs(pca.addr.sin_port);
            std::cerr << " delay: " << pca.delay;
            std::cerr << " period: " << pca.period;
            std::cerr << " type: " << pca.type;
            std::cerr << " source: " << source;
            std::cerr << std::endl;
#endif
            {
                /* Log */
                std::ostringstream out;
                out << "p3ConnectMgr::peerStatus() PushBack Local TCP Address: ";
                out << " id: " << cert_id;
                out << " laddr: " << inet_ntoa(pca.addr.sin_addr);
                out << ":" << ntohs(pca.addr.sin_port);
                out << " type: " << pca.type;
                out << " delay: " << pca.delay;
                out << " period: " << pca.period;
                out << " ts: " << pca.ts;
                out << " source: " << source;
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
            }

            it->second.connAddrs.push_back(pca);
        } else {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::peerStatus() Not adding Local Connect (Diff Network)";
            std::cerr << " id: " << id;
            std::cerr << " laddr: " << inet_ntoa(details.laddr.sin_addr);
            std::cerr << ": " << ntohs(details.laddr.sin_port);
            std::cerr << " own.laddr: " << inet_ntoa(ownState.localaddr.sin_addr);
            std::cerr << ": " << ntohs(ownState.localaddr.sin_port);
            std::cerr << std::endl;
#endif
        }


        if ((details.type & NET_CONN_TCP_EXTERNAL) &&
                (isValidNet(&(details.raddr.sin_addr))))

        {
            /* add the remote address */
            peerConnectAddress pca;
            pca.ts = now;
            pca.delay = tcp_delay;
            pca.period = 0;
            pca.type = NET_CONN_TCP_EXTERNAL;
            pca.addr = details.raddr;

#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::peerStatus() ADDING TCP_REMOTE ADDR: ";
            std::cerr << " id: " << id;
            std::cerr << " raddr: " << inet_ntoa(pca.addr.sin_addr);
            std::cerr << " rport: " << ntohs(pca.addr.sin_port);
            std::cerr << " delay: " << pca.delay;
            std::cerr << " period: " << pca.period;
            std::cerr << " type: " << pca.type;
            std::cerr << " source: " << source;
            std::cerr << std::endl;
#endif
            {
                /* Log */
                std::ostringstream out;
                out << "p3ConnectMgr::peerStatus() PushBack Remote TCP Address: ";
                out << " id: " << cert_id;
                out << " raddr: " << inet_ntoa(pca.addr.sin_addr);
                out << ":" << ntohs(pca.addr.sin_port);
                out << " type: " << pca.type;
                out << " delay: " << pca.delay;
                out << " period: " << pca.period;
                out << " ts: " << pca.ts;
                out << " source: " << source;
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
            }

            it->second.connAddrs.push_back(pca);
        } else {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::peerStatus() Not adding Remote Connect (Type != E or Invalid Network)";
            std::cerr << " id: " << id;
            std::cerr << " raddr: " << inet_ntoa(details.raddr.sin_addr);
            std::cerr << ": " << ntohs(details.raddr.sin_port);
            std::cerr << " type: " << details.type;
            std::cerr << std::endl;
#endif
        }

#endif  // P3CONNMGR_NO_TCP_CONNECTIONS

    } /****** STACK UNLOCK MUTEX *******/

    /* notify if they say we can, or we cannot connect ! */
    if (details.type & NET_CONN_UDP_DHT_SYNC) {
        retryConnectNotify(authMgr->findLibraryMixerByCertId(cert_id));
    }
#else
    } // P3CONNMGR_NO_AUTO_CONNECTION /****** STACK UNLOCK MUTEX *******/
#endif  // P3CONNMGR_NO_AUTO_CONNECTION 

    QMutexLocker stack(&connMtx);

    if (it->second.inConnAttempt) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerStatus() ALREADY IN CONNECT ATTEMPT: ";
        std::cerr << " id: " << id;
        std::cerr << std::endl;

        /*  -> it'll automatically use the addresses */
#endif

        return;
    }


    /* start a connection attempt */
    if (it->second.connAddrs.size() > 0) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerStatus() Started CONNECT ATTEMPT! ";
        std::cerr << " id: " << id;
        std::cerr << std::endl;
#endif

        it->second.actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    } else {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerStatus() No addr suitable for CONNECT ATTEMPT! ";
        std::cerr << " id: " << id;
        std::cerr << std::endl;
#endif
    }

}

void    p3ConnectMgr::peerConnectRequest(std::string id, struct sockaddr_in raddr,
        uint32_t source) {
#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::peerConnectRequest()";
    std::cerr << " id: " << id;
    std::cerr << " raddr: " << inet_ntoa(raddr.sin_addr);
    std::cerr << ":" << ntohs(raddr.sin_port);
    std::cerr << " source: " << source;
    std::cerr << std::endl;
#endif
    {
        /* Log */
        std::ostringstream out;
        out << "p3ConnectMgr::peerConnectRequest()";
        out << " id: " << id;
        out << " raddr: " << inet_ntoa(raddr.sin_addr);
        out << ":" << ntohs(raddr.sin_port);
        out << " source: " << source;
        log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
    }

    /******************** TCP PART *****************************/

#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::peerConnectRequest() Try TCP first";
    std::cerr << std::endl;
#endif

    retryConnectTCP(authMgr->findLibraryMixerByCertId(id));

    /******************** UDP PART *****************************/

    QMutexLocker stack(&connMtx);

    if (ownState.netMode & NET_MODE_UNREACHABLE) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerConnectRequest() Unreachable - no UDP connection";
        std::cerr << std::endl;
#endif
        return;
    }

    /* look up the id */
    std::map<int, peerConnectState>::iterator it;
    it = mFriendList.find(authMgr->findLibraryMixerByCertId(id));
    if (it == mFriendList.end()) {
        /* not found - ignore */
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerConnectRequest() Peer Not Found - Ignore";
        std::cerr << std::endl;
#endif
        return;
    }

    /* if already connected -> done */
    if (it->second.state & PEER_S_CONNECTED) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerConnectRequest() Already connected - Ignore";
        std::cerr << std::endl;
#endif
        return;
    }


    time_t now = time(NULL);
    /* this is a UDP connection request (DHT only for the moment!) */
    if (isValidNet(&(raddr.sin_addr))) {
        /* add the remote address */
        peerConnectAddress pca;
        pca.ts = now;
        pca.type = NET_CONN_UDP_DHT_SYNC;
        pca.delay = 0;

        if (source == CB_DHT) {
            pca.period = P3CONNMGR_UDP_DHT_DELAY;
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::peerConnectRequest() source = DHT ";
            std::cerr << std::endl;
#endif
        } else if (source == CB_PROXY) {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::peerConnectRequest() source = PROXY ";
            std::cerr << std::endl;
#endif
            pca.period = P3CONNMGR_UDP_PROXY_DELAY;
        } else {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::peerConnectRequest() source = UNKNOWN ";
            std::cerr << std::endl;
#endif
            /* error! */
            pca.period = P3CONNMGR_UDP_PROXY_DELAY;
        }

#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerConnectRequest() period = " << pca.period;
        std::cerr << std::endl;
#endif

        pca.addr = raddr;

        {
            /* Log */
            std::ostringstream out;
            out << "p3ConnectMgr::peerConnectRequest() PushBack UDP Address: ";
            out << " id: " << id;
            out << " raddr: " << inet_ntoa(pca.addr.sin_addr);
            out << ":" << ntohs(pca.addr.sin_port);
            out << " type: " << pca.type;
            out << " delay: " << pca.delay;
            out << " period: " << pca.period;
            out << " ts: " << pca.ts;
            log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
        }

        /* push to the back ... TCP ones should be tried first */
        it->second.connAddrs.push_back(pca);
    }

    if (it->second.inConnAttempt) {
        /*  -> it'll automatically use the addresses */
        return;
    }

    /* start a connection attempt */
    if (it->second.connAddrs.size() > 0) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerConnectRequest() Started CONNECT ATTEMPT! ";
        std::cerr << " id: " << id;
        std::cerr << std::endl;
#endif

        it->second.actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    } else {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::peerConnectRequest() No addr suitable for CONNECT ATTEMPT! ";
        std::cerr << " id: " << id;
        std::cerr << std::endl;
#endif
    }
}



/*******************************************************************/
/*******************************************************************/

bool p3ConnectMgr::addUpdateFriend(unsigned int librarymixer_id, QString cert, QString name) {
    QMutexLocker stack(&connMtx);

    //First try to insert the certificate
    int authResult = authMgr->addUpdateCertificate(cert, librarymixer_id);

    std::map<int, peerConnectState>::iterator it = mFriendList.find(librarymixer_id);
    //Existing friend
    if (it != mFriendList.end()) {
        //Update the name
        it->second.name = name;
        it->second.id = authMgr->findCertByLibraryMixerId(librarymixer_id);
        //If the cert has been updated
        if (authResult >= 1) {
            if (it->second.state == PEER_S_NO_CERT) {
                it->second.state = 0;
            }
            /*Note that we are simply updating the peer, and creating a new pqiperson while
              not removing its old pqiperson.
              This is because we would need to signal to pqipersongrp that the old one needs
              to be removed and a new one created simultaneously, and we don't have a convenient
              way to do that yet.*/
            it->second.actions = PEER_NEW;
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
            if ((pstate.id = authMgr->findCertByLibraryMixerId(librarymixer_id)).empty())
                return false;
            pstate.state = 0;
            //Only have pqiperson built if we actually got a key
            pstate.actions = PEER_NEW;
            mStatusChanged = true;
        }
        pstate.visState = VIS_STATE_STD;
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
bool p3ConnectMgr::removeFriend(std::string id) {

    mDhtMgr->dropPeer(id);

    QMutexLocker stack(&connMtx);

    /* move to othersList */
    bool success = false;
    std::map<int, peerConnectState>::iterator it;
    if (mFriendList.end() != (it = mFriendList.find(id))) {

        peerConnectState peer = it->second;

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

/*******************************************************************/
/*******************************************************************/
/*************** External Control ****************/
void    p3ConnectMgr::retryConnect(unsigned int librarymixer_id) {
    std::map<int, peerConnectState>::iterator it = mFriendList.find(librarymixer_id);
    if (it != mFriendList.end()) {
        //Starting a new connection attempt, so reset doubleTried
        it->second.doubleTried = false;
        retryConnectTCP(librarymixer_id);
        retryConnectNotify(librarymixer_id);
    }
}

void    p3ConnectMgr::retryConnectAll() {
    std::map<int, peerConnectState>::iterator it;
    for (it = mFriendList.begin(); it != mFriendList.end(); it++) {
        retryConnect(it->second.librarymixer_id);
    }
}


bool   p3ConnectMgr::retryConnectTCP(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    /* push addresses onto stack */
#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::retryConnectTCP()";
    std::cerr << " id: " << id;
    std::cerr << std::endl;
#endif

    /* look up the id */
    std::map<int, peerConnectState>::iterator it;
    if (mFriendList.end() == (it = mFriendList.find(librarymixer_id))) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectTCP() Peer is not Friend";
        std::cerr << std::endl;
#endif
        return false;
    }

    /* if already connected -> done */
    if (it->second.state & PEER_S_CONNECTED) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectTCP() Peer Already Connected";
        std::cerr << std::endl;
#endif
        return true;
    }

    /* are the addresses different? */

    time_t now = time(NULL);
    std::list<peerConnectAddress>::iterator cit;

    /* add in attempts ... local(TCP), remote(TCP)
     */

#ifndef P3CONNMGR_NO_TCP_CONNECTIONS

    std::string local = inet_ntoa(it->second.localaddr.sin_addr);
    std::string ext = inet_ntoa(it->second.serveraddr.sin_addr);
    /* if address is valid, on the same subnet, and not the same as external address try local */
    if (isValidNet(&(it->second.localaddr.sin_addr)) &&
            sameNet(&(ownState.localaddr.sin_addr), &(it->second.localaddr.sin_addr)) &&
            (local != ext))

    {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectTCP() Local Address Valid: ";
        std::cerr << inet_ntoa(it->second.localaddr.sin_addr);
        std::cerr << ":" << ntohs(it->second.localaddr.sin_port);
        std::cerr << std::endl;
#endif

        /* check if there is a local one on there already */

        bool localExists = false;

        if ((it->second.inConnAttempt) &&
                (it->second.currentConnAddr.type == NET_CONN_TCP_LOCAL)) {
            localExists = true;
        }

        for(cit = it->second.connAddrs.begin();
                (!localExists) && (cit != it->second.connAddrs.end()); cit++) {
            if (cit->type == NET_CONN_TCP_LOCAL)    localExists = true;
        }


        if (!localExists) {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::retryConnectTCP() Adding Local Addr to Queue";
            std::cerr << std::endl;
#endif

            /* add the local address */
            peerConnectAddress pca;
            pca.ts = now;
            pca.type = NET_CONN_TCP_LOCAL;
            pca.addr = it->second.localaddr;

            {
                /* Log */
                std::ostringstream out;
                out << "Attempting connection to friend #";
                out << it->second.librarymixer_id;
                out << " with Local IP: " << inet_ntoa(pca.addr.sin_addr);
                out << ":" << ntohs(pca.addr.sin_port);
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
            }

            it->second.connAddrs.push_back(pca);
        } else {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::retryConnectTCP() Local Addr already in Queue";
            std::cerr << std::endl;
#endif
        }
    }

    /* otherwise try external ... (should check flags) */
    //if ((isValidNet(&(it->second.serveraddr.sin_addr))) &&
    //      (it->second.netMode = NET_MODE_EXT))

    /* always try external */
    if (isValidNet(&(it->second.serveraddr.sin_addr))) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectTCP() Ext Address Valid: ";
        std::cerr << inet_ntoa(it->second.serveraddr.sin_addr);
        std::cerr << ":" << ntohs(it->second.serveraddr.sin_port);
        std::cerr << std::endl;
#endif


        /* check if there is a remote one on there already */

        bool remoteExists = false;
        if ((it->second.inConnAttempt) &&
                (it->second.currentConnAddr.type == NET_CONN_TCP_EXTERNAL)) {
            remoteExists = true;
        }

        for (cit = it->second.connAddrs.begin();
                (!remoteExists) && (cit != it->second.connAddrs.end()); cit++) {
            if (cit->type == NET_CONN_TCP_EXTERNAL) {
                remoteExists = true;
            }
        }

        if (!remoteExists) {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::retryConnectTCP() Adding Ext Addr to Queue";
            std::cerr << std::endl;
#endif

            /* add the remote address */
            peerConnectAddress pca;
            pca.ts = now;
            pca.type = NET_CONN_TCP_EXTERNAL;
            pca.addr = it->second.serveraddr;

            {
                /* Log */
                std::ostringstream out;
                out << "Attempting connection to friend #";
                out << it->second.librarymixer_id;
                out << " with Internet IP: " << inet_ntoa(pca.addr.sin_addr);
                out << ":" << ntohs(pca.addr.sin_port);
                log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
            }

            it->second.connAddrs.push_back(pca);
        } else {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::retryConnectTCP() Ext Addr already in Queue";
            std::cerr << std::endl;
#endif
        }
    }

#endif // P3CONNMGR_NO_TCP_CONNECTIONS

    /* flag as last attempt to prevent loop */
    it->second.lastattempt = time(NULL);

    if (it->second.inConnAttempt) {
        /*  -> it'll automatically use the addresses */
        return true;
    }

    /* start a connection attempt */
    if (it->second.connAddrs.size() > 0) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectTCP() Started CONNECT ATTEMPT! ";
        std::cerr << " id: " << id;
        std::cerr << std::endl;
#endif

        it->second.actions |= PEER_CONNECT_REQ;
        mStatusChanged = true;
    } else {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectTCP() No addr suitable for CONNECT ATTEMPT! ";
        std::cerr << " id: " << id;
        std::cerr << std::endl;
#endif
    }
    return true;
}


bool   p3ConnectMgr::retryConnectNotify(unsigned int librarymixer_id) {
    QMutexLocker stack(&connMtx);

    /* push addresses onto stack */
#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::retryConnectNotify()";
    std::cerr << " id: " << id;
    std::cerr << std::endl;
#endif

    /* look up the id */
    std::map<int, peerConnectState>::iterator it;

    if (mFriendList.end() == (it = mFriendList.find(librarymixer_id))) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectNotify() Peer is not Friend";
        std::cerr << std::endl;
#endif
        return false;
    }

    /* if already connected -> done */
    if (it->second.state & PEER_S_CONNECTED) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectNotify() Peer Already Connected";
        std::cerr << std::endl;
#endif
        return true;
    }

    /* flag as last attempt to prevent loop */
    it->second.lastattempt = time(NULL);

    if (ownState.netMode & NET_MODE_UNREACHABLE) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectNotify() UNREACHABLE so no Notify!";
        std::cerr << " id: " << id;
        std::cerr << std::endl;
#endif
    } else {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::retryConnectNotify() Notifying Peer";
        std::cerr << " id: " << id;
        std::cerr << std::endl;
#endif
        {
            /* Log */
            std::ostringstream out;
            out  << "p3ConnectMgr::retryConnectNotify() Notifying Peer";
            out  << " id: " << librarymixer_id;
            log(LOG_DEBUG_ALERT, CONNECTIONMANAGERZONE, out.str().c_str());
        }

        /* attempt UDP connection */
        mDhtMgr->notifyPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));
    }

    return true;
}





bool    p3ConnectMgr::setLocalAddress(unsigned int librarymixer_id, struct sockaddr_in addr) {
    QMutexLocker stack(&connMtx);

    if (librarymixer_id == ownState.librarymixer_id) {
        ownState.localaddr = addr;
        return true;
    }

    /* check if it is a friend */
    std::map<int, peerConnectState>::iterator it;
    if (mFriendList.end() == (it = mFriendList.find(librarymixer_id))) return false;

    /* "it" points to peer */
    it->second.localaddr = addr;

    return true;
}

bool    p3ConnectMgr::setExtAddress(unsigned int librarymixer_id, struct sockaddr_in addr) {
    QMutexLocker stack(&connMtx);


    if (librarymixer_id == ownState.librarymixer_id) {
        ownState.serveraddr = addr;
        return true;
    }

    /* check if it is a friend */
    std::map<int, peerConnectState>::iterator it;
    if (mFriendList.end() == (it = mFriendList.find(librarymixer_id))) return false;

    /* "it" points to peer */
    it->second.serveraddr = addr;

    return true;
}

bool    p3ConnectMgr::setNetworkMode(unsigned int librarymixer_id, uint32_t netMode) {
    if (librarymixer_id == authMgr->OwnLibraryMixerId()) {
        uint32_t visState = ownState.visState;
        setOwnNetConfig(netMode, visState);

        return true;
    }

    QMutexLocker stack(&connMtx);

    /* check if it is a friend */
    std::map<int, peerConnectState>::iterator it;
    if (mFriendList.end() == (it = mFriendList.find(librarymixer_id))) return false;

    /* "it" points to peer */
    it->second.netMode = netMode;

    return false;
}

bool    p3ConnectMgr::setVisState(unsigned int librarymixer_id, uint32_t visState) {
    if (librarymixer_id == authMgr->OwnLibraryMixerId()) {
        uint32_t netMode = ownState.netMode;
        setOwnNetConfig(netMode, visState);

        return true;
    }

    QMutexLocker stack(&connMtx);

    /* check if it is a friend */
    std::map<int, peerConnectState>::iterator it;
    if (mFriendList.end() == (it = mFriendList.find(librarymixer_id))) return false;

    /* "it" points to peer */
    it->second.visState = visState;

    /* toggle DHT state */
    if (it->second.visState & VIS_STATE_NODHT) {
        /* hidden from DHT world */
        mDhtMgr->dropPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));
    } else mDhtMgr->findPeer(authMgr->findCertByLibraryMixerId(librarymixer_id));

    return false;
}




/*******************************************************************/

int     p3ConnectMgr::getDefaultPort() {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    if (settings.value("Network/RandomizePorts", DEFAULT_RANDOM_PORTS).toBool()) {
        srand(time(NULL));
        return rand() % (PQI_MAX_RAND_PORT - PQI_MIN_RAND_PORT + 1) + PQI_MIN_RAND_PORT;
    } else {
        return PQI_DEFAULT_PORT;
    }

}

bool    p3ConnectMgr::checkNetAddress() {
#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::checkNetAddress()";
    std::cerr << std::endl;
#endif

    std::list<std::string> addrs = getLocalInterfaces();
    std::list<std::string>::iterator it;

    QMutexLocker stack(&connMtx);

    bool found = false;
    for (it = addrs.begin(); (!found) && (it != addrs.end()); it++) {
#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::checkNetAddress() Local Interface: " << *it;
        std::cerr << std::endl;
#endif

        // Ive added the 'isNotLoopbackNet' to prevent re-using the lo address if this was saved in the
        // configuration. In such a case, lo should only be chosen from getPreferredInterface as a last resort
        // fallback solution.
        //
        if ((!isLoopbackNet(&ownState.localaddr.sin_addr)) && (*it) == inet_ntoa(ownState.localaddr.sin_addr)) {
#ifdef CONN_DEBUG
            std::cerr << "p3ConnectMgr::checkNetAddress() Matches Existing Address! FOUND = true";
            std::cerr << std::endl;
#endif
            found = true;
        }
    }
    /* check that we didn't catch 0.0.0.0 - if so go for prefered */
    if ((found) && (ownState.localaddr.sin_addr.s_addr == 0)) {
        found = false;
    }

    if (!found) {
        ownState.localaddr.sin_addr = getPreferredInterface();

#ifdef CONN_DEBUG
        std::cerr << "p3ConnectMgr::checkNetAddress() Local Address Not Found: Using Preferred Interface: ";
        std::cerr << inet_ntoa(ownState.localaddr.sin_addr);
        std::cerr << std::endl;
#endif

    }
    if ((isPrivateNet(&(ownState.localaddr.sin_addr))) ||
            (isLoopbackNet(&(ownState.localaddr.sin_addr)))) {
        /* firewalled */
        //own_cert -> Firewalled(true);
    } else {
        //own_cert -> Firewalled(false);
    }

    int port = ntohs(ownState.localaddr.sin_port);
    if ((port < PQI_MIN_PORT) || (port > PQI_MAX_PORT)) {
        ownState.localaddr.sin_port = htons(getDefaultPort());
    }

    /* if localaddr = serveraddr, then ensure that the ports
     * are the same (modify server)... this mismatch can
     * occur when the local port is changed....
     */

    if (ownState.localaddr.sin_addr.s_addr ==
            ownState.serveraddr.sin_addr.s_addr) {
        ownState.serveraddr.sin_port =
            ownState.localaddr.sin_port;
    }

    // ensure that address family is set, otherwise windows Barfs.
    ownState.localaddr.sin_family = AF_INET;
    ownState.serveraddr.sin_family = AF_INET;

#ifdef CONN_DEBUG
    std::cerr << "p3ConnectMgr::checkNetAddress() Final Local Address: ";
    std::cerr << inet_ntoa(ownState.localaddr.sin_addr);
    std::cerr << ":" << ntohs(ownState.localaddr.sin_port);
    std::cerr << std::endl;
#endif

    return 1;
}

std::string p3ConnectMgr::address_to_string(struct sockaddr_in address) {
    std::ostringstream out;
    out << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port);
    return out.str();
}
