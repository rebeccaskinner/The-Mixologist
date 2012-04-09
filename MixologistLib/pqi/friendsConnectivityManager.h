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

#ifndef FRIENDS_CONNECTIVITY_MANAGER_H
#define FRIENDS_CONNECTIVITY_MANAGER_H

#include <pqi/aggregatedConnections.h>
#include <pqi/pqimonitor.h>
#include <pqi/authmgr.h>
#include <pqi/pqinetwork.h>
#include <interface/peers.h>

#include <QObject>
#include <QMutex>
#include <QMap>
#include <QStringList>

/* Used in inter-class communications with AggregatedConnectionsToFriends to describe the type of connection to make with the attempt. */
enum QueuedConnectionType {
    /* Connect to the friend via TCP using their local address. */
    CONNECTION_TYPE_TCP_LOCAL,
    /* Connect to the friend via TCP using their Internet address. */
    CONNECTION_TYPE_TCP_EXTERNAL,
    /* Send a UDP packet to the friend's Internet address requesting they connect back via TCP. */
    CONNECTION_TYPE_TCP_BACK,
    /* Connect to the friend via UDP using their Internet address. */
    CONNECTION_TYPE_UDP
};

class FriendsConnectivityManager;
class friendListing;
class upnpHandler;
class UdpSorter;

extern FriendsConnectivityManager *friendsConnectivityManager;

/**********************************************************************************
 * FriendsConnectivityManager
 * This is the master connectivity manager for the connections to friends.
 *
 * Contains a friends list that tracks the connectivity of each friend.
 * The tick method steps through list of friends and controls retry of connections with them, as well as timeouts.
 * Other classes can register with the friendsConnectivityManager as a monitor in order to be informed of friends' connectivity status changes.
 *
 * The main idea of our connection to friends strategy is as follows:
 * After our connection is set up, attempt TCP connections to all friends, which will connect all non-firewalled friends.
 *
 * If we are unfirewalled, we also send TCP connect back requests, which will catalyze connections with all full-
 * cone firewalled friends as well as restrictive-cone firewalled friends if our IP/port haven't changed.
 *
 * If we are firewalled, we begin sending a UDP tunneler packet every 20 seconds,
 * which not only opens a hole in our firewall for them, but serves as a combined
 * TCP/UDP connection request as well.
 *
 * On receipt of one of these, those friends now know we are online and firewalled, as
 * only firewalled friends send these.
 *
 * If they believe themselves to be firewalled, they will begin connecting to us via UDP
 * and send a UDP connect back request so we can connect back simultaneously.
 *
 * If they do not believe themselves to be firewalled, before they begin their UDP
 * sequence, they send a TCP connect back request so we can try again to connect them
 * via TCP. This could come in handy if they are unfirewalled, just came online, and
 * were unable to connect to us over TCP because we were firewalled and then they
 * received our UDP tunneler. Also could be helpful if our TCP connection attempt to
 * them was lost.
 *
 * Whether or not we are firewalled, the worst case is firewalled friends behind either restrictive-cone NATs when we have
 * changed IP/port or behind symmetric NAT firewalls. In the former case, we will have
 * to wait for them to update their friends list and connect to us over either TCP or
 * UDP. In the latter case, they will only be able to connect to us over TCP, (i.e. only
 * if we are unfirewalled) and only after they update their friends list to find us
 * again.
 *
 * We ordinarily update our friends list only when a friend connects to us that we don't recognize.
 * However, if we are behind a restrictive-cone NAT or a symmetric NAT then we check for friends list updates every 5 minutes.
 * This is because we may not be able to receive inbound friend requests from some friends, and so we need to keep the list updated
 * so we can try outbound requests frequently.
 *
 * Similarly, we normally retry TCP connections with friends only infrequently. However, if we are behind a symmetric NAT, that is
 * our only opportunity to make connections at all, therefore we try those frequently.
 *
 **********************************************************************************/

class FriendsConnectivityManager: public QObject {
    Q_OBJECT

public:
    /* OwnConnectivityManager must be created before creating FriendsConnectivityManager. */
    FriendsConnectivityManager();

    /* Ticked from the main server thread. */
    void tick();

    /**********************************************************************************
     * Setup
     **********************************************************************************/

    /* Adds a monitor who will be informed of friend connectivity changes.
       Must be called only while initializing from a single-thread, as the monitors are not protected by a mutex. */
    void addMonitor(pqiMonitor *mon);

