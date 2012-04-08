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
#include "pqi/ownConnectivityManager.h" //To be able to get own local address

#include "util/debug.h"

#include "interface/librarymixer-connect.h"
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
 * (2) bad waiting state.
 *
 * // TCP/or SSL connection already established....
 * (5) pqissl::SSL_Connection_Complete() <- okay -> because we made a TCP connection already.
 * (6) pqissl::accept() <- okay because something went wrong.
 * (7) moretoread()/cansend() <- okay
 *
 */

pqissl::pqissl(PQInterface *parent)
    :NetBinInterface(parent, parent->PeerId(), parent->LibraryMixerId()),
     connectionState(STATE_IDLE), currentlyConnected(false),
     sslmode(PQISSL_ACTIVE), ssl_connection(NULL), mOpenSocket(-1),
     sameLAN(false), failedButRetry(false), errorZeroReturnCount(0),
     mConnectionAttemptTimeout(0), mConnectionAttemptTimeoutAt(0) {
    /* set address to zero */
    sockaddr_clear(&remote_addr);
    isTcpOverUdpConnection = false;
}

pqissl::~pqissl() {
    log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::~pqissl -> destroying pqissl");
    stoplistening();
    reset();
}


/**********************************************************************************
 * NetInterface
 **********************************************************************************/

int pqissl::connect(struct sockaddr_in raddr) {
    remote_addr = raddr;
    remote_addr.sin_family = AF_INET;

    return ConnectAttempt();
}

int pqissl::listen() {
    if (sslListener) return sslListener->addFriendToListenFor(PeerId().c_str(), this);
    return 0;
}

int pqissl::stoplistening() {
    if (sslListener) sslListener->removeFriendToListenFor(PeerId().c_str());
    return 1;
}

void pqissl::reset() {
    bool neededReset = false;

    if (ssl_connection != NULL) {
        SSL_shutdown(ssl_connection);
        neededReset = true;
    }
    if (mOpenSocket > 0) {
        net_internal_close(mOpenSocket);
        neededReset = true;
    }

    if (neededReset) {
        log(LOG_DEBUG_ALERT, PQISSLZONE,
            "pqissl::reset Resetting connection with: " + QString::number(LibraryMixerId()) + " at " + addressToString(&remote_addr));
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE,
            "pqissl::reset Resetting inactive connection with: " + QString::number(LibraryMixerId()));
    }

    currentlyConnected = false;
    mOpenSocket = -1;
    connectionState = STATE_IDLE;
    ssl_connection = NULL;
    sameLAN = false;
    errorZeroReturnCount = 0;
    readSoFar = 0;

    /* We only notify of this event if we actually shut something down. */
    if (neededReset && parent()) {
        if (failedButRetry) {
            failedButRetry = false;
            parent()->notifyEvent(this, NET_CONNECT_FAILED_RETRY, &remote_addr);
        } else {
            parent()->notifyEvent(this, NET_CONNECT_FAILED, &remote_addr);
        }
    }
}

bool pqissl::setConnectionParameter(netParameters type, uint32_t value) {
    if (type == NET_PARAM_CONNECT_TIMEOUT) {
        mConnectionAttemptTimeout = value;
        return true;
    }
    return false;
}

/**********************************************************************************
 * BinInterface
 **********************************************************************************/

int pqissl::tick() {
    if (currentlyConnected) return 1;
    if (connectionState == STATE_IDLE) return 1;

    QMutexLocker stack(&pqisslMtx);
    ConnectAttempt();

    return 1;
}

