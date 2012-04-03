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

#include "pqi/pqissludp.h"
#include "pqi/pqinetwork.h"

#include "tcponudp/tou.h"
#include "tcponudp/bio_tou.h"

#include <errno.h>
#include <openssl/err.h>

#include "util/debug.h"

pqissludp::pqissludp(PQInterface *parent)
    :pqissl(NULL, parent), tou_bio(NULL), mConnectPeriod(0) {
    sockaddr_clear(&remote_addr);
    isTcpOverUdpConnection = true;
}

pqissludp::~pqissludp() {
    log(LOG_DEBUG_ALERT, SSL_UDP_ZONE, "pqissludp::~pqissludp -> destroying pqissludp");

    /* Must call reset from here, so that the virtual functions will still work.
     * (Virtual functions called in reset are not called in the base class destructor.
     *
     * This means that reset() will be called a second time in pqissl's destructor, but this should be harmless.
     */

    reset();

    if (tou_bio) {
        BIO_free(tou_bio);
    }
}

// The Address determination is done centrally
int pqissludp::Initiate_Connection() {
    int err;

    mOpenSocket = tou_socket(0,0,0);
    if (mOpenSocket < 0) {
        log(LOG_ALERT, SSL_UDP_ZONE, "Unable to open TCP over UDP socket!");
        connectionState = STATE_FAILED;
        return -1;
    }

    remote_addr.sin_family = AF_INET;

    log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::Initiate_Connection() Attempting Outgoing Connection");

    /* decide if we're active or passive */
    if (LibraryMixerId() < authMgr->OwnLibraryMixerId()) sslmode = PQISSL_ACTIVE;
    else sslmode = PQISSL_PASSIVE;

    if (connectionState != STATE_IDLE) {
        log(LOG_DEBUG_ALERT, SSL_UDP_ZONE, "pqissludp::Initiate_Connection() Already Attempt in Progress!");
        return -1;
    }

    log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::Initiate_Connection() Opening Socket");

    {
        QString out("pqissludp::Initiate_Connection() ");
        out.append("Connecting To: " + QString::number(LibraryMixerId()));
        out.append(" via: " + addressToString(&remote_addr));
        if (sslmode == PQISSL_ACTIVE) {
            out.append(" ACTIVE Connect (SSL_Connect)");
        } else {
            out.append(" PASSIVE Connect (SSL_Accept)");
        }
        log(LOG_DEBUG_ALERT, SSL_UDP_ZONE, out);
    }

    if (remote_addr.sin_addr.s_addr == 0) {
        log(LOG_DEBUG_ALERT, SSL_UDP_ZONE, "pqissludp::Initiate_Connection() Invalid (0.0.0.0) remote address, aborting");
        connectionState = STATE_FAILED;
        reset();
        return -1;
    }

    mConnectionAttemptTimeoutAt = time(NULL) + mConnectionAttemptTimeout;

    /* <===================== UDP Difference *******************/
    if (0 != (err = tou_connect(mOpenSocket, (struct sockaddr *) &remote_addr, sizeof(remote_addr), mConnectPeriod))) {
    /* <===================== UDP Difference *******************/
        int tou_err = tou_errno(mOpenSocket);

        if ((tou_err == EINPROGRESS) || (tou_err == EAGAIN)) {
            log(LOG_DEBUG_ALERT, SSL_UDP_ZONE, "pqissludp::Initiate_Connection() EINPROGRESS Waiting for Socket Connection");

            connectionState = STATE_WAITING_FOR_SOCKET_CONNECT;
            return 0;
        } else if ((tou_err == ENETUNREACH) || (tou_err == ETIMEDOUT)) {
            log(LOG_DEBUG_ALERT, SSL_UDP_ZONE, "pqissludp::Initiate_Connection() ENETUNREACHABLE for friend " +  QString::number(LibraryMixerId()));

            connectionState = STATE_FAILED;
        }

        log(LOG_DEBUG_ALERT, SSL_UDP_ZONE,
            "pqissludp::Initiate_Connection() Error: Connection Failed: " + QString::number(tou_err) +
            " - " + socket_errorType(tou_err).c_str());
        reset();

        return -1;
    } else {
        log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::Init_Connection() connect returned 0");
    }

    connectionState = STATE_WAITING_FOR_SOCKET_CONNECT;

    log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::Initiate_Connection() Waiting for socket connect");

    return 1;
}

