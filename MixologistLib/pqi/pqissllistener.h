/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-6, Robert Fernie
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

#ifndef MRK_PQI_SSL_LISTEN_HEADER
#define MRK_PQI_SSL_LISTEN_HEADER

#include <openssl/ssl.h>

// operating system specific network header.
#include "pqi/pqinetwork.h"

#include <string>
#include <map>

#include "pqi/pqi_base.h"
#include "pqi/pqilistener.h"

#include "pqi/authmgr.h"

/*
 * Specific implementation of a pqilistener that utilizes SSL.
 *
 * In practice, there is one pqissllistener for all of the Mixologist.
 * It listens on the Mixologist port set by the user or UPNP for incoming connections.
 *
 */

class pqissl;

class pqissllistener: public pqilistener {
public:

    pqissllistener(struct sockaddr_in *addr);

    /**********************************************************************************
     * pqilistener interface
     **********************************************************************************/

    /* Handles listening for connections and accepting incoming connections.
       Called from AggregatedConnectionsToFriends's tick. */
    virtual int tick();

    /* Sets the address on which the listener will be listening. */
    virtual int setListenAddr(struct sockaddr_in addr);

    /* Begins listening on the set listen address. */
    virtual int setuplisten();

    /* Stops listening and restarts to a default state. */
    virtual int resetlisten();

    /**********************************************************************************
     * Methods called by pqissl
     **********************************************************************************/

    /* Adds a friend to knownFriends. Called from pqissl. */
    int addFriendToListenFor(std::string id, pqissl *acc);

    /* Removes a friend from knownFriends.  Called from pqissl. */
    int removeFriendToListenFor(std::string id);

private:
    /* Checks the port to see if there are any inbound connections, and if there are begins to connect. */
    int acceptconnection();

    /* Steps through saved connections that were incomplete and were waiting for more data to see if they can
       now be completed. */
    void continueaccepts();

    /* Attempts to continue connecting the specified SSL connection.
       Returns 1 on success, 0 if it is still processing, -1 on failure. */
    int continueSSL(SSL *ssl, struct sockaddr_in remote_addr, bool newConnection);

    /* Gets the certificate from ssl and checks it.
       If everything is okay, hands it off to that friend's pqissl's accept function to complete everything. */
    int completeConnection(int fd, SSL *ssl, struct sockaddr_in &remote_addr);

    /* The address on which the listener has been directed to listen. */
    struct sockaddr_in listenAddress;

    /* The socket on which we are listening. */
    int listeningSocket;

    /* Whether setuplisten has yet been called on this listener. */
    bool listenerActive;

    /* This is a list of incoming SSL connections that we were waiting for more data on.
       It is looped through by continueaccepts() to attempt to see if we can now complete them. */
    std::map<SSL *, struct sockaddr_in> incompleteIncomingConnections;

    //A map of certificates that we're accepting connections from and the pqissl to handle them.
    std::map<std::string, pqissl *> knownFriends;
};

#endif // MRK_PQI_SSL_LISTEN_HEADER
