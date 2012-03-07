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

#ifndef MRK_PQI_CONNECTION_MANAGER_HEADER
#define MRK_PQI_CONNECTION_MANAGER_HEADER

#include "pqi/pqimonitor.h"
#include "pqi/authmgr.h"
#include "pqi/pqinetwork.h"

#include <QMutex>
#include <QMap>

/***** Framework / initial implementation for a connection manager.
 *
 * This needs a state machine for Initialisation.
 *
 * Network state:
 *   NET_UNKNOWN
 *   NET_EXT_UNKNOWN * forwarded port (but Unknown Ext IP) *
 *   NET_EXT_KNOWN   * forwarded port with known IP/Port. *
 *
 *   NET_UPNP_CHECK  * checking for UPnP *
 *   NET_UPNP_KNOWN  * confirmed UPnP ext Ip/port *
 *
 *   NET_UDP_UNKNOWN * not Ext/UPnP - to determine Ext IP/Port *
 *   NET_UDP_KNOWN   * have Stunned for Ext Addr *
 *
 *  Transitions:
 *
 *  NET_UNKNOWN -(config)-> NET_EXT_UNKNOWN
 *  NET_UNKNOWN -(config)-> NET_UPNP_UNKNOWN
 *  NET_UNKNOWN -(config)-> NET_UDP_UNKNOWN
 *
 *  NET_EXT_UNKNOWN -(DHT(ip)/Stun)-> NET_EXT_KNOWN
 *
 *  NET_UPNP_UNKNOWN -(Upnp)-> NET_UPNP_KNOWN
 *  NET_UPNP_UNKNOWN -(timout/Upnp)-> NET_UDP_UNKNOWN
 *
 *  NET_UDP_UNKNOWN -(stun)-> NET_UDP_KNOWN
 *
 *
 * STUN state:
 *  STUN_INIT * done nothing *
 *  STUN_DHT  * looking up peers *
 *  STUN_DONE * found active peer and stunned *
 *
 *
 * Steps.
 *******************************************************************
 * (1) Startup.
 *  - UDP port setup.
 *  - DHT setup.
 *  - Get Stun Keys -> add to DHT.
 *  - Feedback from DHT -> ask UDP to stun.
 *
 * (1) determine Network mode.
 *  If external Port.... Done:
 * (2)
 *******************************************************************
 * Stable operation:
 * (1) tick and check peers.
 * (2) handle callback.
 * (3) notify of new/failed connections.
 *
 *
 */

/* Startup NetModes of what we should try.
   TRYMODE is a mask for all of them. */
const uint32_t NET_MODE_TRYMODE =    0x00f0;

const uint32_t NET_MODE_TRY_EXT  =   0x0010;
const uint32_t NET_MODE_TRY_UPNP =   0x0020;
const uint32_t NET_MODE_TRY_UDP  =   0x0040;

/* Actual NetModes that indicate the current state */
const uint32_t NET_MODE_ACTUAL =      0x000f;

const uint32_t NET_MODE_UNKNOWN =     0x0000;
const uint32_t NET_MODE_EXT =         0x0001;
const uint32_t NET_MODE_UPNP =        0x0002;
const uint32_t NET_MODE_UDP =         0x0004;
const uint32_t NET_MODE_UNREACHABLE = 0x0008;

/* order of attempts ... */
const uint32_t NET_CONN_TCP_ALL         = 0x000f;
const uint32_t NET_CONN_UDP_ALL         = 0x00f0;

/* Used by peerConnectAddress to indicate the type of connection to make. */
const uint32_t NET_CONN_TCP_LOCAL       = 0x0001;
const uint32_t NET_CONN_TCP_EXTERNAL    = 0x0002;
const uint32_t NET_CONN_UDP_DHT_SYNC    = 0x0010;

/* flags of peerStatus */
/* Removed DHT related
const uint32_t NET_FLAGS_USE_DISC       = 0x0001;
const uint32_t NET_FLAGS_USE_DHT        = 0x0002;*/
const uint32_t NET_FLAGS_ONLINE         = 0x0004;
const uint32_t NET_FLAGS_EXTERNAL_ADDR  = 0x0008;
const uint32_t NET_FLAGS_STABLE_UDP     = 0x0010;
const uint32_t NET_FLAGS_TRUSTS_ME      = 0x0020;