int pqissludp::Basic_Connection_Complete() {
    log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::Basic_Connection_Complete()");

    if (time(NULL) > mConnectionAttemptTimeoutAt) {
        log(LOG_DEBUG_ALERT, SSL_UDP_ZONE,
            QString("pqissludp::Basic_Connection_Complete() Connection Timed Out.") +
            " Peer: " + QString::number(LibraryMixerId()) +
            " Period: " + QString::number(mConnectionAttemptTimeout));

        /* as mOpenSocket is valid, this should close it all up */
        reset();

        return -1;
    }

    if (connectionState != STATE_WAITING_FOR_SOCKET_CONNECT) {
        log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::Basic_Connection_Complete() Wrong Mode");
        return -1;
    }

    int err;
    if (0 != (err = tou_errno(mOpenSocket))) {
        if (err == EINPROGRESS) {
            log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::Basic_Connection_Complete() EINPROGRESS: friend " + QString::number(LibraryMixerId()));
        } else if ((err == ENETUNREACH) || (err == ETIMEDOUT)) {
            log(LOG_DEBUG_ALERT, SSL_UDP_ZONE, "pqissludp::Basic_Connection_Complete() ENETUNREACH/ETIMEDOUT: friend " + QString::number(LibraryMixerId()));

            reset();

            connectionState = STATE_FAILED;

            return -1;
        }
    }

    /* <===================== UDP Difference *******************/
    if (tou_connected(mOpenSocket)) {
    /* <===================== UDP Difference *******************/
        log(LOG_WARNING, SSL_UDP_ZONE, "Established TCP over UDP connection to " + addressToString(&remote_addr) + ", initializing encrypted connection");
        return 1;
    } else {
        log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::Basic_Connection_Complete() Not Yet Ready!");
        return 0;
    }

    return -1;
}


int pqissludp::net_internal_close(int fd) {
    log(LOG_DEBUG_ALERT, SSL_UDP_ZONE, "pqissludp::net_internal_close() -> tou_close()");
    return tou_close(fd);
}

int pqissludp::net_internal_SSL_set_fd(SSL *ssl, int fd) {
    log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::net_internal_SSL_set_fd()");

    /* create the bio's */
    tou_bio = BIO_new(BIO_s_tou_socket());

    /* attach the fd's to the BIO's */
    BIO_set_fd(tou_bio, fd, BIO_NOCLOSE);
    SSL_set_bio(ssl, tou_bio, tou_bio);
    return 1;
}

int pqissludp::net_internal_fcntl_nonblock(int) {
    log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::net_internal_fcntl_nonblock()");
    return 0;
}

int pqissludp::listen() {
    log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::listen() (NULLOP)");
    return 1; //udpproxy->listen();
}

int pqissludp::stoplistening() {
    log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::stoplistening() (NULLOP)");
    return 1; //udpproxy->stoplistening();
}


bool pqissludp::setConnectionParameter(netParameters type, uint32_t value) {
    if (type == NET_PARAM_CONNECT_PERIOD) {
        log(LOG_DEBUG_ALERT, SSL_UDP_ZONE,
            "pqissludp::setConnectionParameter() friend " + QString::number(LibraryMixerId()) +
            " PERIOD: " + QString::number(value));

        mConnectPeriod = value;
        return true;
    }
    return pqissl::setConnectionParameter(type, value);
}

bool pqissludp::moretoread() {
    log(LOG_DEBUG_ALL, SSL_UDP_ZONE, "pqissludp::moretoread() polling socket (" + QString::number(mOpenSocket) + ")");

    /* <===================== UDP Difference *******************/
    if (tou_maxread(mOpenSocket)) {
    /* <===================== UDP Difference *******************/
        log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::moretoread() Data to read");
        return 1;
    }

    /* Check the error */
    log(LOG_DEBUG_ALL, SSL_UDP_ZONE, "pqissludp::moretoread() No Data to read!");

    int err;
    if (0 != (err = tou_errno(mOpenSocket))) {
        if ((err == EAGAIN) || (err == EINPROGRESS)) {
            log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::moretoread() EAGAIN/EINPROGRESS: friend " + QString::number(LibraryMixerId()));
            return 0;
        } else if ((err == ENETUNREACH) || (err == ETIMEDOUT)) {
            log(LOG_WARNING, SSL_UDP_ZONE, "pqissludp::moretoread() ENETUNREACH/ETIMEDOUT: friend " + QString::number(LibraryMixerId()));
        } else if (err == EBADF) {
            log(LOG_WARNING, SSL_UDP_ZONE, "pqissludp::moretoread() EBADF: friend " + QString::number(LibraryMixerId()));
        } else {
            log(LOG_WARNING, SSL_UDP_ZONE, "pqissludp::moretoread() Unknown ERROR: " + QString::number(err) + ": friend " + QString::number(LibraryMixerId()));
        }

        reset();
        return 0;
    }

    log(LOG_DEBUG_BASIC, SSL_UDP_ZONE, "pqissludp::moretoread() No Data + No Error (really nothing)");

    return 0;
}

bool pqissludp::cansend() {
    log(LOG_DEBUG_ALL, SSL_UDP_ZONE, "pqissludp::cansend() polling socket!");

    /* <===================== UDP Difference *******************/
    return (0 < tou_maxwrite(mOpenSocket));
    /* <===================== UDP Difference *******************/

}
