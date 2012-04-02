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

#include "pqi/pqinotify.h"
#include "pqi/pqissl.h"
#include "pqi/pqissllistener.h"
#include "pqi/pqinetwork.h"

#include <errno.h>
#include <openssl/err.h>

#include "util/debug.h"
#include <sstream>

#include "interface/iface.h" //for librarymixerconnect variable
#include "interface/librarymixer-connect.h"

pqissllistener::pqissllistener(struct sockaddr_in *addr)
    :listenAddress(*addr), listenerActive(false) {

    if (!(authMgr->active())) {
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Encryption failure", "SSL-CTX-CERT-ROOT not initialised!");
        exit(1);
    }

    setuplisten();
    return;
}

int pqissllistener::tick() {
    acceptconnection();
    continueaccepts();
    return 1;
}

int pqissllistener::setListenAddr(struct sockaddr_in addr) {
    listenAddress = addr;
    return 1;
}

int pqissllistener::setuplisten() {
    int err;
    if (listenerActive) return -1;

    listeningSocket = socket(PF_INET, SOCK_STREAM, 0);
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // ie UNIX
    if (listeningSocket < 0) {
        pqioutput(LOG_DEBUG_ALERT, SSL_LISTENER_ZONE, "pqissllistener::setuplisten() Cannot Open Socket!");

        return -1;
    }

    err = fcntl(listeningSocket, F_SETFL, O_NONBLOCK);
    if (err < 0) {
        std::ostringstream out;
        out << "Error: Cannot make socket NON-Blocking: ";
        out << err << std::endl;
        pqioutput(PQL_ERROR, SSL_LISTENER_ZONE, out.str().c_str());

        return -1;
    }

    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#else //WINDOWS_SYS 
    if ((unsigned) listeningSocket == INVALID_SOCKET) {
        std::ostringstream out;
        out << "pqissllistener::setuplisten()";
        out << " Cannot Open Socket!" << std::endl;
        out << "Socket Error:";
        out  << socket_errorType(WSAGetLastError()) << std::endl;
        pqioutput(LOG_DEBUG_ALERT, SSL_LISTENER_ZONE, out.str().c_str());

        return -1;
    }

    // Make nonblocking.
    unsigned long int on = 1;
    if (0 != (err = ioctlsocket(listeningSocket, FIONBIO, &on))) {
        std::ostringstream out;
        out << "pqissllistener::setuplisten()";
        out << "Error: Cannot make socket NON-Blocking: ";
        out << err << std::endl;
        out << "Socket Error:";
        out << socket_errorType(WSAGetLastError()) << std::endl;
        pqioutput(LOG_DEBUG_ALERT, SSL_LISTENER_ZONE, out.str().c_str());

        return -1;
    }
#endif
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/

    // setup listening address.
    listenAddress.sin_family = AF_INET;

    pqioutput(PQL_DEBUG_BASIC, SSL_LISTENER_ZONE, "pqissllistener::setuplisten() Setting up on " + addressToString(&listenAddress));

    if (0 != (err = bind(listeningSocket, (struct sockaddr *) &listenAddress, sizeof(listenAddress)))) {
        std::ostringstream out;
        out << "pqissllistener::setuplisten()";
        out << " Cannot Bind to Local Address!" << std::endl;
        showSocketError(out);
        pqioutput(LOG_DEBUG_ALERT, SSL_LISTENER_ZONE, out.str().c_str());
        getPqiNotify()->AddSysMessage(SYS_ERROR, "Network failure", QString("Unable to open TCP port ") + addressToString(&listenAddress));

        exit(1);
        return -1;
    } else {
        pqioutput(PQL_DEBUG_BASIC, SSL_LISTENER_ZONE, "pqissllistener::setuplisten() Bound to Address.");
    }

    if (0 != (err = listen(listeningSocket, 100))) {
        getPqiNotify()->AddSysMessage(SYS_ERROR,
                                      "Network failure",
                                      QString("Unable to open TCP port ") + addressToString(&listenAddress));

        exit(1);
        return -1;
    } else {
        pqioutput(PQL_DEBUG_BASIC, SSL_LISTENER_ZONE, "pqissllistener::setuplisten() Listening to Socket");
    }
    listenerActive = true;
    return 1;
}

int pqissllistener::resetlisten() {
    pqioutput(PQL_DEBUG_BASIC, SSL_LISTENER_ZONE, QString("Resetting listen with socket ").append(QString::number(listeningSocket)));
    if (listenerActive) {
        /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // ie UNIX
        shutdown(listeningSocket, SHUT_RDWR);
        close(listeningSocket);
#else //WINDOWS_SYS 
        closesocket(listeningSocket);
#endif
        /********************************** WINDOWS/UNIX SPECIFIC PART ******************/

        listenerActive = false;
        return 1;
    }

    return 0;
}