const uint32_t TCP_STD_TIMEOUT_PERIOD   = 5; /* 5 seconds! */

class peerAddrInfo {
public:
    peerAddrInfo();

    bool found;
    uint32_t type;
    struct sockaddr_in laddr, raddr;
    time_t ts;
};

class peerConnectAddress {
public:
    peerConnectAddress();

    struct sockaddr_in addr;
    uint32_t delay;  /* to stop simultaneous connects */
    uint32_t period; /* UDP only */
    uint32_t type; //Directives on how the connection should be made
};

/* There is one of these for each friend, stored in mFriendsList to record status of connection.
   Also there is one for self stored in ownState. */
class peerConnectState {
public:
    peerConnectState();

    QString name; //Display name of friend, not set for self
    std::string id; //cert_id
    unsigned int librarymixer_id;

    /* This contains flags for not only what our current netMode is, but also what we want to try. */
    uint32_t netMode; /* EXT / UPNP / UDP / INVALID */

    struct sockaddr_in localaddr; //Local address of peer
    struct sockaddr_in serveraddr; //Global address of peer

    /* When connected, this will show when connected, when disconnected, show time of last disconnect. */
    time_t lastcontact;

    uint32_t connecttype;  // NET_CONN_TCP_ALL / NET_CONN_UDP_ALL

    /* Time of last attempt to connect to an offline friend. */
    time_t lastattempt;

    /* Time of last received packet from a friend. */
    time_t lastheard;

    /* Under the current connection algorithm, each try is actually two back-to-back tries.
       This is because if the Mixologist restarts it will generate a new encryption certificate.
       The first attempt to connect will be rejected because of an invalid certificate, which
       prompts the other side's Mixologist to update its encryption key list from LibraryMixer.
       The other side will then automatically try to connect to all friends with changes (which
       would include us).
       However, if we are firewalled but the other side is not, this will fail, but a second
       attempt to connect after a set amount of time will succeed.

       If this is false upon completion of a connection attempt, a schedulednexttry is setup.*/
    bool doubleTried;

    /* Request a next try to connect at a specific time. Disabled if set to 0.
       Used with usedIps for when an IP is in use during a connection attempt and retry must be scheduled. */
    time_t schedulednexttry;

    uint32_t state;  //The connection state
    uint32_t actions;

    uint32_t source; /* most current source */
    peerAddrInfo dht;
    peerAddrInfo peer;

    bool inConnAttempt;
    peerConnectAddress currentConnAddr; //current address being tried

    /* A list of connect attempts to make (in order).
       Items are added by retryConnectTCP, which adds the localaddr and serveraddr,
       as well as by the DHT functions.  Items are removed when used. */
    std::list<peerConnectAddress> connAddrs;

};

class ConnectivityManager;
class upnphandler;
class p3DhtMgr;

/**********************************************************************************
 * This is the master connectivity manager for the Mixiologist.
 * Handles initial set up own connection such as ports and UPNP mappings.
 * Also contains a friends list that tracks the connectivity of each friend.
 * The tick method steps through list of friends and controls retry of connections with them, as well as timeouts.
 * Other classes can register with the connMgr as a monitor in order to be informed of friends' connectivity status changes.
 **********************************************************************************/

extern ConnectivityManager *connMgr;

class ConnectivityManager: public pqiConnectCb {

public:
    /* users_name is the display name of the logged in user. */
    ConnectivityManager();

    /* Ticked from the main server thread. */
    void tick();

    /**********************************************************************************
     * Setup
     **********************************************************************************/

    /* Adds a monitor who will be informed of friend connectivity changes.
       Must be called only while initializing from a single-thread, as the monitors are not protected by a mutex. */
    void addMonitor(pqiMonitor *mon);

    /* Sets own IP address and port that will be used by the Mixologist.
       Called once on init, and repeatedly in netTick() while we don't have an Internet address yet. */
    void checkNetAddress();

