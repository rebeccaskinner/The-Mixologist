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
#include "pqi/pqiassist.h"

#include <QMutex>

/* This is the master connection manager for the entire program.
   Sets up own connection such as ports and UPNP mappings.
   Also contains loop that steps through list of friends and controls retry of connections with them. */

class p3ConnectMgr;
extern p3ConnectMgr *connMgr;

/* VIS_STATE_XXXX
 * determines how public this peer wants to be...
 *
 * STD = advertise to Peers / DHT checking etc
 * GRAY = share with friends / but not DHT
 * DARK = hidden from all
 * BROWN? = hidden from friends / but on DHT
 */


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

const uint32_t VIS_STATE_NODISC = 0x0001;
const uint32_t VIS_STATE_NODHT  = 0x0002;

const uint32_t VIS_STATE_STD    = 0x0000;
const uint32_t VIS_STATE_GRAY   = VIS_STATE_NODHT;
const uint32_t VIS_STATE_DARK   = VIS_STATE_NODISC | VIS_STATE_NODHT;
const uint32_t VIS_STATE_BROWN  = VIS_STATE_NODISC;

/* Startup NetModes of what we should try */
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

const uint32_t NET_CONN_TCP_LOCAL       = 0x0001;
const uint32_t NET_CONN_TCP_EXTERNAL    = 0x0002;
const uint32_t NET_CONN_UDP_DHT_SYNC    = 0x0010;
const uint32_t NET_CONN_UDP_PEER_SYNC   = 0x0020; /* coming soon */

/* flags of peerStatus */
const uint32_t NET_FLAGS_USE_DISC       = 0x0001;
const uint32_t NET_FLAGS_USE_DHT        = 0x0002;
const uint32_t NET_FLAGS_ONLINE         = 0x0004;
const uint32_t NET_FLAGS_EXTERNAL_ADDR  = 0x0008;
const uint32_t NET_FLAGS_STABLE_UDP     = 0x0010;
const uint32_t NET_FLAGS_TRUSTS_ME      = 0x0020;

const uint32_t TCP_STD_TIMEOUT_PERIOD   = 5; /* 5 seconds! */

//used for usedIp map in p3ConnectMgr
const short USED_IP_CONNECTING = 1;
const short USED_IP_CONNECTED = 2;

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
    uint32_t type;
    time_t ts;
};

/*There is one of these for each friend, stored in mFriendsList to record status of connection.
  Also there is one for self stored in ownState.*/
class peerConnectState {
public:
    peerConnectState();

    QString name; //Display name of friend
    std::string id; //cert_id
    unsigned int librarymixer_id;

    uint32_t netMode; /* EXT / UPNP / UDP / INVALID */
    uint32_t visState; /* STD, GRAY, DARK */

    struct sockaddr_in localaddr; //Local address of peer
    struct sockaddr_in serveraddr; //Global address of peer

    /* When connected, this will show when connected, when disconnected, show when disconnected.*/
    time_t lastcontact;

    uint32_t connecttype;  // NET_CONN_TCP_ALL / NET_CONN_UDP_ALL
    /*Time of last attempt to connect to an offline friend */
    time_t lastattempt;
    /*Time of last received packet from a friend */
    time_t lastheard;
    /*Request a next try to connect at a specific time. Disabled if set to 0.
      Used with usedIps for when an IP is in use during a connection attempt and retry must be scheduled.*/
    time_t schedulednexttry;
    /*Under the current connection algorithm, each try is actually two back-to-back tries.
      This is because if the Mixologist restarts it will generate a new encryption certificate.
      The first attempt to connect will be rejected because of an invalid certificate, which
      prompts the other side's Mixologist to update its encryption key list from Library Mixer.
      The other side will then automatically try to connect to all friends with changes (which
      would include us).
      However, if we are firewalled by the other side is not, this will fail, but a second
      attempt to connect after a set amount of time will succeed.

      If this is false upon completion of a connection attempt, a schedulednexttry is setup.*/
    bool doubleTried;

    uint32_t state;  //The connection state
    uint32_t actions;

