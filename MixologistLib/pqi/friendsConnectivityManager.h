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

/* A pending connection attempt. */
class QueuedConnectionAttempt {
public:
    QueuedConnectionAttempt();

    /* The address to connect to. */
    struct sockaddr_in addr;

    /* What type of connection attempt should be made when dequeued. */
    QueuedConnectionType queuedConnectionType;
};

/* There is one of these for each friend, stored in mFriendsList to record status of connection.
   Also there is one for self stored in ownState. */
class peerConnectState {
public:
    peerConnectState();

    QString name;
    std::string id;
    unsigned int librarymixer_id;

    /* The local and external addresses of the friend. */
    struct sockaddr_in localaddr;
    struct sockaddr_in serveraddr;

    /* When connected, this will show when connected, when disconnected, show time of last disconnect. */
    time_t lastcontact;

    /* Time of last received packet from a friend, used to calculate timeout. */
    time_t lastheard;

    /* Request a next try to connect at a specific time. Disabled if set to 0.
       Used with usedSockets for when an IP is in use during a connection attempt and retry must be scheduled.
       Also used when a connection fails due to the other side not recognizing our encryption certificate,
       and then we schedule a try so that they have time to update their friend list before we connect again. */
    time_t nextTcpTryAt;

    /* Indicates the state of the friend, such as if we are connected or if they are not signed up for the Mixologist. */
    FriendConnectState state;

    /* Bitwise flags for actions that need to be taken. The flags are defined in pqimonitors.h. */
    uint32_t actions;

    /* A list of connect attempts to make (in order). */
    QList<QueuedConnectionAttempt> queuedConnectionAttempts;

    /* If our state is FCS_IN_CONNECT_ATTEMPT, the current attempt being tried. */
    QueuedConnectionAttempt currentConnectionAttempt;
};

class FriendsConnectivityManager;
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
 * If we are unfirewalled, we send a TCP connect back request, which will catalyze connections with all full-
 * cone firewalled friends as well as restrictive-cone firewalled friends if our IP/port haven't changed.
 *
 * If we are firewalled, we begin sending a UDP tunneler packet every 20 seconds,
 * which not only opens a hole in our firewall for them, but serves as a combined
 * TCP/UDP connection request as well, which will catalyze connections with all full-
 * cone firewalled friends as well as restrictive-cone firewalled friends if our IP/port
 * haven't changed.
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
 * We ordinarily update our friends list
 *
 **********************************************************************************/

class FriendsConnectivityManager: public QObject {
    Q_OBJECT

public:
    /* users_name is the display name of the logged in user. */
    FriendsConnectivityManager();

    /* Ticked from the main server thread. */
    void tick();

    /**********************************************************************************
     * Setup
     **********************************************************************************/

    /* Adds a monitor who will be informed of friend connectivity changes.
       Must be called only while initializing from a single-thread, as the monitors are not protected by a mutex. */
    void addMonitor(pqiMonitor *mon);

    /* Set whether the Mixologist should be attempting to connect to friends.
       If enabled is true, calls for an immediate connect attempt to all friends. */
    void setEnabled(bool enabled);

    /**********************************************************************************
     * MixologistLib functions, not shared via interface with GUI
     **********************************************************************************/

    /* Fills in state for user with librarymixer_id
       Can be used for friends or self.
       Returns true, or false if unable to find user with librarymixer_id. */
    bool getPeerConnectState(unsigned int librarymixer_id, peerConnectState &state);

    /* Called by pqistreamer whenever we received a packet from a friend, updates their peerConnectState so we know not to time them out. */
    void heardFrom(unsigned int librarymixer_id);

    void getExternalAddresses(QList<struct sockaddr_in> &toFill);

    /**********************************************************************************
     * The interface to AggregatedConnectionsToFriends
     **********************************************************************************/
    /* AggregatedConnectionsToFriends holds the actual network connections to friends, and receives order to connect/disconnect from the FriendsConnectivityManager.
       Whenever we want to connect to a friend via a certain method, we queue an attempt into that friend's peerConnectState.
       We then inform the monitors, of which AggregatedConnectionsToFriends is one, that a pending connection request exists. */
public:
    /* Called by AggregatedConnectionsToFriends to pop off a given friend's next queuedConnectionAttempt,
       which is used to populate the references so that it can use that information to actually connect.
       Returns true if the address information was successfully set into the references. */
    bool getQueuedConnectAttempt(unsigned int librarymixer_id, struct sockaddr_in &addr, QueuedConnectionType &queuedConnectionType);

    /* Called by AggregatedConnectionsToFriends to report a change in connectivity with a friend.
       This could result from a connection request initiated by getQueuedConnectAttempt(), or it could be an update on an already existing connection.
       If result is 1 it indicates connection, -1 is either disconnect or connection failure, 0 is failure but request to schedule another try via TCP.
       If the report was not of success, if there are more addresses to try, informs the monitors a pending connection request exists.
       Returns false if unable to find the friend, true in all other cases. */
    bool reportConnectionUpdate(unsigned int librarymixer_id, int result);

private:
    /* Queues the given connectionType of a friend identified by connectState for a connection attempt.
       localOrExternal is only used if queuedConnectionType is TCP. */
    void queueConnectionAttempt(peerConnectState* connectState, QueuedConnectionType queuedConnectionType);

    /**********************************************************************************
     * Body of public-facing API functions called through p3peers
     **********************************************************************************/
public:
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
    bool addUpdateFriend(unsigned int librarymixer_id, const QString &cert, const QString &name,
                                 const QString &localIP, ushort localPort,
                                 const QString &externalIP, ushort externalPort);

    /* Immediate retry to connect to all offline friends. */
    void tryConnectAll();

private slots:
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
       Any function that calls this should know not to call it if the variable readyToConnectToFriends != CONNECT_FRIENDS_READY. */
    bool tryConnectTCP(unsigned int librarymixer_id);

    /* Sends a UDP packet requesting a connect back via TCP connection with the given friend.
       Any function that calls this should know not to call it if the variable readyToConnectToFriends != CONNECT_FRIENDS_READY. */
    bool tryConnectBackTCP(unsigned int librarymixer_id);

    /* Tries the UDP connection with the given friend.
       Any function that calls this should know not to call it if the variable readyToConnectToFriends != CONNECT_FRIENDS_READY. */
    bool tryConnectUDP(unsigned int librarymixer_id);

    mutable QMutex connMtx;

    /* List of pqiMonitor implementors to be informed of connection status events. */
    QList<pqiMonitor *> monitorListeners;

    /* True when the monitors need to be informed of a change. */
    bool mStatusChanged;

    /* Whether connecting to friends is currently enabled. */
    bool friendsManagerEnabled;

    /* When the last time our friends list was updated from LibraryMixer. */
    time_t friendsListUpdateTime;

    /* This is the master friends list for the Mixologist, and also tracks their connectivity. */
    QMap<unsigned int, peerConnectState> mFriendList; //librarymixer_ids and peerConnectStates

    /* A map of IP+port combo's already in use, and their state.
       The first element is a string representation of the IP + port combo,
       and the second is whether it is used because it is connecting or if it because it is connected.
       This is used to prevent multiple ConnectionToFriends from attempting to connect to the same IP+port simultaneously. */
    enum UsedSocketState {
        USED_IP_CONNECTING,
        USED_IP_CONNECTED
    };
    QMap<QString, UsedSocketState> usedSockets;

    friend class OwnConnectivityManager;
};

#endif // FRIENDS_CONNECTIVITY_MANAGER_H