int pqissl::senddata(void *data, int length) {
    int bytesSent = SSL_write(ssl_connection, data, length);

    if (length != bytesSent) {
        std::ostringstream out;
        out << "pqissl::senddata() Full Packet Not Sent! Expected length (" << length << ") actually sent (" << bytesSent << ")\n";

        int sslErrorCode = SSL_get_error(ssl_connection, bytesSent);
        if (sslErrorCode == SSL_ERROR_SYSCALL) {
            out << "SSL_write() SSL_ERROR_SYSCALL Socket closed abruptly, resetting\n";
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
            reset();
            return -1;
        } else if (sslErrorCode == SSL_ERROR_WANT_WRITE) {
            out << "SSL_write() SSL_ERROR_WANT_WRITE\n";
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
            return -1;
        } else if (sslErrorCode == SSL_ERROR_WANT_READ) {
            out << "SSL_write() SSL_ERROR_WANT_READ\b";
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
            return -1;
        } else {
            out << "SSL_write() UNKNOWN ERROR: " << sslErrorCode;
            out << std::endl;
            printSSLError(ssl_connection, bytesSent, sslErrorCode, ERR_get_error(), out);
            out << std::endl;
            out << "\tResetting!";
            out << std::endl;
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

            reset();
            return -1;
        }
    }
    return bytesSent;
}

int pqissl::readdata(void *data, int length) {
    /* Read in the packet. However, packets can be split into multiple ssl buffers when they are larger than 16384 bytes.
       Therefore, we use a do while loop to read it in multiple slices. */
    do {
        int bytesRead = SSL_read(ssl_connection,
                                 (void *)((unsigned long int)data + (unsigned long int)readSoFar),
                                 length - readSoFar);

        /* bytesRead = 0 means unsuccessful read by SSL_read, < 0 means error */
        if (bytesRead <= 0) {

            int sslErrorCode = SSL_get_error(ssl_connection, bytesRead);
            unsigned long extraErrorInfo =  ERR_get_error();

            /* This code will be called when
             * (1) moretoread -> returns true +
             * (2) SSL_read fails
             *
             * There are two ways this can happen:
             * (1) there is a little data on the socket, but not enough for a full SSL record, so there legimitately is no error, and the moretoread()
             * was correct, but the read fails.
             *
             * (2) the socket has been closed correctly. This leads to moretoread() -> true, and ZERO error. We catch this case by counting how many times
             * it occurs in a row (because the other one will not).
             */
            if ((sslErrorCode == SSL_ERROR_ZERO_RETURN) && (extraErrorInfo == 0)) {
                std::ostringstream out;
                ++errorZeroReturnCount;
                out << "SSL_read() SSL_ERROR_ZERO_RETURN ERROR: Has socket been closed? Attempt: " << errorZeroReturnCount << "\n";

                if (PQISSL_MAX_READ_ZERO_COUNT < errorZeroReturnCount) {
                    out << "Count passed limit, shutting down!\n";
                    reset();
                }

                log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
                return -1;
            }

            /* The only real error we expect */
            if (sslErrorCode == SSL_ERROR_SYSCALL) {
                log(LOG_WARNING, PQISSLZONE, "Connection to " + addressToString(&remote_addr) + " lost");
                reset();
                return -1;
            } else if (sslErrorCode == SSL_ERROR_WANT_WRITE) {
                log(LOG_DEBUG_ALERT, PQISSLZONE, "SSL_read() SSL_ERROR_WANT_WRITE");
                return -1;
            } else if (sslErrorCode == SSL_ERROR_WANT_READ) {
                /* SSL_WANT_READ is not a critical error. It's just a sign that
                   the internal SSL buffer is not ready to accept more data. So -1
                   is returned, and the connection will be retried as is on next call of readdata().*/
                log(LOG_DEBUG_ALL, PQISSLZONE, "SSL_read() SSL_ERROR_WANT_READ");
                return -1;
            } else {
                std::ostringstream out;
                out << "SSL_read() UNKNOWN ERROR: " << sslErrorCode;
                out << std::endl;
                out << "\tResetting!";
                printSSLError(ssl_connection, bytesRead, sslErrorCode, extraErrorInfo, out);
                log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
                reset();
                return -1;
            }
        } else readSoFar += bytesRead;
    } while (readSoFar < length);

    if (length != readSoFar) {
        QString toLog = QString("pqissl::readdata() finished but expected length was ") +
                        QString::number(length) + ", but actually read " +
                        QString::number(readSoFar) + "\n";
        log(LOG_DEBUG_ALERT, PQISSLZONE, toLog);
    }

    readSoFar = 0;
    errorZeroReturnCount = 0;
    return length;
}

bool pqissl::isactive() {return currentlyConnected;}

