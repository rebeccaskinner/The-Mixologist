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

#ifndef MRK_PQI_SSL_HEADER
#define MRK_PQI_SSL_HEADER

#include <openssl/ssl.h>

// operating system specific network header.
#include "pqi/pqinetwork.h"

#include <string>
#include <map>

#include "pqi/pqi_base.h"

#include "pqi/p3connmgr.h"

#include "pqi/authmgr.h"

#include <QMutex>

/*
 * pqissl
 *
 * This provides the base OpenSSL-wrapping interface class for an individual connection
 * (AuthMgr wraps some other non-individual connection OpenSSL functions.)
 *
 * Each certificate has a pqissl, created in pqisslpersongrp when a friend is added,
 * which represents the SSL TCP connection to the friend represented by that certificate.
 * A pqissl isn't updated when a friend gets a new certificate, instead a new one is created for that friend.
 *
 * A pqissl also isn't tied to any particular address, and can be called with different addresses to attempt to connect.
 *
 * This is subclassed by pqissludp.
 *
 */

class pqissllistener;

class pqissl: public NetBinInterface {
    friend class pqissllistener;

public:
    pqissl(pqissllistener *l, PQInterface *parent);
    virtual ~pqissl();

    /**********************************************************************************
     * NetInterface
     **********************************************************************************/

    /* Attempt a connection to the friend at the specified address.
       Returns 1 on success, 0 on failure (may be temporary, in which case can simply try again), -1 on error. */
    virtual int connect(struct sockaddr_in raddr);

    /* Begin listening for connections from this friend.
       Returns 1 on success, 0 on not ready, and -1 on error. */
    virtual int listen();

    /* Stops listening for connections from this friend.
       Returns 1 on success or if already not listening, and -1 on error. */
    virtual int stoplistening();

    /* Closes any existing connection with the friend, and resets to initial condition.
       Can be called even if not currently connected. */
    virtual void reset();

    /* Sets the specified parameter to value.
       Returns false if no such parameter applies to the NetInterface. */
    virtual bool setConnectionParameter(netParameters type, uint32_t value);

    /**********************************************************************************
     * BinInterface
     **********************************************************************************/
    /* Ticked by the containing pqiperson.
       Attempts to connect */
    virtual int tick();

    /* Sends data with length.
       Returns the amount of data sent, or -1 on error. */
    virtual int senddata(void *data, int length);

    /* Returns -1 on errors, otherwise returns the size in bytes of the packet this is read. */
    virtual int readdata(void *data, int length);

    /* Returns true when connected. */
    virtual bool isactive();

    /* Returns true when there is more data available to be read by the connection. */
    virtual bool moretoread();

    /* Returns true when more data is ready to be sent by the connection. */
    virtual bool cansend();

    /* Closes any existing connection with the friend, and resets to initial condition.
       Can be called even if not currently connected. */
    virtual void close();

    /* If a connection is with a friend on the same LAN, then we'll let that connection be excused from bandwidth balancing in pqistreamer. */
    virtual bool bandwidthLimited() {return sameLAN;}

protected:
    /**********************************************************************************
     * Internals of the SSL connection
     **********************************************************************************/
    /* Variable that indicates state of the connection in progress.
       A completed connection and an idle connection are both STATE_IDLE. */
    enum connectionStates {
        STATE_IDLE = 0,
        STATE_INITIALIZED = 1,
        STATE_WAITING_FOR_SOCKET_CONNECT = 2,
        STATE_WAITING_FOR_SSL_CONNECT = 3,
        STATE_WAITING_FOR_SSL_AUTHORIZE = 4,
        STATE_FAILED = 5
    };
    connectionStates connectionState;

    /* This looks at the connectionState and calls other functions to handle based upon that. */
    int ConnectAttempt();

    /* Looks at whether we have been requested to delay connecting.
       If we have, then sets the appropriate state and delay.
       If we haven't, or if we've fulfilled the delay request, immediately calls Initiate_Connection().
       Returns 0 while delayed, -1 on errors. */
    int Init_Or_Delay_Connection();

