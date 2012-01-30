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

#include "pqi/pqissl.h"
#include "pqi/pqinetwork.h"

#include "util/net.h"
#include "util/debug.h"

#include "interface/peers.h"

#include <unistd.h>
#include <errno.h>
#include <openssl/err.h>

#include <sstream>

#include "pqi/pqissllistener.h"

static const int PQISSL_MAX_READ_ZERO_COUNT = 20;
static const int PQISSL_SSL_CONNECT_TIMEOUT = 30;

/********** PQI SSL STUFF ******************************************
 *
 * A little note on the notifyEvent(FAILED)....
 *
 * this is called from
 * (1) reset if needed!
 * (2) Determine_Remote_Address (when all options have failed).
 *
 * reset() is only called when a TCP/SSL connection has been
 * established, and there is an error. If there is a failed TCP
 * connection, then an alternative address can be attempted.
 *
 * reset() is called from
 * (1) destruction.
 * (2) disconnect()
 * (3) bad waiting state.
 *
 * // TCP/or SSL connection already established....
 * (5) pqissl::SSL_Connection_Complete() <- okay -> because we made a TCP connection already.
 * (6) pqissl::accept() <- okay because something went wrong.
 * (7) moretoread()/cansend() <- okay
 *
 */

pqissl::pqissl(pqissllistener *l, PQInterface *parent)
    :NetBinInterface(parent, parent->PeerId(), parent->LibraryMixerId()),
     waiting(WAITING_NOT), active(false), certvalid(false),
     sslmode(PQISSL_ACTIVE), ssl_connection(NULL), sockfd(-1),
     pqil(l),  // no init for remote_addr.
     readpkt(NULL), pktlen(0),
     attempt_ts(0),
     net_attempt(0), net_failure(0), net_unreachable(0),
     sameLAN(false), n_read_zero(0),
     mConnectDelay(0), mConnectTS(0),
     mConnectTimeout(0), mTimeoutTS(0)

{
    /* set address to zero */
    sockaddr_clear(&remote_addr);

    {
        std::ostringstream out;
        out << "pqissl for PeerId: " << PeerId();
        log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());
    }

    return;
}

pqissl::~pqissl() {
    log(LOG_DEBUG_ALERT, PQISSLZONE,
        "pqissl::~pqissl -> destroying pqissl");
    stoplistening(); /* remove from pqissllistener only */
    reset();
    return;
}


/********** Implementation of NetInterface *************************/

int pqissl::connect(struct sockaddr_in raddr) {
    // reset failures
    net_failure = 0;
    remote_addr = raddr;
    remote_addr.sin_family = AF_INET;

    return ConnectAttempt();
}

// tells pqilistener to listen for us.
int pqissl::listen() {
    if (pqil) return pqil -> addlistenaddr(PeerId(), this);
    return 0;
}

int     pqissl::stoplistening() {
    if (pqil) pqil -> removeListenPort(PeerId());
    return 1;
}

int     pqissl::disconnect() {
    return reset();
}

/* BinInterface version of reset() for pqistreamer */
int     pqissl::close() {
    return reset();
}

// put back on the listening queue.
int     pqissl::reset() {
    std::ostringstream out;

    /* a reset shouldn't cause us to stop listening
     * only reasons for stoplistening() are;
     *
     * (1) destruction.
     * (2) connection.
     * (3) WillListen state change
     *
     */

    {
        QString toLog("pqissl::reset Resetting connection with: ");
        toLog.append(peers->getPeerName(peers->findLibraryMixerByCertId(PeerId())));
        toLog.append(" (");
        toLog.append(inet_ntoa(remote_addr.sin_addr));
        toLog.append(")");
        log(LOG_DEBUG_ALERT, PQISSLZONE, toLog);
    }
    out << "pqissl::reset State Before Reset:" << std::endl;
    out << "\tActive: " << (int) active << std::endl;
    out << "\tsockfd: " << sockfd << std::endl;
    out << "\twaiting: " << waiting << std::endl;
    out << "\tssl_con: " << ssl_connection << std::endl;
    out << std::endl;

    bool neededReset = false;

    if (ssl_connection != NULL) {
        out << "pqissl::reset Shutting down SSL Connection";
        out << std::endl;
        SSL_shutdown(ssl_connection);
        neededReset = true;
    }

    if (sockfd > 0) {
        out << "pqissl::reset() Shutting down (active) socket";
        out << std::endl;
        net_internal_close(sockfd);
        sockfd = -1;
        neededReset = true;
    }
    active = false;
    sockfd = -1;
    waiting = WAITING_NOT;
    ssl_connection = NULL;
    sameLAN = false;
    n_read_zero = 0;
    total_len = 0 ;

    if (neededReset) {
        out << "pqissl::reset() Reset Required!" << std::endl;
        out << "pqissl::reset() Will Attempt notifyEvent(NET_CONNECT_FAILED)";
        out << std::endl;
    }

    out << "pqissl::reset() Complete!" << std::endl;
    log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());

    // notify people of problem!
    // but only if we really shut something down.
    if (neededReset) {
        // clean up the streamer
        if (parent()) {
            parent() -> notifyEvent(this, NET_CONNECT_FAILED);
        }
    }
    return 1;
}