bool pqissl::moretoread() {
    if (mOpenSocket < 0) return false;
    unsigned int validSockfd = mOpenSocket;

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

    if (select(mOpenSocket + 1, &ReadFDs, &WriteFDs, &ExceptFDs, &timeout) < 0) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::moretoread() Select ERROR!");
        return false;
    }

    if (FD_ISSET(mOpenSocket, &ExceptFDs)) {
        //error - reset socket.
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::moretoread() Select Exception ERROR!");

        reset();
        return false;
    }

    if (FD_ISSET(mOpenSocket, &WriteFDs)) {
        log(LOG_DEBUG_ALL, PQISSLZONE, "pqissl::moretoread() Can Write!");
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::moretoread() Can *NOT* Write!");
    }

    if (FD_ISSET(mOpenSocket, &ReadFDs)) {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::moretoread() Data to Read!");
        return true;
    } else {
        log(LOG_DEBUG_ALL, PQISSLZONE, "pqissl::moretoread() No Data to Read!");
        return false;
    }

}

bool pqissl::cansend() {
    if (mOpenSocket < 0) return false;
    unsigned int validSockfd = mOpenSocket;

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

    if (select(mOpenSocket + 1, &ReadFDs, &WriteFDs, &ExceptFDs, &timeout) < 0) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::cansend() Select Error!");
        return false;
    }

    if (FD_ISSET(mOpenSocket, &ExceptFDs)) {
        //error - reset socket.
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::cansend() Select Exception!");

        reset();
        return 0;
    }

    if (FD_ISSET(mOpenSocket, &WriteFDs)) {
        log(LOG_DEBUG_ALL, PQISSLZONE, "pqissl::cansend() Can Write!");
        return 1;
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::cansend() Can *NOT* Write!");
        return 0;
    }
}

void pqissl::close() {reset();}

/**********************************************************************************
 * Internals of the SSL connection
 **********************************************************************************/

int pqissl::ConnectAttempt() {
    switch (connectionState) {
        case STATE_IDLE:
            log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::ConnectAttempt() STATE = Not Waiting, starting connection");
            sslmode = PQISSL_ACTIVE; /* we're starting this one */
            return Initiate_Connection();
        case STATE_WAITING_FOR_SOCKET_CONNECT:
            log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::ConnectAttempt() STATE = Waiting Sock Connect");
            return Initiate_SSL_Connection();
        case STATE_WAITING_FOR_SSL_CONNECT:
            log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::ConnectAttempt() STATE = Waiting SSL Connection");
            return Authorize_SSL_Connection();
        case STATE_WAITING_FOR_SSL_AUTHORIZE:
            log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::ConnectAttempt() STATE = Waiting SSL Authorise");
            return Authorize_SSL_Connection();
        case STATE_FAILED:
            log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::ConnectAttempt() Failed - Retrying");
            return Failed_Connection();
        default:
            log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::ConnectAttempt() STATE = Unknown - Reset");
            reset();
            break;
    }

    return -1;
}