    uint32_t source; /* most current source */
    peerAddrInfo dht;
    peerAddrInfo peer;

    bool inConnAttempt;
    peerConnectAddress currentConnAddr; //current address being tried
    /* A list of connect attempts to make (in order)
    Items are added by retryConnectTCP, which adds the localaddr and serveraddr,
    as well as by the DHT functions.  Items are removed when used. */
    std::list<peerConnectAddress> connAddrs;

};


class p3ConnectMgr: public pqiConnectCb {

public:
    /* users_name is the display name of the logged in user. */
    p3ConnectMgr(QString users_name);

    /* Ticked from the main server thread. */
    void tick();

    /*************** Setup ***************************/

    /* Adds the dht manager. */
    void addNetAssistConnect(pqiNetAssistConnect *dht) {mDhtMgr = dht;}

    //If port randomization is set, then generates a random port.
    //Otherwise, returns the default port.
    int getDefaultPort();

    /* Sets own IP address and port that will be used by the Mixologist. */
    bool checkNetAddress();

    /*************** External Control ****************/
    bool shutdown(); /* blocking shutdown call */

    //Retries both TCP and UDP
    void retryConnect(unsigned int librarymixer_id);
    void retryConnectAll();

    bool getUPnPState() {
        return mUpnpMgr->getActive();
    }
    bool getUPnPEnabled() {
        return mUpnpMgr->getEnabled();
    }
    bool getDHTEnabled() {
        return mDhtMgr->getEnabled();
    }

    bool getNetStatusOk() {
        return netFlagOk;
    }
    bool getNetStatusUpnpOk() {
        return netFlagUpnpOk;
    }
    bool getNetStatusDhtOk() {
        return netFlagDhtOk;
    }
    bool getNetStatusExtOk() {
        return netFlagExtOk;
    }
    bool getNetStatusUdpOk() {
        return netFlagUdpOk;
    }
    bool getNetStatusTcpOk() {
        return netFlagTcpOk;
    }
    bool getNetResetReq() {
        return netFlagResetReq;
    }

    //Sets own netMode with try flags, and sets visState
    void setOwnNetConfig(uint32_t netMode, uint32_t visState);
    bool setLocalAddress(unsigned int librarymixer_id, struct sockaddr_in addr);
    bool setExtAddress(unsigned int librarymixer_id, struct sockaddr_in addr);

    bool setNetworkMode(unsigned int librarymixer_id, uint32_t netMode);
    bool setVisState(unsigned int librarymixer_id, uint32_t visState);

    /* add/remove friends */
    //Either adds a new friend, or updates the existing friend
    bool addUpdateFriend(unsigned int librarymixer_id, QString cert, QString name);
    //bool  removeFriend(std::string);

    /*************** External Control ****************/

    /* access to network details (called through Monitor) */
    const std::string getOwnCertId();

    bool isFriend(unsigned int librarymixer_id);
    bool isOnline(unsigned int librarymixer_id);
    /*Fills in state for user with librarymixer_id
      Can be used for friends or self.
      Returns true, or false if unable to find user with librarymixer_id*/
    bool getPeerConnectState(unsigned int librarymixer_id, peerConnectState &state);
    //Returns either friend's name or a blank string if no such friend
    QString getFriendName(unsigned int librarymixer_id);
    QString getFriendName(std::string cert_id);

    QString getOwnName();
    //Like getFriendList but only returns friends that are online
    void getOnlineList(std::list<int> &peers);
    //Like getFriendList but only returns friends with Mixology encryption keys
    void getSignedUpList(std::list<int> &peers);
    //Returns a list (based on mFriendsList) of all librarymixer_ids of friends
    void getFriendList(std::list<int> &peers);

    /**************** handle monitors *****************/
    void addMonitor(pqiMonitor *mon);

    /******* overloaded from pqiConnectCb *************/
    //3 functions are related to DHT
    virtual void peerStatus(std::string cert_id,
                               struct sockaddr_in laddr, struct sockaddr_in raddr,
                               uint32_t type, uint32_t flags, uint32_t source);
    virtual void peerConnectRequest(std::string id,
                                       struct sockaddr_in raddr, uint32_t source);
    virtual void stunStatus(std::string id, struct sockaddr_in raddr, uint32_t type, uint32_t flags);

