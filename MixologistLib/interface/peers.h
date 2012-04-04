/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-8, Robert Fernie.
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

#ifndef PEER_GUI_INTERFACE_H
#define PEER_GUI_INTERFACE_H

#include <inttypes.h>
#include <string>
#include <list>
#include <QString>
#include <QObject>

/* The Main Interface Class - for information about your Peers */
class Peers;
extern Peers *peers;

/* Used to indicate the state of friends.
   Not Mixologist enabled is set when we don't have an encryption cert for the friend,
   because they haven't ever used the Mixologist (but are friends on LibraryMixer). */
enum FriendConnectState {
    FCS_NOT_MIXOLOGIST_ENABLED,
    FCS_NOT_CONNECTED,
    FCS_IN_CONNECT_ATTEMPT,
    FCS_CONNECTED_TCP,
    FCS_CONNECTED_UDP
};

/* The current state of the connection's setup. */
enum ConnectionStatus {
    /* The states below here are initial set up states. */
    /* We attempt to find two STUN servers that we will use.
       First we check if we can get two of our friends who are online and available to be our STUN servers
       by sending normal STUN requests from our test port requesting back on the test port. */
    CONNECTION_STATUS_FINDING_STUN_FRIENDS,
    /* If we failed to get at least two friends as STUN servers, we fall back to public STUN servers in order to fill us out.
       If we can't get two STUN servers then we can't auto-configure and are CONNECTION_STATUS_UNKNOWN. */
    CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS,
    /* Using our primary STUN server, send a STUN request from our test port, requesting to receive the response on our main port.
       If we receive it, if the local IP matches the external IP, we are CONNECTION_STATUS_UNFIREWALLED, otherwise we are CONNECTION_STATUS_PORT_FORWARDED. */
    CONNECTION_STATUS_STUNNING_INITIAL,
    /* Out initial STUN did not return, so we attempt to use UPNP to reach out and configure the firewall. */
    CONNECTION_STATUS_TRYING_UPNP,
    /* We received a positive response from the firewall that a mapping was added for us, and now we test the result by again
       using our primary STUN server to send a STUN request from our test port, requesting to receive the response on our main port.
       If we receive it this time, we know UPNP worked and we are CONNECTION_STATUS_UPNP_IN_USE. */
    CONNECTION_STATUS_STUNNING_UPNP_TEST,
    /* We get here if UPNP was non-existent or otherwise failed.
       We send a STUN request from our main port requesting back on the main port to the secondary STUN server.
       We use the secondary server here because we want to be able to detect using the primary STUN server in the next step
       whether sending to a single destination is enough to open a hole in our firewall for all senders, i.e. we are behind a full-cone NAT.
       If we fail here auto-configuration has failed as our STUN servers are behaving errctically and we are CONNECTION_STATUS_UNKNOWN. */
    CONNECTION_STATUS_STUNNING_MAIN_PORT,
    /* We just heard back with the normal STUN, so we now know we are definitely behind a firewall of some sort, as we can receive traffic on a given port
       that we directly requested, but cannot receive unrequested traffic.
       We again test by using our primary STUN server to send a STUN request from our test port, requesting to receive the response on our main port.
       If we receive it this time, we know we are behind a full-cone NAT, where simply sending one outbound packet is enough to open the firewall
       for all senders to reach us. Commence CONNECTION_STATUS_UDP_HOLE_PUNCHING. */
    CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST,
    /* We didn't hear back from the full-cone test, so we now will open a hole in the firewall for the primary STUN server to our main port.
       Using our primary STUN server, send a STUN request from our main port, requesting to receive the response on our main port.
       If the response has the same port number as from our CONNECTION_STATUS_STUNNING_MAIN_PORT test, we are CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING,
       and in order to be reachable, we will need to periodically contact each of our friends at the address they will use to contact us.
       If the response has a changed port, we are CONNECTION_STATUS_SYMMETRIC_NAT, and will be completely unreachable from outside.
       If we receive no response, our STUN servers are again behaving erratically and we are CONNECTION_STATUS_UNKNOWN. */
    CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST,

