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

#include <pqi/pqinetwork.h>
#include <pqi/pqi_base.h>
#include <pqi/authmgr.h>

#include <QHash>

/*
 * There is one pqissllistener for all of the Mixologist.
 * It listens on TCP on the main Mixologist port for incoming connections.
 *
 */

class pqissl;

class pqissllistener;
extern pqissllistener *sslListener;

class pqissllistener {
public:

    pqissllistener();

    /* Begins listening on the set listen address. */
    int setuplisten(struct sockaddr_in *addr);

    /* Handles listening for connections and accepting incoming connections.
       Called from AggregatedConnectionsToFriends's tick. */
    int tick();

    /* Stops listening and restarts to a default state. */
    int resetlisten();

    /**********************************************************************************
     * Methods called by pqissl
     **********************************************************************************/

    /* Adds a friend to knownFriends. Called from pqissl. */
    int addFriendToListenFor(const QString &cert_id, pqissl *acc);

    /* Removes a friend from knownFriends.  Called from pqissl. */
    int removeFriendToListenFor(const QString &cert_id);

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

    mutable QMutex listenerMutex;

    /* The address on which the listener has been directed to listen. */
    struct sockaddr_in listenAddress;

    /* The socket on which we are listening. */
    int listeningSocket;

    /* Whether setuplisten has yet been called on this listener. */
    bool listenerActive;

    /* This is a list of incoming SSL connections that we were waiting for more data on.
       It is looped through by continueaccepts() to attempt to see if we can now complete them. */
    QHash<SSL *, struct sockaddr_in> incompleteIncomingConnections;

    //A map of certificates that we're accepting connections from and the pqissl to handle them.
    QHash<QString, pqissl *> knownFriends;
};

#endif // MRK_PQI_SSL_LISTEN_HEADER