int pqissllistener::acceptconnection() {
    if (!listenerActive) return 0;

    struct sockaddr_in remote_addr;
    socklen_t addrlen = sizeof(remote_addr);
    int fd = accept(listeningSocket, (struct sockaddr *) &remote_addr, &addrlen);
    int err = 0;

    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // ie UNIX
    if (fd < 0) {
        pqioutput(PQL_DEBUG_ALL, SSL_LISTENER_ZONE,
                  "pqissllistener::acceptconnnection() Nothing to Accept!");
        return 0;
    }

    err = fcntl(fd, F_SETFL, O_NONBLOCK);
    if (err < 0) {
        std::ostringstream out;
        out << "pqissllistener::acceptconnection()";
        out << "Error: Cannot make socket NON-Blocking: ";
        out << err << std::endl;
        pqioutput(LOG_DEBUG_ALERT, SSL_LISTENER_ZONE, out.str().c_str());

        close(fd);
        return -1;
    }

    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#else //WINDOWS_SYS 
    if ((unsigned) fd == INVALID_SOCKET) {
        pqioutput(PQL_DEBUG_ALL, SSL_LISTENER_ZONE,
                  "pqissllistener::acceptconnnection() Nothing to Accept!");
        return 0;
    }

    // Make nonblocking.
    unsigned long int on = 1;
    if (0 != (err = ioctlsocket(fd, FIONBIO, &on))) {
        std::ostringstream out;
        out << "pqissllistener::acceptconnection()";
        out << "Error: Cannot make socket NON-Blocking: ";
        out << err << std::endl;
        out << "Socket Error:";
        out << socket_errorType(WSAGetLastError()) << std::endl;
        pqioutput(LOG_DEBUG_ALERT, SSL_LISTENER_ZONE, out.str().c_str());

        closesocket(fd);
        return 0;
    }
#endif
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/

    log(LOG_WARNING, SSL_LISTENER_ZONE,
        QString("Established incoming connection from ") + addressToString(&remote_addr) + ", initializing encrypted connection");

    /* Create the basic SSL object so we can begin accepting an SSL connection. */
    SSL *ssl = SSL_new(authMgr->getCTX());
    SSL_set_fd(ssl, fd);

    return continueSSL(ssl, remote_addr, true);
}

void pqissllistener::continueaccepts() {
    std::map<SSL *, struct sockaddr_in>::iterator it, it_to_delete;

    for (it = incompleteIncomingConnections.begin(); it != incompleteIncomingConnections.end();) {
        if (0 != continueSSL(it->first, it->second, false)) {
            it_to_delete = it++;
            incompleteIncomingConnections.erase(it_to_delete);
        } else {
            it++;
        }
    }
}

int pqissllistener::continueSSL(SSL *ssl, struct sockaddr_in remote_addr, bool newConnection) {
    int fd =  SSL_get_fd(ssl);

    //Make the SSL handshake
    int err = SSL_accept(ssl);

    if (err <= 0) {
        int ssl_err = SSL_get_error(ssl, err);
        int err_err = ERR_get_error();

        if ((ssl_err == SSL_ERROR_WANT_READ) || (ssl_err == SSL_ERROR_WANT_WRITE)) {
            std::ostringstream out;
            out << "pqissllistener::continueSSL() Incoming connection waiting for more data\n";

            if (newConnection) {
                out << "pqissllistener::continueSSL() Incoming connection queued while waiting for more data\n";
                incompleteIncomingConnections[ssl] = remote_addr;
            }

            pqioutput(PQL_DEBUG_BASIC, SSL_LISTENER_ZONE, out.str().c_str());
            return 0;
        }

        /* If SSL failed because of an unrecognized encryption certificate, we should update our certificates from LibraryMixer. */
        if (ERR_GET_LIB(err_err) == ERR_LIB_SSL &&
            ERR_GET_FUNC(err_err) == SSL_F_SSL3_GET_CLIENT_CERTIFICATE &&
            ERR_GET_REASON(err_err) == SSL_R_NO_CERTIFICATE_RETURNED) {
            log(LOG_WARNING, SSL_LISTENER_ZONE, "Incoming connection with unrecognized encryption key, disconnecting and updating friend list.");
            librarymixerconnect->downloadFriends();
        }
        /* Not seeing this in the OpenSSL documentation, but all 0's seems to catch this case. */
        else if (ERR_GET_LIB(err_err) == 0 &&
                 ERR_GET_FUNC(err_err) == 0 &&
                 ERR_GET_REASON(err_err) == 0) {
            log(LOG_WARNING, PQISSLZONE, "Friend disconnected incoming before encryption initialization was completed, possibly to update friend list");
        }
        else {
            std::ostringstream out;
            out << "SSL errors (" << err << ")!" << std::endl;
            printSSLError(ssl, err, ssl_err, err_err, out);
            pqioutput(PQL_DEBUG_ALERT, SSL_LISTENER_ZONE, out.str().c_str());
        }

        SSL_shutdown(ssl);

        /************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // ie UNIX
        shutdown(fd, SHUT_RDWR);
        close(fd);
#else //WINDOWS_SYS 
        closesocket(fd);
#endif
        /************************** WINDOWS/UNIX SPECIFIC PART ******************/

        SSL_free(ssl);

        std::ostringstream out;
        out << "Read Error on the SSL Socket on";
        out << fd;
        out << std::endl;
        out << "Shutting it down!" << std::endl;
        pqioutput(PQL_DEBUG_ALERT, SSL_LISTENER_ZONE, out.str().c_str());

        return -1;
    }

    if (completeConnection(fd, ssl, remote_addr) < 1) {
        pqioutput(LOG_DEBUG_ALERT, SSL_LISTENER_ZONE, "pqissllistener::completeConnection() Failed!");

        SSL_shutdown(ssl);

        /************************** WINDOWS/UNIX SPECIFIC PART ******************/
    #ifndef WINDOWS_SYS // ie UNIX
        shutdown(fd, SHUT_RDWR);
        close(fd);
    #else //WINDOWS_SYS
        closesocket(fd);
    #endif
        /************************** WINDOWS/UNIX SPECIFIC PART ******************/
        SSL_free(ssl);

        pqioutput(PQL_WARNING, SSL_LISTENER_ZONE, "Unable to complete encrypted connection that was incoming from " + addressToString(&remote_addr));

        return -1;
    }

    return 1;
}