    /**********************************************************************************
     * MixologistLib functions, not shared via interface with GUI
     **********************************************************************************/

    /* Returns a pointer to that friend's listing, or NULL if not found. */
    friendListing* getFriendListing(unsigned int librarymixer_id);

    /* Called by pqistreamer whenever we received a packet from a friend, updates their friendListing so we know not to time them out. */
    void heardFrom(unsigned int librarymixer_id);

    /* Returns all friends' external addresses in a list.
       Used by ownConnectivityManager in using them as STUN servers. */
    void getExternalAddresses(QList<struct sockaddr_in> &toFill);

    /* Disconnects from all friends. */
    void disconnectAllFriends();

    /**********************************************************************************
     * The interface to AggregatedConnectionsToFriends
     **********************************************************************************/
    /* AggregatedConnectionsToFriends holds the actual network connections to friends, and receives order to connect/disconnect from the FriendsConnectivityManager.
       Whenever we want to connect to a friend via a certain method, we queue an attempt into that friend's friendListing.
       We then inform the monitors, of which AggregatedConnectionsToFriends is one, that a pending connection request exists. */

    /* Called by AggregatedConnectionsToFriends to pop off a given friend's next queuedConnectionAttempt,
       which is used to populate the references so that it can use that information to actually connect.
       Returns true if the address information was successfully set into the references. */
    bool getQueuedConnectAttempt(unsigned int librarymixer_id, struct sockaddr_in &addr, QueuedConnectionType &queuedConnectionType);

    /* Called by AggregatedConnectionsToFriends to report a change in connectivity with a friend.
       This could result from a connection request initiated by getQueuedConnectAttempt(), or it could be an update on an already existing connection.
       If result is 1 it indicates connection, -1 is either disconnect or connection failure, 0 is failure but request to schedule another try via TCP.
       If the report was not of success, if there are more addresses to try, informs the monitors a pending connection request exists.
       Returns false if unable to find the friend, true in all other cases. */
    bool reportConnectionUpdate(unsigned int librarymixer_id, int result, ConnectionType type, struct sockaddr_in *remoteAddress);

    /**********************************************************************************
     * Body of public-facing API functions called through p3peers
     **********************************************************************************/

    /* List of LibraryMixer ids for all online friends. */
    void getOnlineList(QList<unsigned int> &friend_ids);

    /* List of LibraryMixer ids for all friends with encryption keys. */
    void getSignedUpList(QList<unsigned int> &friend_ids);

    /* List of LibraryMixer ids for all friends. */
    void getFriendList(QList<unsigned int> &friend_ids);

    /* Returns true if that id belongs to a friend. */
    bool isFriend(unsigned int librarymixer_id);

    /* Returns true if that id belongs to a friend, and that friend is online. */
    bool isOnline(unsigned int librarymixer_id);

    /* Returns the name of the friend with that LibraryMixer id.
       Returns an empty string if no such friend. */
    QString getPeerName(unsigned int librarymixer_id);

    /* Either adds a new friend, or updates the existing friend. */
    bool addUpdateFriend(unsigned int librarymixer_id, const QString &cert, const QString &name,
                                 const QString &localIP, ushort localPort,
                                 const QString &externalIP, ushort externalPort);

    /* Immediate retry to connect to all offline friends. */
    void tryConnectToAll();

signals:
    void friendAdded(unsigned int librarymixer_id);

    void friendConnected(unsigned int librarymixer_id);

    void friendDisconnected(unsigned int librarymixer_id);

private slots:
    /* Set whether the Mixologist should be attempting to connect to friends.
       If enabled is true, will update the friends list and then begin connectin to friends. */
    void setEnabled(bool enabled);

    /* Connected to the UDP Sorter for when we receive a UDP Tunneler packet. */
    void receivedUdpTunneler(unsigned int librarymixer_id, QString address, ushort port);

    /* Connected to the UDP Sorter for when we receive a UDP Connection Notice packet. */
    void receivedUdpConnectionNotice(unsigned int librarymixer_id, QString address, ushort port);

    /* Connected to the UDP Sorter for when we receive a request to connect-back-via-TCP. */
    void receivedTcpConnectionRequest(unsigned int librarymixer_id, QString address, ushort port);