    /* Blocking call that shuts down the connection manager. */
    bool shutdown();

    /**********************************************************************************
     * MixologistLib functions, not shared via interface with GUI
     **********************************************************************************/

    /* Fills in state for user with librarymixer_id
       Can be used for friends or self.
       Returns true, or false if unable to find user with librarymixer_id. */
    bool getPeerConnectState(unsigned int librarymixer_id, peerConnectState &state);

    /* pqipersongrp is closer to the actual network connections than the ConnectivityManager.
       pqipersongrp is a Monitor of statuses, and whenever the ConnectivityManager signals with PEER_CONNECT_REQ that we should try to
       connect to a given friend, this is called by pqipersongrp on the friend to pop off the next member of connAddrs,
       which is used to populate the references so that it can use that information to actually connect.
       Returns true if the address information was successfully set into the references. */
    bool getQueuedConnectAttempt(unsigned int librarymixer_id, struct sockaddr_in &addr, uint32_t &delay, uint32_t &period, uint32_t &type);

    /* Called by pqipersongrp to report a change in connectivity with a friend.
       This could result from a connection request initiated by getQueuedConnectAttempt(), or it could be an update on an already existing connection.
       Updates the friend's state in the ConnectivityManager appropriately.
       If the report was not of success, if there are more connAddrs to try in peerConnecctState, sets its action to PEER_CONNECT_REQ.
       If we haven't doubleTried yet, then schedules our second try.
       Returns false if unable to find the friend, true in all other cases. */
    bool reportConnectionUpdate(unsigned int librarymixer_id, bool success, uint32_t flags);

    /* Called by pqistreamer whenever we received a packet from a friend, updates their peerConnectState so we know not to time them out. */
    void heardFrom(unsigned int librarymixer_id);

    /**********************************************************************************
     * Body of public-facing API functions called through p3peers
     **********************************************************************************/
    /* List of LibraryMixer ids for all online friends. */
    void getOnlineList(std::list<int> &ids);

    /* List of LibraryMixer ids for all friends with encryption keys. */
    void getSignedUpList(std::list<int> &ids);

    /* List of LibraryMixer ids for all friends. */
    void getFriendList(std::list<int> &ids);

    /* Returns true if that id belongs to a friend. */
    bool isFriend(unsigned int librarymixer_id);

    /* Returns true if that id belongs to a friend, and that friend is online. */
    bool isOnline(unsigned int librarymixer_id);

    /* Returns the name of the friend with that LibraryMixer id.
       Returns an empty string if no such friend. */
    QString getPeerName(unsigned int librarymixer_id);

    /* Either adds a new friend, or updates the existing friend. */
    bool addUpdateFriend(unsigned int librarymixer_id, QString cert, QString name);

    /* Immediate retry to connect to that friend. */
    void retryConnect(unsigned int librarymixer_id);

    /* Immediate retry to connect to all offline friends. */
    void retryConnectAll();

    bool setLocalAddress(unsigned int librarymixer_id, struct sockaddr_in addr);
    bool setExtAddress(unsigned int librarymixer_id, struct sockaddr_in addr);
    bool setNetworkMode(unsigned int librarymixer_id, uint32_t netMode);

    /******* overloaded from pqiConnectCb *************/
    //3 functions are related to DHT
    virtual void peerStatus(std::string cert_id,
                               struct sockaddr_in laddr, struct sockaddr_in raddr,
                               uint32_t type, uint32_t flags, uint32_t source);
    virtual void peerConnectRequest(std::string id,
                                       struct sockaddr_in raddr, uint32_t source);
    virtual void stunStatus(std::string id, struct sockaddr_in raddr, uint32_t type, uint32_t flags);

private:
    /* Looks at our own netStatus and performs the required actions.
       Basically, the netStatus can be set to be starting something up, and this will see that and do startup.
       Other than the startup states, there is a done state for netStatus, and that simply performs
       maintenance actions. */
    void netTick();

    /* Steps through the friends list and decides if it is time to retry to connect to offline friends.
       Also times out friends that haven't been heard from in a while. */
    void statusTick();