bool    pqissl::connect_parameter(uint32_t type, uint32_t value) {
    {
        std::ostringstream out;
        out << "pqissl::connect_parameter() Peer: " << PeerId();
        out << " type: " << type << "value: " << value;
        log(LOG_DEBUG_ALL, PQISSLZONE, out.str().c_str());
    }

    if (type == NET_PARAM_CONNECT_DELAY) {
        std::ostringstream out;
        out << "pqissl::connect_parameter() Peer: " << PeerId();
        out << " DELAY: " << value;
        log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());


        mConnectDelay = value;
        return true;
    } else if (type == NET_PARAM_CONNECT_TIMEOUT) {
        std::ostringstream out;
        out << "pqissl::connect_parameter() Peer: " << PeerId();
        out << " TIMEOUT: " << value;
        log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());

        mConnectTimeout = value;
        return true;
    }
    return false;
    //return NetInterface::connect_parameter(type, value);
}


/********** End of Implementation of NetInterface ******************/
/********** Implementation of BinInterface **************************
 * Only status() + tick() are here ... as they are really related
 * to the NetInterface, and not the BinInterface,
 *
 */

/* returns ...
 * -1 if inactive.
 *  0 if connecting.
 *  1 if connected.
 */

int     pqissl::status() {
    int alg;

    std::ostringstream out;
    out << "pqissl::status()";

    if (active) {
        out << " active: " << std::endl;
        // print out connection.
        out << "Connected TO : " << PeerId();
        out << std::endl;
        // print out cipher.
        out << "\t\tSSL Cipher:" << SSL_get_cipher(ssl_connection);
        out << " (" << SSL_get_cipher_bits(ssl_connection, &alg);
        out << ":" << alg << ") ";
        out << "Vers:" << SSL_get_cipher_version(ssl_connection);
        out << std::endl;
        out << std::endl;

    } else {
        out << " Waiting for connection!" << std::endl;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());

    if (active) {
        return 1;
    } else if (waiting > 0) {
        return 0;
    }
    return -1;
}

// tick......
int pqissl::tick() {
    //pqistreamer::tick();

    // continue existing connection attempt.
    if (!active) {
        // if we are waiting.. continue the connection (only)
        if (waiting > 0) {
            pqisslMtx.lock();
            std::ostringstream out;
            out << "pqissl::tick() ";
            out << "Continuing Connection Attempt for " << PeerId();
            log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());

            ConnectAttempt();
            pqisslMtx.unlock();
            return 1;
        }
    }
    return 1;
}

/********** End of Implementation of BinInterface ******************/
/********** Internals of SSL Connection ****************************/


int     pqissl::ConnectAttempt() {
    switch (waiting) {
        case WAITING_NOT:

            sslmode = PQISSL_ACTIVE; /* we're starting this one */

            log(LOG_DEBUG_BASIC, PQISSLZONE,
                "pqissl::ConnectAttempt() STATE = Not Waiting, starting connection");

            return Init_Or_Delay_Connection();

            break;
        case WAITING_DELAY:

            log(LOG_DEBUG_BASIC, PQISSLZONE,
                "pqissl::ConnectAttempt() STATE = Waiting Delay, starting connection");

            return Init_Or_Delay_Connection();

            break;

        case WAITING_SOCK_CONNECT:

            log(LOG_DEBUG_BASIC, PQISSLZONE,
                "pqissl::ConnectAttempt() STATE = Waiting Sock Connect");

            return Initiate_SSL_Connection();
            break;

        case WAITING_SSL_CONNECTION:

            log(LOG_DEBUG_BASIC, PQISSLZONE,
                "pqissl::ConnectAttempt() STATE = Waiting SSL Connection");

            return Authorise_SSL_Connection();
            break;

        case WAITING_SSL_AUTHORISE:

            log(LOG_DEBUG_BASIC, PQISSLZONE,
                "pqissl::ConnectAttempt() STATE = Waiting SSL Authorise");

            return Authorise_SSL_Connection();
            break;
        case WAITING_FAIL_INTERFACE:

            log(LOG_DEBUG_BASIC, PQISSLZONE,
                "pqissl::ConnectAttempt() Failed - Retrying");

            return Failed_Connection();
            break;


        default:
            log(LOG_DEBUG_ALERT, PQISSLZONE,
                "pqissl::ConnectAttempt() STATE = Unknown - Reset");

            reset();
            break;
    }
    log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::ConnectAttempt() Unknown");

    return -1;
}