    /* Opens a socket, makes it non-blocking, then connects it to the target address.
       Returns 1 on success, 0 if not ready, -1 on errors. */
    virtual int Initiate_Connection();

    /* Completes the basic TCP connection by calling Basic_Connection_Complete().
       Then initiates the SSL connection.
       Returns 1 on success, -1 on errors. */
    int Initiate_SSL_Connection();
    /* Completes the basic TCP connection to the target address.
       Returns 1 on success, 0 if not ready, -1 on errors. */
    virtual int Basic_Connection_Complete();

    /* Completes the SSL connection by calling SSL_Connection_Complete().
       Then calls accept() to finalize the connection.
       Returns 1 on success, 0 if not ready, -1 on errors. */
    int Authorize_SSL_Connection();
    /* Completes the basic SSL connection over the already created connection.
       Returns 1 on success, 0 if not ready, -1 on errors. */
    int SSL_Connection_Complete();

    /* Final internal processing of the SSL connection.
       By the time this is called, the connection is already set up.
       This performs all the necessary actions to internally mark the connection as completed,
       as well as informed the parent pqiperson that the connection is complete.
       Called both by pqissl from Authorize_SSL_Connection() and also by pqissllistener from acceptInbound() to finalize connections. */
    int accept(SSL *ssl, int fd, struct sockaddr_in foreign_addr);
    /* Simply calls accept() but wraps it in a mutex.
       Designed so that pqissllistener can safely call accept. */
    int acceptInbound(SSL *ssl, int fd, struct sockaddr_in foreign_addr);

    /* Informs the parent that the connection attempt ended in failure. */
    virtual int Failed_Connection();

    /* protected internal fns that are overloaded for udp case. */
    virtual int net_internal_close(int fd_to_close) {return unix_close(fd_to_close);}
    virtual int net_internal_SSL_set_fd(SSL *ssl, int fd) {return SSL_set_fd(ssl, fd);}
    virtual int net_internal_fcntl_nonblock(int fd) {return unix_fcntl_nonblock(fd);}

    /* Whether this connection has already been fully set up. */
    bool currentlyConnected;

    /* Whether this is a passive UDP connection or an active TCP connection. */
    enum SSLModes {
        PQISSL_PASSIVE = 0x00, //UDP Connection
        PQISSL_ACTIVE = 0x01 //TCP Connection
    };
    SSLModes sslmode;

    /* Attributes of the connection (or connection in progress). */
    SSL *ssl_connection;
    int mOpenSocket;
    struct sockaddr_in remote_addr;

    /* Pointer to the global sslListener that we'll need to register with. */
    pqissllistener *sslListener;

    /* Reading in packets may require multiple calls to readdata().
       This stores how far in we've made it into our read so far. */
    int readSoFar;

    /* Some flags that indicate the status of the various interfaces (local), (server).
       Unused by TCP, only used by UDP. */
    unsigned int net_attempt;
    unsigned int net_failure;
    unsigned int net_unreachable;

    bool sameLAN; //Whether we are on the same subnet as this friend. Used to exempt from bandwidth balancing.

    /* SSL will throw an error called zero return when the connection is closed.
       If this repeatedly occurs, the connection should be marked dead, so this tracks the number of occurrences. */
    int errorZeroReturnCount;

    /* Variables for the delay set by setConnectionParameter. */
    uint32_t mRequestedConnectionDelay;
    time_t mConnectionDelayedUntil;

    /* Variables for the connection attempt timeout set by setConnectionParameter. */
    uint32_t mConnectionAttemptTimeout;
    time_t mConnectionAttemptTimeoutAt;

    /* When the SSL connection on top of the basic connection should timeout.
       Ensures that we don't get stuck (can happen on udp!) */
    time_t mSSLConnectionAttemptTimeoutAt;

private:
    mutable QMutex pqisslMtx; /* accept can be called from a separate thread, so we need mutex protection */
};

#endif // MRK_PQI_SSL_HEADER