int pqissl::Initiate_Connection() {
    int err;
    struct sockaddr_in address = remote_addr;

    log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::Initiate_Connection() Attempting Outgoing Connection.");

    if (connectionState != STATE_IDLE) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::Initiate_Connection() Already Attempt in Progress!");
        return -1;
    }

    log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::Initiate_Connection() Opening Socket");

    /* Open socket. */
    int socket = unix_socket(PF_INET, SOCK_STREAM, 0);

    log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::Initiate_Connection() socket = " + QString::number(socket));

    if (socket < 0) {
        log(LOG_WARNING, PQISSLZONE, QString("Failed to open socket! Socket Error:") + socket_errorType(errno).c_str());
        net_internal_close(socket);
        connectionState = STATE_FAILED;
        return -1;
    }

    /* Make socket non-blocking. */
    err = unix_fcntl_nonblock(socket);
    if (err < 0) {
        log(LOG_WARNING, PQISSLZONE, "Error: Cannot make socket NON-Blocking: " + QString::number(err));
        net_internal_close(socket);
        connectionState = STATE_FAILED;
        return -1;
    }

    /* Initiate connection to remote address. */
    log(LOG_DEBUG_ALERT, PQISSLZONE,\
        "pqissl::Initiate_Connection() Connecting to: " + QString::number(LibraryMixerId()) + " via " + addressToString(&address));

    if (address.sin_addr.s_addr == 0) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::Initiate_Connection() Invalid (0.0.0.0) remote address, aborting\n");
        net_internal_close(socket);
        connectionState = STATE_FAILED;
        return -1;
    }

    mConnectionAttemptTimeoutAt = time(NULL) + mConnectionAttemptTimeout;

    if (0 != (err = unix_connect(socket, (struct sockaddr *) &address, sizeof(address)))) {
        std::ostringstream out;
        out << "pqissl::Initiate_Connection() unix_connect returns:";
        out << err << " -> errno: " << errno << " error: ";
        out << socket_errorType(errno) << std::endl;

        if (errno == EINPROGRESS) {
            connectionState = STATE_WAITING_FOR_SOCKET_CONNECT;
            mOpenSocket = socket;

            out << " EINPROGRESS Waiting for Socket Connection";
            log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());

            return 0;
        } else if ((errno == ENETUNREACH) || (errno == ETIMEDOUT)) {
            out << "ENETUNREACHABLE: friend: " << LibraryMixerId();
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

            net_internal_close(socket);

            connectionState = STATE_FAILED;

            return -1;
        } else {
            /* If we get here we failed for some other reason and should abandon this interface.
               Known reasons to get here: EINVAL (bad address) */

            out << "Error: Connection Failed: " << errno;
            out << " - " << socket_errorType(errno) << std::endl;

            net_internal_close(socket);
            connectionState = STATE_FAILED;

            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());

            return -1;
        }
    }

    connectionState = STATE_WAITING_FOR_SOCKET_CONNECT;
    mOpenSocket = socket;

    log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::Initiate_Connection() Waiting for Socket Connect");

    return 1;
}

int pqissl::Basic_Connection_Complete() {
    /* If timed out close out connection. */
    if (time(NULL) > mConnectionAttemptTimeoutAt) {
        std::ostringstream out;
        out << "pqissl::Basic_Connection_Complete() Connection timed out. ";
        out << "Peer: " << LibraryMixerId() << " Period: ";
        out << mConnectionAttemptTimeout;

        log(LOG_DEBUG_BASIC, PQISSLZONE, out.str().c_str());

        reset();
        return -1;
    }

    if (connectionState != STATE_WAITING_FOR_SOCKET_CONNECT) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::Basic_Connection_Complete() Wrong mode");
        return -1;
    }

    if (mOpenSocket < 0) return -1;
    unsigned int validSockfd = mOpenSocket;

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

    int sr = select(mOpenSocket + 1, &ReadFDs, &WriteFDs, &ExceptFDs, &timeout);
    if (sr < 0) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::Basic_Connection_Complete() Select ERROR(1)");

        net_internal_close(mOpenSocket);
        mOpenSocket=-1;

        connectionState = STATE_FAILED;
        return -1;
    }

    if (FD_ISSET(mOpenSocket, &ExceptFDs)) {
        //Error - reset socket.
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::Basic_Connection_Complete() Select ERROR(2)");

        net_internal_close(mOpenSocket);
        mOpenSocket=-1;

        connectionState = STATE_FAILED;
        return -1;
    }

    if (FD_ISSET(mOpenSocket, &WriteFDs)) {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::Basic_Connection_Complete() Can Write!");
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::Basic_Connection_Complete() Not Yet Ready!");
        return 0;
    }

    if (FD_ISSET(mOpenSocket, &ReadFDs)) {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::Basic_Connection_Complete() Can Read!");
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::Basic_Connection_Complete() No Data to Read!");
    }

    int err = 1;
    if (unix_getsockopt_error(mOpenSocket, &err) != 0) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::Basic_Connection_Complete() BAD GETSOCKOPT!");
        connectionState = STATE_FAILED;

        return -1;
    } else {
        if (err == 0) {
            log(LOG_WARNING, PQISSLZONE, "Established outgoing connection to " + addressToString(&remote_addr) + ", initializing encrypted connection");
            return 1;
        }

        if (err == EINPROGRESS) {
            log(LOG_DEBUG_ALERT, PQISSLZONE, QString("pqissl::Basic_Connection_Complete() EINPROGRESS: friend: ") + QString::number(LibraryMixerId()));
            return 0;
        }

        /* Handle the various error states. */
        if ((err == ENETUNREACH) || (err == ETIMEDOUT)) {
            log(LOG_DEBUG_ALERT, PQISSLZONE, QString("pqissl::Basic_Connection_Complete() ENETUNREACH/ETIMEDOUT: friend: ") + QString::number(LibraryMixerId()));
        } else if ((err == EHOSTUNREACH) || (err == EHOSTDOWN)) {
            log(LOG_DEBUG_ALERT, PQISSLZONE, QString("pqissl::Basic_Connection_Complete() EHOSTUNREACH/EHOSTDOWN: friend: ") + QString::number(LibraryMixerId()));
        } else if ((err == ECONNREFUSED)) {
            log(LOG_DEBUG_ALERT, PQISSLZONE, QString("pqissl::Basic_Connection_Complete() ECONNREFUSED: friend: ") + QString::number(LibraryMixerId()));
        } else {
            log(LOG_DEBUG_ALERT, PQISSLZONE, "Error: Connection Failed UNKNOWN ERROR: " + QString::number(err) +
                                             " - " + socket_errorType(err).c_str());
        }
        net_internal_close(mOpenSocket);
        mOpenSocket = -1;

        connectionState = STATE_FAILED;

        return -1;
    }
}