/****************************** FAILED ATTEMPT ******************************
 * Determine the Remote Address.
 *
 * Specifics:
 * TCP / UDP
 * TCP - check for which interface to use.
 * UDP - check for request proxies....
 *
 * X509 / XPGP - Same.
 *
 */

int     pqissl::Failed_Connection() {
    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::ConnectAttempt() Failed - Notifying");

    if (parent()) {
        parent() -> notifyEvent(this, NET_CONNECT_UNREACHABLE);
    }
    waiting = WAITING_NOT;

    return 1;
}

/****************************** MAKE CONNECTION *****************************
 * Open Socket and Initiate Connection.
 *
 * Specifics:
 * TCP / UDP
 * TCP - socket()/connect()
 * UDP - tou_socket()/tou_connect()
 *
 * X509 / XPGP - Same.
 *
 */

int     pqissl::Init_Or_Delay_Connection() {
    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Init_Or_Delay_Connection() Attempting Outgoing Connection....");

    if (waiting == WAITING_NOT) {
        waiting = WAITING_DELAY;

        /* set delay */
        if (mConnectDelay == 0) {
            return Initiate_Connection();
        }

        /* set Connection TS.
         */
        {
            std::ostringstream out;
            out << "pqissl::Init_Or_Delay_Connection() ";
            out << " Delaying Connection to ";
            out << PeerId() << " for ";
            out << mConnectDelay;
            out << " seconds";
            log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());
        }


        mConnectTS = time(NULL) + mConnectDelay;
        return 0;
    } else if (waiting == WAITING_DELAY) {
        {
            std::ostringstream out;
            out << "pqissl::Init_Or_Delay_Connection() ";
            out << " Connection to ";
            out << PeerId() << " starting in ";
            out << mConnectTS - time(NULL);
            out << " seconds";
            log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());
        }

        if (time(NULL) > mConnectTS) {
            return Initiate_Connection();
        }
        return 0;
    }

    log(LOG_DEBUG_ALERT, PQISSLZONE,
        "pqissl::Initiate_Connection() Already Attempt in Progress!");
    return -1;
}


int     pqissl::Initiate_Connection() {
    int err;
    struct sockaddr_in addr = remote_addr;

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Initiate_Connection() Attempting Outgoing Connection....");

    if (waiting != WAITING_DELAY) {
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::Initiate_Connection() Already Attempt in Progress!");
        return -1;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Initiate_Connection() Opening Socket");

    // open socket connection to addr.
    int osock = unix_socket(PF_INET, SOCK_STREAM, 0);

    {
        std::ostringstream out;
        out << "pqissl::Initiate_Connection() osock = " << osock;
        log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());
    }

    if (osock < 0) {
        std::ostringstream out;
        out << "Failed to open socket!" << std::endl;
        out << "Socket Error:" << socket_errorType(errno) << std::endl;
        log(LOG_WARNING, PQISSLZONE, out.str().c_str());

        net_internal_close(osock);
        waiting = WAITING_FAIL_INTERFACE;
        return -1;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Initiate_Connection() Making Non-Blocking");

    err = unix_fcntl_nonblock(osock);
    if (err < 0) {
        std::ostringstream out;
        out << "Error: Cannot make socket NON-Blocking: ";
        out << err << std::endl;
        log(LOG_WARNING, PQISSLZONE, out.str().c_str());

        waiting = WAITING_FAIL_INTERFACE;
        net_internal_close(osock);
        return -1;
    }

    {
        std::ostringstream out;
        out << "pqissl::Initiate_Connection() ";
        out << "Connecting To: ";
        out << PeerId() << " via: ";
        out << inet_ntoa(addr.sin_addr);
        out << ":" << ntohs(addr.sin_port);
        log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
    }

    if (addr.sin_addr.s_addr == 0) {
        std::ostringstream out;
        out << "pqissl::Initiate_Connection() ";
        out << "Invalid (0.0.0.0) Remote Address,";
        out << " Aborting Connect.";
        out << std::endl;
        log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
        waiting = WAITING_FAIL_INTERFACE;
        net_internal_close(osock);
        return -1;
    }


    mTimeoutTS = time(NULL) + mConnectTimeout;
    //std::cerr << "Setting Connect Timeout " << mConnectTimeout << " Seconds into Future " << std::endl;

    if (0 != (err = unix_connect(osock, (struct sockaddr *) &addr, sizeof(addr)))) {
        std::ostringstream out;
        out << "pqissl::Initiate_Connection() unix_connect returns:";
        out << err << " -> errno: " << errno << " error: ";
        out << socket_errorType(errno) << std::endl;

        if (errno == EINPROGRESS) {
            // set state to waiting.....
            waiting = WAITING_SOCK_CONNECT;
            sockfd = osock;

            out << " EINPROGRESS Waiting for Socket Connection";
            log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());

            return 0;
        } else if ((errno == ENETUNREACH) || (errno == ETIMEDOUT)) {
            out << "ENETUNREACHABLE: cert: " << PeerId();
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

            // Then send unreachable message.
            net_internal_close(osock);
            osock=-1;
            //reset();

            waiting = WAITING_FAIL_INTERFACE;
            // removing unreachables...
            //net_unreachable |= net_attempt;

            return -1;
        }

        /* IF we get here ---- we Failed for some other reason.
                 * Should abandon this interface
         * Known reasons to get here: EINVAL (bad address)
         */

        out << "Error: Connection Failed: " << errno;
        out << " - " << socket_errorType(errno) << std::endl;

        net_internal_close(osock);
        osock=-1;
        waiting = WAITING_FAIL_INTERFACE;

        log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
        // extra output for the moment.
        //std::cerr << out.str();

        return -1;
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::Init_Connection() connect returned 0");
    }

    waiting = WAITING_SOCK_CONNECT;
    sockfd = osock;

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Initiate_Connection() Waiting for Socket Connect");

    return 1;
}