int pqissllistener::completeConnection(int fd, SSL *ssl, struct sockaddr_in &remote_addr) {
    //Extract the peer certificate
    X509 *peercert = SSL_get_peer_certificate(ssl);

    if (peercert == NULL) {
        pqioutput(LOG_WARNING, SSL_LISTENER_ZONE, "Should not be possible, incoming connection reached pqissllistener::completeConnection() without a certificate!");
        return -1;
    }

    unsigned char peercert_id[20];
    if (!authMgr->getCertId(peercert, peercert_id)) return false;
    std::string cert_id = std::string((char *)peercert_id, sizeof(peercert_id));
    X509_free(peercert);

    //Find certificate in our knownFriends list
    bool found = false;
    std::map<std::string, pqissl *>::iterator it;
    for (it = knownFriends.begin(); (found!=true) && (it!=knownFriends.end());) {
        if (it->first == cert_id) found = true;
        else it++;
    }

    if (found == false) {
        /* This should be a fairly obscure case.
           When we called SSL_accept, the handshake was already made.
           At that point, the AuthMgr was already queried and verified the certificate was one it recognized.
           Most of the time our list of pqissl should be the same as the list of certs in AuthMgr.
           There is, however, the potential for a narrow window where the cert has been downloaded but the pqissl not yet created. */
        pqioutput(LOG_DEBUG_ALERT, SSL_LISTENER_ZONE, "Incoming connection presented an unrecognized certificate: " + addressToString(&remote_addr));
        return -1;
    }

    pqissl *friendsSSL = it->second;

    /* Even though we are connected now, we don't remove this friend from the list of certificates we are listening from.
       At all times the list of knownFriends is the full list of friends.
       This way a connection that died but we haven't realized it can be replaced by a new incoming connection. */

    friendsSSL->acceptInbound(ssl, fd, remote_addr);
    return 1;
}

int pqissllistener::addFriendToListenFor(std::string id, pqissl *acc) {
    std::map<std::string, pqissl *>::iterator it;

    std::ostringstream out;
    out << "Adding to Cert Listening Addresses Id: " << id << std::endl;

    out << "Current Certs:" << std::endl;    
    for (it = knownFriends.begin(); it != knownFriends.end(); it++) {
        out << it->first << std::endl;
        if (it -> first == id) {
            out << "pqissllistener::addFriendToListenFor() Already listening for Cert\n";

            pqioutput(PQL_DEBUG_ALERT, SSL_LISTENER_ZONE, out.str().c_str());
            return -1;
        }
    }

    pqioutput(PQL_DEBUG_BASIC, SSL_LISTENER_ZONE, out.str().c_str());

    knownFriends[id] = acc;
    return 1;
}

int pqissllistener::removeFriendToListenFor(std::string id) {
    // check if in list.
    std::map<std::string, pqissl *>::iterator it;
    for (it = knownFriends.begin(); it!=knownFriends.end(); it++) {
        if (it->first == id) {
            knownFriends.erase(it);

            pqioutput(PQL_DEBUG_BASIC, SSL_LISTENER_ZONE, "pqissllisten::removeFriendToListenFor() Success!");
            return 1;
        }
    }

    pqioutput(LOG_DEBUG_ALERT, SSL_LISTENER_ZONE, "pqissllistener::removeFriendToListenFor() Failed to Find a Match");

    return -1;
}