int pqissl::Initiate_SSL_Connection() {
    int err = Basic_Connection_Complete();
    if (err <= 0) return err;

    mSSLConnectionAttemptTimeoutAt = time(NULL) + PQISSL_SSL_CONNECT_TIMEOUT;

    //SSL was already initialized by the authMgr
    SSL *ssl = SSL_new(authMgr->getCTX());
    if (ssl == NULL) {
        log(LOG_WARNING, PQISSLZONE, "Failed to initiate SSL system!");
        return -1;
    }

    ssl_connection = ssl;

    net_internal_SSL_set_fd(ssl, mOpenSocket);

    log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::Initiate_SSL_Connection() Waiting for SSL Connection");

    connectionState = STATE_WAITING_FOR_SSL_CONNECT;
    return 1;
}

int pqissl::SSL_Connection_Complete() {
    /* Check if SSL timeout. */
    if (time(NULL) > mSSLConnectionAttemptTimeoutAt) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::SSL_Connection_Complete timed out");

        reset();
        return -1;
    }

    if (connectionState == STATE_WAITING_FOR_SSL_AUTHORIZE) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::SSL_Connection_Complete() Waiting");
        return 1;
    }
    if (connectionState != STATE_WAITING_FOR_SSL_CONNECT) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::SSL_Connection_Complete() Still Waiting");
        return -1;
    }

    /* If we are in the role of a client (when using TCP) then connect,
       otherwise if we're in the role of the server (when using UDP) then accept.
       Note that TCP server listening is handled by pqissllistener. */
    int result;
    if (sslmode == PQISSL_ACTIVE) {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "--------> Active Connect!");
        result = SSL_connect(ssl_connection);
    } else {
        log(LOG_DEBUG_BASIC, PQISSLZONE, "--------> Passive Accept!");
        result = SSL_accept(ssl_connection);
    }

    if (result == 1) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, QString("pqissl::SSL_Connection_Complete() Success!: Peer: ") + QString::number(LibraryMixerId()));
        connectionState = STATE_WAITING_FOR_SSL_AUTHORIZE;
        return 1;
    } else {
        int sslError = SSL_get_error(ssl_connection, result);
        if ((sslError == SSL_ERROR_WANT_READ) || (sslError == SSL_ERROR_WANT_WRITE)) {
            log(LOG_DEBUG_BASIC, PQISSLZONE, "Waiting for SSL handshake!");
            return 0;
        }

        int error = ERR_get_error();

        bool unrecognizedCertificate = false;
        /* Depending on whether we are considered the SSL server or client, these will be the two errors for an unrecognized certificate. */
        if (ERR_GET_LIB(error) == ERR_LIB_SSL &&
            ERR_GET_FUNC(error) == SSL_F_SSL3_GET_CLIENT_CERTIFICATE &&
            ERR_GET_REASON(error) == SSL_R_NO_CERTIFICATE_RETURNED) {
            unrecognizedCertificate = true;
        }
        if (ERR_GET_LIB(error) == ERR_LIB_SSL &&
            ERR_GET_FUNC(error) == SSL_F_SSL3_GET_SERVER_CERTIFICATE &&
            ERR_GET_REASON(error) == SSL_R_CERTIFICATE_VERIFY_FAILED) {
            unrecognizedCertificate = true;
        }

        bool friendDisconnected = false;
        /* Not seeing this in the OpenSSL documentation,
           but this is what we get when our friend disconnects us due to not recognizing our cert on an ordinary pqissl connection,
           and also sometimes on pqissludp connection. */
        if (ERR_GET_LIB(error) == 0 &&
                         ERR_GET_FUNC(error) == 0 &&
                         ERR_GET_REASON(error) == 0) {
            friendDisconnected = true;
        }
        /* This is what we get when our friend disconnects us due to not recognizing our cert with the way OpenSSL is set up on a pqissludp connection,
           at least part of the time. */
        if (ERR_GET_LIB(error) == ERR_LIB_SSL &&
            ERR_GET_FUNC(error) == SSL_F_SSL3_READ_BYTES &&
            ERR_GET_REASON(error) == SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN) {
            friendDisconnected = true;
        }

        if (unrecognizedCertificate) {
            log(LOG_WARNING, PQISSLZONE, "Connected to friend's address but received an unrecognized encryption key, disconnecting and updating friend list.");
            /* If SSL failed because of an unrecognized encryption certificate, we should update our certificates from LibraryMixer. */
            librarymixerconnect->downloadFriends();
            if (!isTcpOverUdpConnection) {
                log(LOG_WARNING, PQISSLZONE, "Scheduling a quick retry of the connection after updating the friend list");
                failedButRetry = true;
            }
        } else if (friendDisconnected) {
            log(LOG_WARNING, PQISSLZONE, "Friend disconnected before encryption initialization was completed, possibly to update friend list");
            if (!isTcpOverUdpConnection) {
                log(LOG_WARNING, PQISSLZONE, "Scheduling a quick retry of the connection after giving enough time for them to update their friend list");
                failedButRetry = true;
            }
        } else {
            log(LOG_WARNING, PQISSLZONE, "Error establishing encrypted connection, disconnecting");
            std::ostringstream out;
            out << "Issues with SSL connection (mode: " << sslmode << ")!" << std::endl;
            printSSLError(ssl_connection, result, sslError, error, out);
            log(LOG_DEBUG_ALERT, PQISSLZONE, out.str().c_str());
        }

        reset();
        connectionState = STATE_FAILED;

        return -1;
    }
}