/******************************  CHECK SOCKET   *****************************
 * Check the Socket.
 *
 * select() and getsockopt().
 *
 * Specifics:
 * TCP / UDP
 * TCP - select()/getsockopt()
 * UDP - tou_error()
 *
 * X509 / XPGP - Same.
 *
 */

int     pqissl::Basic_Connection_Complete() {
    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Basic_Connection_Complete()...");

    /* new TimeOut code. */
    if (time(NULL) > mTimeoutTS) {
        std::ostringstream out;
        out << "pqissl::Basic_Connection_Complete() Connection Timed Out. ";
        out << "Peer: " << PeerId() << " Period: ";
        out << mConnectTimeout;

        log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());
        /* as sockfd is valid, this should close it all up */

        reset();
        return -1;
    }


    if (waiting != WAITING_SOCK_CONNECT) {
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::Basic_Connection_Complete() Wrong Mode");
        return -1;
    }
    // use select on the opened socket.
    // Interestingly - This code might be portable....

    if (sockfd < 0) return -1;
    unsigned int validSockfd = sockfd;

    fd_set ReadFDs, WriteFDs, ExceptFDs;
    FD_ZERO(&ReadFDs);
    FD_ZERO(&WriteFDs);
    FD_ZERO(&ExceptFDs);

    FD_SET(validSockfd, &ReadFDs);
    FD_SET(validSockfd, &WriteFDs);
    FD_SET(validSockfd, &ExceptFDs);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Basic_Connection_Complete() Selecting ....");

    int sr = 0;
    if (0 > (sr = select(sockfd + 1,
                         &ReadFDs, &WriteFDs, &ExceptFDs, &timeout))) {
        // select error.
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::Basic_Connection_Complete() Select ERROR(1)");

        net_internal_close(sockfd);
        sockfd=-1;
        //reset();
        waiting = WAITING_FAIL_INTERFACE;
        return -1;
    }

    {
        std::ostringstream out;
        out << "pqissl::Basic_Connection_Complete() Select ";
        out << " returned " << sr;
        log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());
    }


    if (FD_ISSET(sockfd, &ExceptFDs)) {
        // error - reset socket.
        // this is a definite bad socket!.

        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::Basic_Connection_Complete() Select ERROR(2)");

        net_internal_close(sockfd);
        sockfd=-1;
        //reset();
        waiting = WAITING_FAIL_INTERFACE;
        return -1;
    }

    if (FD_ISSET(sockfd, &WriteFDs)) {
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::Basic_Connection_Complete() Can Write!");
    } else {
        // happens frequently so switched to debug msg.
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::Basic_Connection_Complete() Not Yet Ready!");
        return 0;
    }

    if (FD_ISSET(sockfd, &ReadFDs)) {
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::Basic_Connection_Complete() Can Read!");
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::Basic_Connection_Complete() No Data to Read!");
    }

    int err = 1;
    if (0==unix_getsockopt_error(sockfd, &err)) {
        if (err == 0) {

            {
                std::ostringstream out;
                out << "Established connection to ";
                out << inet_ntoa(remote_addr.sin_addr);
                out << ":";
                out << ntohs(remote_addr.sin_port);
                out << ", initializing encrypted connection";
                log(LOG_WARNING, PQISSLZONE, out.str().c_str());
                out.clear();
                out << "pqissl::Basic_Connection_Complete()";
                out << " TCP Connection Complete: cert: ";
                out << PeerId();
                out << " on osock: " << sockfd;
                log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
            }
            return 1;
        } else if (err == EINPROGRESS) {

            std::ostringstream out;
            out << "pqissl::Basic_Connection_Complete()";
            out << " EINPROGRESS: cert: " << PeerId();
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

            return 0;
        } else if ((err == ENETUNREACH) || (err == ETIMEDOUT)) {
            std::ostringstream out;
            out << "pqissl::Basic_Connection_Complete()";
            out << " ENETUNREACH/ETIMEDOUT: cert: ";
            out << PeerId();
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

            // Then send unreachable message.
            net_internal_close(sockfd);
            sockfd=-1;
            //reset();

            waiting = WAITING_FAIL_INTERFACE;
            // removing unreachables...
            //net_unreachable |= net_attempt;

            return -1;
        } else if ((err == EHOSTUNREACH) || (err == EHOSTDOWN)) {
            std::ostringstream out;
            out << "pqissl::Basic_Connection_Complete()";
            out << " EHOSTUNREACH/EHOSTDOWN: cert: ";
            out << PeerId();
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

            // Then send unreachable message.
            net_internal_close(sockfd);
            sockfd=-1;
            //reset();
            waiting = WAITING_FAIL_INTERFACE;

            return -1;
        } else if ((err == ECONNREFUSED)) {
            std::ostringstream out;
            out << "pqissl::Basic_Connection_Complete()";
            out << " ECONNREFUSED: cert: ";
            out << PeerId();
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

            // Then send unreachable message.
            net_internal_close(sockfd);
            sockfd=-1;
            //reset();
            waiting = WAITING_FAIL_INTERFACE;

            return -1;
        }

        std::ostringstream out;
        out << "Error: Connection Failed UNKNOWN ERROR: " << err;
        out << " - " << socket_errorType(err);
        log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

        net_internal_close(sockfd);
        sockfd=-1;
        //reset(); // which will send Connect Failed,
        return -1;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Basic_Connection_Complete() BAD GETSOCKOPT!");
    waiting = WAITING_FAIL_INTERFACE;

    return -1;
}


