/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-8, Robert Fernie
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

#ifndef AGGREGATED_CONNECTIONS_H
#define AGGREGATED_CONNECTIONS_H

#include "pqi/pqihandler.h"
#include "pqi/pqilistener.h"
#include "pqi/pqiservice.h"
#include "pqi/pqimonitor.h"

/*
 * AggregatedConnectionsToFriends
 *
 * This is the aggregate that contains all connections to friends.
 *
 * It holds one listening socket that listens on TCP for the entire application - the pqilistener.
 *
 * It also holds a series of ConnectionToFriends, each of which represents the connection to that friend (through whatever means of connection).
 *
 * This is also the service server, to which services can be attached.
 *
 */

/* Used to describe types of connections. */
enum ConnectionType {
    TCP_CONNECTION,
    UDP_CONNECTION
};

class AggregatedConnectionsToFriends;
extern AggregatedConnectionsToFriends *aggregatedConnectionsToFriends;

class ConnectionToFriend;

class AggregatedConnectionsToFriends: public pqihandler, public pqiMonitor, public p3ServiceServer {
public:
    AggregatedConnectionsToFriends();

    /* Creates the listener and has it begin listening on the listen address. */
    void init_listener();

    /* Stops the listener and destroys it. */
    void stop_listener();

    /* Loads the transfer rates from the settings files and sets them. */
    void load_transfer_rates();

    /* This tick is called from ftserver, which perhaps should be revisited. */
    virtual int tick();

    /* Called by the FriendsConnectivityManager's tick function with a list of pqipeers whose statuses have changed.
       For each friend in the list, handles if their action is PEER_NEW, PEER_CONNECT_REQ, or PEER_TIMEOUT. */
    virtual void statusChange(const std::list<pqipeer> &changedFriends);

    /* Called by the contained ConnectionToFriends to inform when a connection has been made.
       If result is 1 it indicates connection, -1 is either disconnect or connection failure, 0 is failure but request to schedule another try. */
    bool notifyConnect(unsigned int librarymixer_id, int result, ConnectionType type, struct sockaddr_in *remoteAddress);

private:
    /* Creates a new ConnectionToFriend object to represent a friend. */
    ConnectionToFriend *createPerson(std::string id, unsigned int librarymixer_id, pqilistener *listener);

    /* These functions are called by statusChange to handle the various events. */
    /* Creates and adds a new friend. */
    int addPeer(std::string id, unsigned int librarymixer_id);

    /* Removes an existing friend, resetting any connections to them. */
    bool removePeer(unsigned int librarymixer_id);

    /* Calls the FriendsConnectivityManager and dequeues and processes a queued connection attempt. */
    int connectPeer(unsigned int librarymixer_id);

    /* Resets the connection with a connected friend. */
    void timeoutPeer(unsigned int librarymixer_id);

    /* Handles the ticking for the p3ServiceServer. */
    int tickServiceRecv();
    int tickServiceSend();

    /* The global incoming TCP connection listening socket for all. */
    pqilistener *pqil;
};
#endif // AGGREGATED_CONNECTIONS_H