int pqissl::Authorize_SSL_Connection() {
    int err = SSL_Connection_Complete();
    if (err <= 0) return -1;

    /* Check to make sure the certificate presented matches the certificate of this pqissl. */
    X509 *peercert = SSL_get_peer_certificate(ssl_connection);

    if (peercert == NULL) {
        pqioutput(LOG_WARNING, PQISSLZONE, "Should not be possible, incoming connection reached pqissl::Authorize_SSL_Connection() without a certificate!");
        reset();
        connectionState = STATE_FAILED;
        return -1;
    }

    unsigned char peercert_id[20];
    if (!authMgr->getCertId(peercert, peercert_id)) return false;
    std::string cert_id = std::string((char *)peercert_id, sizeof(peercert_id));
    X509_free(peercert);

    if (cert_id != PeerId()) {
        pqioutput(LOG_WARNING, PQISSLZONE, "While attempting to connect to " + QString::number(LibraryMixerId()) +
                                           " somehow got " + QString::number(authMgr->findLibraryMixerByCertId(cert_id)) +
                                           " instead!");
        reset();
        connectionState = STATE_FAILED;
        return -1;
    }

    /* We reset the connectionState here in advance for accept.
       This way accept (which is also called from pqissllistener for inbound connections)
       can always take STATE_IDLE to indicate no problems. */
    connectionState = STATE_IDLE;

    log(LOG_DEBUG_ALERT, PQISSLZONE, QString("pqissl::Authorize_SSL_Connection() Accepting Conn. Peer: ") + QString::number(LibraryMixerId()));

    accept(ssl_connection, mOpenSocket, remote_addr);
    return 1;
}