int     pqissl::Initiate_SSL_Connection() {
    int err;

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Initiate_SSL_Connection() Checking Basic Connection");

    if (0 >= (err = Basic_Connection_Complete())) {
        return err;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Initiate_SSL_Connection() Basic Connection Okay");

    // setup timeout value.
    ssl_connect_timeout = time(NULL) + PQISSL_SSL_CONNECT_TIMEOUT;

    // Perform SSL magic.
    // library already inited by sslroot().
    SSL *ssl = SSL_new(authMgr->getCTX());
    if (ssl == NULL) {
        log(LOG_WARNING, PQISSLZONE,
            "Failed to initiate SSL system!");
        return -1;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Initiate_SSL_Connection() SSL Connection Okay");

    ssl_connection = ssl;

    net_internal_SSL_set_fd(ssl, sockfd);
    if (err < 1) {
        std::ostringstream out;
        out << "pqissl::Initiate_SSL_Connection() SSL_set_fd failed!";
        out << std::endl;
        printSSLError(ssl, err, SSL_get_error(ssl, err),
                      ERR_get_error(), out);

        log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
        return -1;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Initiate_SSL_Connection() Waiting for SSL Connection");

    waiting = WAITING_SSL_CONNECTION;
    return 1;
}

int     pqissl::SSL_Connection_Complete() {
    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::SSL_Connection_Complete()??? ... Checking");

    if (waiting == WAITING_SSL_AUTHORISE) {
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::SSL_Connection_Complete() Waiting = W_SSL_AUTH");

        return 1;
    }
    if (waiting != WAITING_SSL_CONNECTION) {
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::SSL_Connection_Complete() Still Waiting..");

        return -1;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::SSL_Connection_Complete() Attempting SSL_connect");

    /* if we are in the role of a server then accept,
       otherwise if we're in the role of the client connect */
    int result;

    if (sslmode) {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "--------> Active Connect!");
        result = SSL_connect(ssl_connection);
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "--------> Passive Accept!");
        result = SSL_accept(ssl_connection);
    }

    if (result != 1) {
        int serr = SSL_get_error(ssl_connection, result);
        if ((serr == SSL_ERROR_WANT_READ)
                || (serr == SSL_ERROR_WANT_WRITE)) {
            log(LOG_DEBUG_BASIC, PQISSLZONE,
                "Waiting for SSL handshake!");

            waiting = WAITING_SSL_CONNECTION;
            return 0;
        }

        {
            std::ostringstream out;
            out << "Unable to setup encrypted connection" << std::endl;
            log(LOG_WARNING, PQISSLZONE, out.str().c_str());
        }

        {
            std::ostringstream out;
            out << "Issues with SSL connection (" << result << ")!" << std::endl;
            printSSLError(ssl_connection, result, serr,
                          ERR_get_error(), out);
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
        }

        reset();
        waiting = WAITING_FAIL_INTERFACE;

        return -1;
    }
    // if we get here... success v quickly.

    {
        std::ostringstream out;
        out << "pqissl::SSL_Connection_Complete() Success!: Peer: " << PeerId();
        log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
    }

    waiting = WAITING_SSL_AUTHORISE;
    return 1;
}

int     pqissl::Authorise_SSL_Connection() {
    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Authorise_SSL_Connection()");

    if (time(NULL) > ssl_connect_timeout) {
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::Authorise_SSL_Connection timed out");
        /* as sockfd is valid, this should close it all up */
        reset();
        return 0;
    }

    int err;
    if (0 >= (err = SSL_Connection_Complete())) {
        return err;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE,
        "pqissl::Authorise_SSL_Connection() SSL_Connection_Complete");

    // reset switch.
    waiting = WAITING_NOT;

    {
        // then okay...
        std::ostringstream out;
        out << "pqissl::Authorise_SSL_Connection() Accepting Conn. Peer: " << PeerId();
        log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

        accept(ssl_connection, sockfd, remote_addr);
        return 1;
    }
}

int pqissl::accept(SSL *ssl, int fd, struct sockaddr_in foreign_addr) { // initiate incoming connection.
    //We only need mutex protection if we are not already calling from within the mutex
    bool unlock_when_done = false;
    if (waiting != WAITING_NOT) {
        // outgoing connection in progress.
        // shut that one down and keep the already accepted listening connection.
        pqisslMtx.lock();
        unlock_when_done = true;
        log(LOG_WARNING, PQISSLZONE, "Two connections to same friend in progress - Shutting down outbound and keeping inbound");

        switch (waiting) {
            case WAITING_SOCK_CONNECT:
                log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() STATE = Waiting Sock Connect");
                break;
            case WAITING_SSL_CONNECTION:
                log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() STATE = Waiting SSL Connection");
                break;
            case WAITING_SSL_AUTHORISE:
                log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() STATE = Waiting SSL Authorise");
                break;
            case WAITING_FAIL_INTERFACE:
                log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() STATE = Failed");
                break;
            default:
                log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::accept() STATE = Unknown, Reseting connection");
                reset();
                break;
        }

    }

    /* shutdown existing - in all cases use the new one */
    if ((ssl_connection) && (ssl_connection != ssl)) {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() closing previously existing ssl_connection");
        SSL_shutdown(ssl_connection);
    }

    if ((sockfd > -1) && (sockfd != fd)) {
        QString tolog = "Closing old network socket: ";
        tolog.append(QString::number(sockfd));
        tolog.append(" current socket is: ");
        tolog.append(QString::number(fd));
        log(LOG_WARNING, PQISSLZONE, tolog);

        net_internal_close(sockfd);
    }


    // save ssl + sock.

    ssl_connection = ssl;
    sockfd = fd;

    /* if we connected - then just writing the same over,
     * but if from ssllistener then we need to save the address.
     */
    remote_addr = foreign_addr;

    /* check whether it is on the same LAN */

    peerConnectState details;
    connMgr->getPeerConnectState(authMgr->OwnLibraryMixerId(), details);
    sameLAN = isSameSubnet(&(remote_addr.sin_addr), &(details.localaddr.sin_addr));

    {
        std::ostringstream out;
        out << "pqissl::accept() Successful connection with: " << PeerId();
        out << std::endl;
        out << "\t\tchecking for same LAN";
        out << std::endl;
        out << "\t localaddr: " << inet_ntoa(details.localaddr.sin_addr);
        out << std::endl;
        out << "\t remoteaddr: " << inet_ntoa(remote_addr.sin_addr);
        out << std::endl;
        if (sameLAN) {
            out << "\tSAME LAN - no bandwidth restrictions!";
        } else {
            out << "\tDifferent LANs - bandwidth restrictions!";
        }
        out << std::endl;

        log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
    }

    // establish the ssl details.
    // cipher name.
    int alg;
    int err;

    {
        std::ostringstream out;
        out << "SSL Cipher:" << SSL_get_cipher(ssl) << std::endl;
        out << "SSL Cipher Bits:" << SSL_get_cipher_bits(ssl, &alg);
        out << " - " << alg << std::endl;
        out << "SSL Cipher Version:" << SSL_get_cipher_version(ssl) << std::endl;
        log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());
    }

    // make non-blocking / or check.....
    if ((err = net_internal_fcntl_nonblock(sockfd)) < 0) {
        log(LOG_WARNING, PQISSLZONE, "Error: Cannot make socket NON-Blocking: ");

        active = false;
        waiting = WAITING_FAIL_INTERFACE;
        // failed completely.
        reset();
        if (unlock_when_done) pqisslMtx.unlock();
        return -1;
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() Socket Made Non-Blocking!");
    }

    // we want to continue listening - incase this socket is crap, and they try again.
    //stoplistening();

    active = true;
    waiting = WAITING_NOT;

    // Notify the pqiperson.... (Both Connect/Receive)
    if (parent()) {
        parent() -> notifyEvent(this, NET_CONNECT_SUCCESS);
    }
    if (unlock_when_done) pqisslMtx.unlock();
    std::ostringstream out;
    out << "Completing encrypted connection with " << std::endl;
    out << inet_ntoa(remote_addr.sin_addr) << ":" << ntohs(remote_addr.sin_port);
    log(LOG_WARNING, PQISSLZONE, out.str().c_str());
    return 1;
}

/********** Implementation of BinInterface **************************
 * All the rest of the BinInterface.
 *
 */

int     pqissl::senddata(void *data, int len) {
    int tmppktlen ;

    tmppktlen = SSL_write(ssl_connection, data, len) ;

    if (len != tmppktlen) {
        std::ostringstream out;
        out << "pqissl::senddata()";
        out << " Full Packet Not Sent!" << std::endl;
        out << " -> Expected len(" << len << ") actually sent(";
        out << tmppktlen << ")" << std::endl;

        int err = SSL_get_error(ssl_connection, tmppktlen);
        // incomplete operations - to repeat....
        // handled by the pqistreamer...
        if (err == SSL_ERROR_SYSCALL) {
            out << "SSL_write() SSL_ERROR_SYSCALL";
            out << std::endl;
            out << "Socket Closed Abruptly.... Resetting PQIssl";
            out << std::endl;
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
            reset();
            return -1;
        } else if (err == SSL_ERROR_WANT_WRITE) {
            out << "SSL_write() SSL_ERROR_WANT_WRITE";
            out << std::endl;
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
            return -1;
        } else if (err == SSL_ERROR_WANT_READ) {
            out << "SSL_write() SSL_ERROR_WANT_READ";
            out << std::endl;
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
            return -1;
        } else {
            out << "SSL_write() UNKNOWN ERROR: " << err;
            out << std::endl;
            printSSLError(ssl_connection, tmppktlen, err, ERR_get_error(), out);
            out << std::endl;
            out << "\tResetting!";
            out << std::endl;
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

            reset();
            return -1;
        }
    }
    return tmppktlen;
}

int     pqissl::readdata(void *data, int len) {

    // There is a do, because packets can be splitted into multiple ssl buffers
    // when they are larger than 16384 bytes. Such packets have to be read in
    // multiple slices.
    do {
        int tmppktlen  ;

#ifdef DEBUG_PQISSL
        std::cerr << "calling SSL_read. len=" << len << ", total_len=" << total_len << std::endl ;
#endif
        tmppktlen = SSL_read(ssl_connection, (void *)((unsigned long int)data+(unsigned long int)total_len), len-total_len) ;
#ifdef DEBUG_PQISSL
        std::cerr << "have read " << tmppktlen << " bytes" << std::endl ;
        std::cerr << "data[0] = "
                  << (int)((uint8_t *)data)[0] << " "
                  << (int)((uint8_t *)data)[1] << " "
                  << (int)((uint8_t *)data)[2] << " "
                  << (int)((uint8_t *)data)[3] << " "
                  << (int)((uint8_t *)data)[4] << " "
                  << (int)((uint8_t *)data)[5] << " "
                  << (int)((uint8_t *)data)[6] << " "
                  << (int)((uint8_t *)data)[7] << std::endl ;
#endif

        // Need to catch errors.....
        //
        if (tmppktlen <= 0) { // probably needs a reset.
            std::ostringstream out;

            int error = SSL_get_error(ssl_connection, tmppktlen);
            unsigned long err2 =  ERR_get_error();

            if ((error == SSL_ERROR_ZERO_RETURN) && (err2 == 0)) {
                /* this code will be called when
                 * (1) moretoread -> returns true. +
                 * (2) SSL_read fails.
                 *
                 * There are two ways this can happen:
                 * (1) there is a little data on the socket, but not enough
                 * for a full SSL record, so there legimitately is no error, and the moretoread()
                 * was correct, but the read fails.
                 *
                 * (2) the socket has been closed correctly. this leads to moretoread() -> true,
                 * and ZERO error.... we catch this case by counting how many times
                 * it occurs in a row (cos the other one will not).
                 */

                ++n_read_zero;
                out << "SSL_read() SSL_ERROR_ZERO_RETURN ERROR: Has socket been closed? Attempt: " << n_read_zero;
                out << std::endl;

                if (PQISSL_MAX_READ_ZERO_COUNT < n_read_zero) {
                    out << "Count passed limit, shutting down!";
                    reset();
                }

                log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
                return -1;
            }

            /* the only real error we expect */
            if (error == SSL_ERROR_SYSCALL) {
                out << "Connection to " << inet_ntoa(remote_addr.sin_addr) << " lost";
                /*unsigned long err_error;
                while ((err_error = ERR_get_error()) != 0){
                    out << ERR_error_string(err_error, NULL) << std::endl;
                }*/
                log(LOG_WARNING, PQISSLZONE, out.str().c_str());
                reset();
                return -1;
            } else if (error == SSL_ERROR_WANT_WRITE) {
                out << "SSL_read() SSL_ERROR_WANT_WRITE";
                out << std::endl;
                log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
                return -1;
            } else if (error == SSL_ERROR_WANT_READ) {
                /* SSL_WANT_READ is not a critical error. It's just a sign that
                   the internal SSL buffer is not ready to accept more data. So -1
                   is returned, and the connection will be retried as is on next call of readdata().*/
                out << "SSL_read() SSL_ERROR_WANT_READ";
                out << std::endl;
                log(LOG_DEBUG_ALL, PQISSLZONE, out.str().c_str());
                return -1;
            } else {
                out << "SSL_read() UNKNOWN ERROR: " << error;
                out << std::endl;
                out << "\tResetting!";
                printSSLError(ssl_connection, tmppktlen, error, err2, out);
                log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
                reset();
                return -1;
            }

            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
            //exit(1);
        } else
            total_len+=tmppktlen ;
    } while (total_len < len) ;

#ifdef DEBUG_PQISSL
    std::cerr << "pqissl: have read data of length " << total_len << ", expected is " << len << std::endl ;
#endif

    if (len != total_len) {
        std::ostringstream out;
        out << "pqissl::readdata()";
        out << " Full Packet Not read!" << std::endl;
        out << " -> Expected len(" << len << ") actually read(";
        out << total_len << ")" << std::endl;
        log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
    }
    total_len = 0 ;     // reset the packet pointer as we have finished a packet.
    n_read_zero = 0;
    return len;//tmppktlen;
}


// dummy function currently.
int     pqissl::netstatus() {
    return 1;
}

int     pqissl::isactive() {
    return active;
}

bool    pqissl::moretoread() {
    {
        std::ostringstream out;
        out << "pqissl::moretoread()";
        out << "  polling socket (" << sockfd << ")";
        log(LOG_DEBUG_ALL, PQISSLZONE, out.str().c_str());
    }

    if (sockfd < 0) return false;
    unsigned int validSockfd = sockfd;

    fd_set ReadFDs, WriteFDs, ExceptFDs;
    FD_ZERO(&ReadFDs);
    FD_ZERO(&WriteFDs);
    FD_ZERO(&ExceptFDs);

    FD_SET(validSockfd, &ReadFDs);
    FD_SET(validSockfd, &WriteFDs);
    FD_SET(validSockfd, &ExceptFDs);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (select(sockfd + 1, &ReadFDs, &WriteFDs, &ExceptFDs, &timeout) < 0) {
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::moretoread() Select ERROR!");
        return 0;
    }

    if (FD_ISSET(sockfd, &ExceptFDs)) {
        // error - reset socket.
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::moretoread() Select Exception ERROR!");

        // this is a definite bad socket!.
        // reset.
        reset();
        return 0;
    }

    if (FD_ISSET(sockfd, &WriteFDs)) {
        // write can work.
        log(LOG_DEBUG_ALL, PQISSLZONE,
            "pqissl::moretoread() Can Write!");
    } else {
        // write can work.
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::moretoread() Can *NOT* Write!");
    }

    if (FD_ISSET(sockfd, &ReadFDs)) {
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::moretoread() Data to Read!");
        return 1;
    } else {
        log(LOG_DEBUG_ALL, PQISSLZONE,
            "pqissl::moretoread() No Data to Read!");
        return 0;
    }

}

bool    pqissl::cansend() {
    log(LOG_DEBUG_ALL, PQISSLZONE,
        "pqissl::cansend() polling socket!");

    // Interestingly - This code might be portable....
    if (sockfd < 0) return false;
    unsigned int validSockfd = sockfd;

    fd_set ReadFDs, WriteFDs, ExceptFDs;
    FD_ZERO(&ReadFDs);
    FD_ZERO(&WriteFDs);
    FD_ZERO(&ExceptFDs);

    FD_SET(validSockfd, &ReadFDs);
    FD_SET(validSockfd, &WriteFDs);
    FD_SET(validSockfd, &ExceptFDs);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (select(sockfd + 1, &ReadFDs, &WriteFDs, &ExceptFDs, &timeout) < 0) {
        // select error.
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::cansend() Select Error!");

        return 0;
    }

    if (FD_ISSET(sockfd, &ExceptFDs)) {
        // error - reset socket.
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::cansend() Select Exception!");

        // this is a definite bad socket!.
        // reset.
        reset();
        return 0;
    }

    if (FD_ISSET(sockfd, &WriteFDs)) {
        // write can work.
        log(LOG_DEBUG_ALL, PQISSLZONE,
            "pqissl::cansend() Can Write!");
        return 1;
    } else {
        // write can work.
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::cansend() Can *NOT* Write!");

        return 0;
    }

}

std::string pqissl::gethash() {
    std::string dummyhash;
    return dummyhash;
}

/********** End of Implementation of BinInterface ******************/