    /* The states below here are final states. */
    CONNECTION_STATUS_UNFIREWALLED,
    CONNECTION_STATUS_PORT_FORWARDED,
    CONNECTION_STATUS_UPNP_IN_USE,
    CONNECTION_STATUS_UDP_HOLE_PUNCHING,
    CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING,
    CONNECTION_STATUS_SYMMETRIC_NAT,
    CONNECTION_STATUS_UNKNOWN
};
inline bool connectionStatusInFinalState(int status) {return status >= CONNECTION_STATUS_UNFIREWALLED;}
inline bool connectionStatusUdpHolePunching(int status) {return (status == CONNECTION_STATUS_UDP_HOLE_PUNCHING ||
                                                                 status == CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING);}
/* Note we are treating status unknown as a good connection. Rationale is if auto-config failed, just treat is like unfirewalled. */
inline bool connectionStatusGoodConnection(int status) {return (status == CONNECTION_STATUS_UNFIREWALLED ||
                                                                status == CONNECTION_STATUS_PORT_FORWARDED ||
                                                                status == CONNECTION_STATUS_UPNP_IN_USE ||
                                                                status == CONNECTION_STATUS_UNKNOWN);}


/* Details class */
class PeerDetails {
public:

    PeerDetails();

    /* Identifying details */
    unsigned int librarymixer_id;
    QString name;

    /* Current connection status. */
    FriendConnectState state;

    /* Whether we are currently waiting and have a connection action scheduled for them. */
    bool waitingForAnAction;

    /* Address where we believe them to be, as given by LibraryMixer. */
    QString localAddr;
    uint16_t localPort;
    QString extAddr;
    uint16_t extPort;

    /* When connected, this will show when connected, when disconnected, show time of last disconnect. */
    uint32_t lastConnect;
};

class Peers: public QObject {
    Q_OBJECT

public:
    /* Returns the logged in Mixologist user's own encryption certificate id. */
    virtual std::string getOwnCertId() = 0;

    /* Returns the logged in Mixologist user's own LibraryMixer id. */
    virtual unsigned int getOwnLibraryMixerId() = 0;

    /* Returns the logged in user's own name. */
    virtual QString getOwnName() = 0;

    /* List of LibraryMixer ids for all online friends. */
    virtual void getOnlineList(QList<unsigned int> &friend_ids) = 0;

    /* List of LibraryMixer ids for all friends with encryption keys. */
    virtual void getSignedUpList(QList<unsigned int> &friend_ids) = 0;

    /* List of LibraryMixer ids for all friends. */
    virtual void getFriendList(QList<unsigned int> &friend_ids) = 0;

    /* Returns true if that id belongs to a friend. */
    virtual bool isFriend(unsigned int librarymixer_id) = 0;

    /* Returns true if that id belongs to a friend, and that friend is online. */
    virtual bool isOnline(unsigned int librarymixer_id) = 0;

    /* Returns the name of the friend with that LibraryMixer id.
       Returns an empty string if no such friend. */
    virtual QString getPeerName(unsigned int librarymixer_id) = 0;

    /* Fills in PeerDetails for user with librarymixer_id
       Can be used for friends or self.
       Returns true, or false if unable to find user with librarymixer_id*/
    virtual bool getPeerDetails(unsigned int librarymixer_id, PeerDetails &d) = 0;

    /* Either adds a new friend, or updates the existing friend. */
    virtual bool addUpdateFriend(unsigned int librarymixer_id, const QString &cert, const QString &name,
                                 const QString &localIP, ushort localPort,
                                 const QString &externalIP, ushort externalPort) = 0;

    /* Immediate retry to connect to all offline friends. */
    virtual void connectAll() = 0;

    /* The maximum and minimum values for the Mixologist's ports. */
    static const int MIN_PORT = 1024;
    static const int MAX_PORT = 50000;

signals:
    /* Used to inform the GUI of changes to the current ConnectionStatus.
       All values of newStatus should be members of ConnectionStatus. */
    void connectionStateChanged(int newStatus);
};

#endif