    /* Connected to the LibraryMixerConnect so we know when we have updated our friends list. */
    void friendsListUpdated();

private:
    /* Periodically updates friends list from LibraryMixer for connection types that rely on having constantly updately information. */
    void friendsListUpdateTick();

    /* Handles connectivity with friends after our own connection is set up.
       Steps through the friends list and decides if it is time to retry a TCP conection to offline friends.
       We do not handle UDP connections as those are only begun when we receive a UDP Tunneler packet.
       Also times out friends that haven't been heard from in a while. */
    void connectivityTick();

    /* Informs registered monitors of changes in connectivity.
       Also informs the GUI of people coming online via notify. */
    void monitorsTick();

    /**********************************************************************************
     * Other helpers
     **********************************************************************************/
    /* Tries the TCP connection with the given friend.
       Any function that calls this should know not to call it if the variable readyToConnectToFriends != CONNECT_FRIENDS_READY.
       Needs mutex protection. */
    bool tryConnectTCP(unsigned int librarymixer_id);

    /* Sends a UDP packet requesting a connect back via TCP connection with the given friend.
       Any function that calls this should know not to call it if the variable readyToConnectToFriends != CONNECT_FRIENDS_READY.
       Needs mutex protection. */
    bool tryConnectBackTCP(unsigned int librarymixer_id);

    /* Tries the UDP connection with the given friend.
       Any function that calls this should know not to call it if the variable readyToConnectToFriends != CONNECT_FRIENDS_READY.
       Needs mutex protection. */
    bool tryConnectUDP(unsigned int librarymixer_id);

    /* If the friend is ready for a connection attempt (not already in attempt, not delayed, has methods to try),
       sets everything up so that the next monitorsTick will notify the monitors.
       Needs mutex protection. */
    void informMonitorsTryConnect(unsigned int librarymixer_id);

    mutable QMutex connMtx;

    /* List of pqiMonitor implementors to be informed of connection status events. */
    QList<pqiMonitor *> monitorListeners;

    /* True when the monitors need to be informed of a change. */
    bool mStatusChanged;

    /* Whether connecting to friends is currently enabled. */
    bool friendsManagerEnabled;

    /* Set by setEnabled to signal we need to download the friends list and then enable friendsManagerEnabled. */
    bool downloadFriendsAndEnable;

    /* The last time our friends list was updated from LibraryMixer. */
    time_t friendsListUpdateTime;

    /* The last time we retried connections with all friends. */
    time_t outboundConnectionTryAllTime;

    /* This is the master friends list for the Mixologist, and also tracks their connectivity.
       Keyed by librarymixer_id. */
    QMap<unsigned int, friendListing*> mFriendList;

    /* A map of IP+port combo's already in use, and their state.
       The first element is a string representation of the IP + port combo,
       and the second is whether it is used because it is connecting or if it because it is connected.
       This is used to prevent multiple ConnectionToFriends from attempting to connect to the same IP+port simultaneously. */
    enum UsedSocketState {
        USED_IP_CONNECTING,
        USED_IP_CONNECTED
    };
    QMap<QString, UsedSocketState> usedSockets;
};

/* There is one of these for each friend, stored in mFriendsList to record status of connection.
   Also there is one for self stored in ownState. */
struct friendListing {
    friendListing();

    QString name;
    std::string id; //The encryption certificate ID
    unsigned int librarymixer_id;

    /* The local and external addresses of the friend. */
    struct sockaddr_in localaddr;
    struct sockaddr_in serveraddr;

    /* When connected, this will show when connected, when disconnected, show time of last disconnect. */
    time_t lastcontact;

    /* Time of last received packet from a friend, used to calculate timeout. */
    time_t lastheard;

    /* Indicates the state of the friend, such as if we are connected or if they are not signed up for the Mixologist. */
    FriendConnectState state;

    /* Bitwise flags for actions that need to be taken. The flags are defined in pqimonitors.h. */
    uint32_t actions;

    /* Whether trying each of these connection types is scheduled.
       These are ordered by order of priority - if more than one is true, will generally try in this order. */
    bool tryTcpLocal;
    bool tryTcpExternal;
    bool tryTcpConnectBackRequest;
    bool tryUdp;

    /* Used to delay the next try. */
    time_t nextTryDelayedUntil;

    /* Which of the connection types, if any, is currently being tried. */
    QueuedConnectionType currentlyTrying;
};

#endif // FRIENDS_CONNECTIVITY_MANAGER_H