    /* Informs registered monitors of changes in connectivity.
       Also informs the GUI of people coming online via notify. */
    void monitorsTick();

    /* Called from tick on the first time tick runs. */
    void netStartup();

    /* If port randomization is not set, returns the saved port if it is valid.
       If port randomization is set, then generates a random port.
       If there is no saved port, generates a random port and saves it. */
    int getLocalPort();

    /* Called from within netTick when mNetStatus is NET_UPNP_INIT to start UPNP. */
    void netUpnpInit();

    /* Called from within netTick when mNetStatus is NET_UPNP_SETUP.
       Will be called multiple times until either UPNP returns successfully or fails to succeed before time out. */
    void netUpnpCheck();

    void netUdpCheck();
    void netUnreachableCheck();

    /* Checks and makes sure UPNP is still properly functioning periodically.
       Called from netTick.
       Does not perform any action if called within the past 5 minutes.
       Tick happens 1/second or more, seems excessive to be testing the connection health that frequently.
       Should either later test this assumption, or compare against established software to see common practice. */
    void netUpnpMaintenance();

    /* Retries the TCP connection.
       Called by the generic retryConnect as well as by statustick and peerConnectRequest. */
    bool retryConnectTCP(unsigned int librarymixer_id);

    /* Retries the UDP connection.
       Called by generic retryConnect. */
    bool retryConnectUDP(unsigned int librarymixer_id);

    /* Queues the given connectionType of a friend identified by connectState for a connection attempt. */
    void queueConnectionAttempt(peerConnectState* connectState, uint32_t connectionType);

    /* Utility function for usedIps to convert a sockaddr_in into a string representation of form ###.###.###.###:## */
    QString static addressToString(struct sockaddr_in address);

    /* Udp / Stun functions */
    bool udpInternalAddress(struct sockaddr_in iaddr);
    bool udpExtAddressCheck();

    /* Adds a peer to the stun try list. */
    void udpStunPeer(std::string id, struct sockaddr_in &addr);

    void stunInit();
    bool stunCheck();
    void stunCollect(std::string id, struct sockaddr_in addr, uint32_t flags);

    upnphandler *mUpnpMgr;
    p3DhtMgr *mDhtMgr;

    mutable QMutex connMtx; /* protects below */

    time_t   mNetInitTS;
    /* Indicates the status of setting up the network.  Will be one of
       NET_UNKNOWN, NET_UPNP_INIT, NET_UPNP_SETUP,
       NET_UDP_SETUP, or NET_DONE */
    uint32_t mNetStatus;

    uint32_t mStunStatus;
    uint32_t mStunFound;
    bool  mStunMoreRequired;

    /* List of pqiMonitor implementors to be informed of connection status events. */
    QList<pqiMonitor *> monitorListeners;

    /* True when the monitors need to be informed of a change. */
    bool  mStatusChanged;

    /* external Address determination */
    bool mUpnpAddrValid, mStunAddrValid;
    bool mStunAddrStable;
    struct sockaddr_in mUpnpExtAddr;
    struct sockaddr_in mStunExtAddr;

    /* network status flags (read by interface) */
    bool netFlagOk;
    bool netFlagUpnpOk;
    bool netFlagDhtOk;
    bool netFlagExtOk;
    bool netFlagUdpOk;
    bool netFlagTcpOk;
    bool netFlagResetReq;

    peerConnectState ownState;

    std::list<std::string> mStunList;

    /* This is the master friends list for the Mixologist */
    QMap<unsigned int, peerConnectState> mFriendList; //librarymixer_ids and peerConnectStates

    /* A map of IP+port combo's already in use, and their state.  The first element is a string
    representation of the IP + port combo, and the second is whether it is used because it is
    connecting or if it because it is connected.
    This is used to prevent multiple pqipersons from attempting to connect to the same address simultaneously */
    enum UsedIPState {
        USED_IP_CONNECTING,
        USED_IP_CONNECTED
    };
    QMap<QString, UsedIPState> usedIps;

};

#endif // MRK_PQI_CONNECTION_MANAGER_HEADER