    /****************** Connections *******************/
    /* Called by pqipersongrp with a cert_id to get that member of mFriendsList
    and pop off the next member of connAddrs, which is used to populate the references.*/
    bool connectAttempt(unsigned int librarymixer_id, struct sockaddr_in &addr,
                           uint32_t &delay, uint32_t &period, uint32_t &type);
    /* Called by pqipersongrp when one of its persons has reported the results
    of it connection. Sets the person's peerConnectState in mFriendsList appropriately.
    If the report was not of success, if there are more connAddrs to try in
    peerConnecctState, sets its action to PEER_CONNECT_REQ.
    If doubleTried is false, then sets it to true and schedules a retry.*/
    bool connectResult(unsigned int librarymixer_id, bool success, uint32_t flags);
    /* Called by pqistreamer whenever we received a packet from a friend, updates
       their peerConnectState so we know not to time them out. */
    void heardFrom(unsigned int librarymixer_id);


private:
    //Handles own net status
    void netTick();
    //Handles status of peers
    void statusTick();
    //Informs registered monitors of changes in connectivity
    //Also informs the GUI of people coming online via notify
    void tickMonitors();
    //Starts up own net connection
    void netStartup();

    /* Called from within netTick when mNetStatus is NET_UPNP_INIT to start UPNP. */
    void netUpnpInit();
    /* Called from within netTick when mNetStatus is NET_UPNP_SETUP.
       Will be called multiple times until either UPNP returns successfully or fails to succeed before time out. */
    void netUpnpCheck();

    void netUdpCheck();
    void netUnreachableCheck();

    /* Checks and makes sure UPNP is still properly functioning periodically.
       Called from netTick.
       Does not perform any action if called within the past minute.
       Tick happens 1/second or more, seems excessive to be testing the connection health that frequently.
       Should either later mtest this assumption, or compare against established software to see common practice. */
    void netUpnpMaintenance();

    /* Udp / Stun functions */
    bool udpInternalAddress(struct sockaddr_in iaddr);
    bool udpExtAddressCheck();
    void udpStunPeer(std::string id, struct sockaddr_in &addr);

    void stunInit();
    bool stunCheck();
    void stunCollect(std::string id, struct sockaddr_in addr, uint32_t flags);

    /* connect attempts */
    //Retries the TCP connection.
    //Called by the generic retryConnect as well as by statustick and peerConnectRequest.
    bool retryConnectTCP(unsigned int librarymixer_id);
    //Retries the UDP connection
    bool retryConnectNotify(unsigned int librarymixer_id);

    //Utility function for usedIps to convert a sockaddr_in into a string
    //representation of form ###.###.###.###:##.
    std::string address_to_string(struct sockaddr_in address);

    pqiNetAssistFirewall *mUpnpMgr;
    pqiNetAssistConnect *mDhtMgr;

    mutable QMutex connMtx; /* protects below */

    time_t   mNetInitTS;
    /* Indicates the status of setting up the network.  Will be one of
       NET_UNKNOWN, NET_UPNP_INIT, NET_UPNP_SETUP,
       NET_UDP_SETUP, or NET_DONE */
    uint32_t mNetStatus;

    uint32_t mStunStatus;
    uint32_t mStunFound;
    bool  mStunMoreRequired;

    //List of pqiMonitor implementors to be informed of connection status events
    std::list<pqiMonitor *> clients;
    //True when the monitors need to be informed of a change
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
    std::map<int, peerConnectState> mFriendList; //librarymixer_ids and peerConnectStates

    /* A map of IP+port combo's already in use, and their state.  The first element is a string
    representation of the IP + port combo, and the second is either USED_IP_CONNECTING or
    USED_IP_CONNECTED.  This is used to prevent multiple pqipersons from attempting to
    connect to the same address simultaneously */
    std::map<std::string, int> usedIps;

};

#endif // MRK_PQI_CONNECTION_MANAGER_HEADER