int pqissl::accept(SSL *ssl, int socket, struct sockaddr_in foreign_addr) { // initiate incoming connection.
    /* First check is we have any outbound connections in progress.
       If there are, this means this is an inbound connection.
       We need to shutdown one of them, and as a blanket rule, we will favor the inbound connection that is already
       at accept() over the outbound connection that has not yet reached accept(). */

    /* This signals that there is an outbound connection in progress.
       (Other than one that already reached SSL_Connection_Complete, in which case it has pre-marked itself as STATE_IDLE for us.) */
    if (connectionState != STATE_IDLE) {
        log(LOG_WARNING, PQISSLZONE, "Two connections to same friend in progress - Shutting down outbound and keeping inbound");
        switch (connectionState) {
            case STATE_WAITING_FOR_SOCKET_CONNECT:
                log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() STATE = Waiting Sock Connect");
                break;
            case STATE_WAITING_FOR_SSL_CONNECT:
                log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() STATE = Waiting SSL Connection");
                break;
            case STATE_WAITING_FOR_SSL_AUTHORIZE:
                log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() STATE = Waiting SSL Authorise");
                break;
            case STATE_FAILED:
                log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::accept() STATE = Failed");
                break;
            default:
                log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::accept() STATE = Unknown, Reseting connection");
                reset();
                break;
        }

    }

    /* If we have an existing ssl connection, and it isn't the same one passed in as an argument, shut it down. */
    if ((ssl_connection) && (ssl_connection != ssl)) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "pqissl::accept() closing previously existing ssl_connection");
        SSL_shutdown(ssl_connection);
    }

    /* If we have an existing socket, and it isn't the same one passed in as an argument, shut it down. */
    if ((mOpenSocket > -1) && (mOpenSocket != socket)) {
        log(LOG_DEBUG_ALERT, PQISSLZONE, "Closing old network socket: "+ QString::number(mOpenSocket) + " current socket is: " + QString::number(socket));
        net_internal_close(mOpenSocket);
    }

    ssl_connection = ssl;
    mOpenSocket = socket;

    /* If this is as outbound connection then this is just harmlessly writing the same thing over itself,
       but if this is an inbound connection from pqissllistener then we need to save the address. */
    remote_addr = foreign_addr;


    sameLAN = isSameSubnet(&(remote_addr.sin_addr), &(ownConnectivityManager->getOwnLocalAddress()->sin_addr));

    /* Make socket non-blocking. */
    int err = net_internal_fcntl_nonblock(mOpenSocket);
    if (err < 0) {
        log(LOG_WARNING, PQISSLZONE, "Error: Cannot make socket NON-Blocking");

        currentlyConnected = false;
        connectionState = STATE_FAILED;

        reset();
        return -1;
    }

    /* We don't stop listening in case this socket is bad, and the peer tries again.
       stoplistening(); */

    currentlyConnected = true;
    connectionState = STATE_IDLE;

    /* Notify the ConnectionToFriend. */
    if (parent()) parent()->notifyEvent(this, NET_CONNECT_SUCCESS, &remote_addr);

    log(LOG_WARNING, PQISSLZONE, QString("Completed creating encrypted connection with ") + addressToString(&remote_addr));
    return 1;
}

int pqissl::acceptInbound(SSL *ssl, int fd, struct sockaddr_in foreign_addr) {
    QMutexLocker stack(&pqisslMtx);
    return accept(ssl, fd, foreign_addr);
}

int pqissl::Failed_Connection() {
    log(LOG_DEBUG_BASIC, PQISSLZONE, "pqissl::ConnectAttempt() Failed - Notifying");

    if (parent()) {
        parent()->notifyEvent(this, NET_CONNECT_FAILED, &remote_addr);
    }

    connectionState = STATE_IDLE;

    return 1;
}
