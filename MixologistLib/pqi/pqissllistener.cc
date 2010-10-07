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
#include "pqi/pqissllistener.h"
#include "pqi/pqinetwork.h"

#include <errno.h>
#include <openssl/err.h>

#include "util/debug.h"
#include <sstream>

#include "interface/iface.h" //for librarymixerconnect variable
#include "interface/librarymixer-connect.h"

const int pqissllistenzone = 49787;


/************************ PQI SSL LISTEN BASE ****************************
 *
 * This provides all of the basic connection stuff,
 * and calls completeConnection afterwards...
 *
 */


pqissllistenbase::pqissllistenbase(struct sockaddr_in addr, AuthMgr *am, p3ConnectMgr *cm)
    :laddr(addr), active(false),
     mAuthMgr(am), mConnMgr(cm)

{
    if (!(mAuthMgr -> active())) {
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone,
                  "SSL-CTX-CERT-ROOT not initialised!");

        exit(1);
    }

    setuplisten();
    return;
}

pqissllistenbase::~pqissllistenbase() {
    return;
}

int     pqissllistenbase::tick() {
    status();
    // check listen port.
    acceptconnection();
    return continueaccepts();
}

int     pqissllistenbase::status() {
    std::ostringstream out;
    out << "pqissllistenbase::status(): ";
    out << " Listening on port: " << ntohs(laddr.sin_port) << std::endl;
    pqioutput(PQL_DEBUG_ALL, pqissllistenzone, out.str().c_str());
    return 1;
}



int pqissllistenbase::setuplisten() {
    int err;
    if (active)
        return -1;

    lsock = socket(PF_INET, SOCK_STREAM, 0);
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // ie UNIX
    if (lsock < 0) {
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone,
                  "pqissllistenbase::setuplisten() Cannot Open Socket!");

        return -1;
    }

    err = fcntl(lsock, F_SETFL, O_NONBLOCK);
    if (err < 0) {
        std::ostringstream out;
        out << "Error: Cannot make socket NON-Blocking: ";
        out << err << std::endl;
        pqioutput(PQL_ERROR, pqissllistenzone, out.str().c_str());

        return -1;
    }

    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#else //WINDOWS_SYS 
    if ((unsigned) lsock == INVALID_SOCKET) {
        std::ostringstream out;
        out << "pqissllistenbase::setuplisten()";
        out << " Cannot Open Socket!" << std::endl;
        out << "Socket Error:";
        out  << socket_errorType(WSAGetLastError()) << std::endl;
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone, out.str().c_str());

        return -1;
    }

    // Make nonblocking.
    unsigned long int on = 1;
    if (0 != (err = ioctlsocket(lsock, FIONBIO, &on))) {
        std::ostringstream out;
        out << "pqissllistenbase::setuplisten()";
        out << "Error: Cannot make socket NON-Blocking: ";
        out << err << std::endl;
        out << "Socket Error:";
        out << socket_errorType(WSAGetLastError()) << std::endl;
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone, out.str().c_str());

        return -1;
    }
#endif
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/

    // setup listening address.

    // fill in fconstant bits.

    laddr.sin_family = AF_INET;

    {
        std::ostringstream out;
        out << "pqissllistenbase::setuplisten()";
        out << "\tAddress Family: " << (int) laddr.sin_family;
        out << std::endl;
        out << "\tSetup Address: " << inet_ntoa(laddr.sin_addr);
        out << std::endl;
        out << "\tSetup Port: " << ntohs(laddr.sin_port);

        pqioutput(PQL_DEBUG_BASIC, pqissllistenzone, out.str().c_str());
    }

    if (0 != (err = bind(lsock, (struct sockaddr *) &laddr, sizeof(laddr)))) {
        std::ostringstream out;
        out << "pqissllistenbase::setuplisten()";
        out << " Cannot Bind to Local Address!" << std::endl;
        showSocketError(out);
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone, out.str().c_str());

        exit(1);
        return -1;
    } else {
        pqioutput(PQL_DEBUG_BASIC, pqissllistenzone,
                  "pqissllistenbase::setuplisten() Bound to Address.");
    }

    if (0 != (err = listen(lsock, 100))) {
        std::ostringstream out;
        out << "pqissllistenbase::setuplisten()";
        out << "Error: Cannot Listen to Socket: ";
        out << err << std::endl;
        showSocketError(out);
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone, out.str().c_str());

        exit(1);
        return -1;
    } else {
        pqioutput(PQL_DEBUG_BASIC, pqissllistenzone,
                  "pqissllistenbase::setuplisten() Listening to Socket");
    }
    active = true;
    return 1;
}

int pqissllistenbase::setListenAddr(struct sockaddr_in addr) {
    laddr = addr;
    return 1;
}

int pqissllistenbase::resetlisten() {
    pqioutput(PQL_DEBUG_BASIC, pqissllistenzone, QString("Resetting listen with socket ").append(QString::number(lsock)));
    if (active) {
        // close ports etc.
        /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // ie UNIX
        shutdown(lsock, SHUT_RDWR);
        close(lsock);
#else //WINDOWS_SYS 
        closesocket(lsock);
#endif
        /********************************** WINDOWS/UNIX SPECIFIC PART ******************/

        active = false;
        return 1;
    }

    return 0;
}


int pqissllistenbase::acceptconnection() {
    if (!active)
        return 0;
    // check port for any sockets...
    pqioutput(PQL_DEBUG_ALL, pqissllistenzone, "pqissllistenbase::accepting()");

    // These are local but temp variables...
    // can't be arsed making them all the time.
    struct sockaddr_in remote_addr;
    socklen_t addrlen = sizeof(remote_addr);
    int fd = accept(lsock, (struct sockaddr *) &remote_addr, &addrlen);
    int err = 0;

    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // ie UNIX
    if (fd < 0) {
        pqioutput(PQL_DEBUG_ALL, pqissllistenzone,
                  "pqissllistenbase::acceptconnnection() Nothing to Accept!");
        return 0;
    }

    err = fcntl(fd, F_SETFL, O_NONBLOCK);
    if (err < 0) {
        std::ostringstream out;
        out << "pqissllistenbase::acceptconnection()";
        out << "Error: Cannot make socket NON-Blocking: ";
        out << err << std::endl;
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone, out.str().c_str());

        close(fd);
        return -1;
    }

    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#else //WINDOWS_SYS 
    if ((unsigned) fd == INVALID_SOCKET) {
        pqioutput(PQL_DEBUG_ALL, pqissllistenzone,
                  "pqissllistenbase::acceptconnnection() Nothing to Accept!");
        return 0;
    }

    // Make nonblocking.
    unsigned long int on = 1;
    if (0 != (err = ioctlsocket(fd, FIONBIO, &on))) {
        std::ostringstream out;
        out << "pqissllistenbase::acceptconnection()";
        out << "Error: Cannot make socket NON-Blocking: ";
        out << err << std::endl;
        out << "Socket Error:";
        out << socket_errorType(WSAGetLastError()) << std::endl;
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone, out.str().c_str());

        closesocket(fd);
        return 0;
    }
#endif
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/

    {
        std::ostringstream out;
        out << "Incoming connection from ";
        out << inet_ntoa(remote_addr.sin_addr);
        out << ", initializing encrypted connection";
        pqioutput(PQL_WARNING, pqissllistenzone, out.str().c_str());
    }

    // Negotiate certificates. SSL stylee.
    // Allow negotiations for secure transaction.

    SSL *ssl = SSL_new(mAuthMgr -> getCTX());
    SSL_set_fd(ssl, fd);

    return continueSSL(ssl, remote_addr, true); // continue and save if incomplete.
}

int pqissllistenbase::continueSSL(SSL *ssl, struct sockaddr_in remote_addr, bool addin) {
    // attempt the accept again.
    int fd =  SSL_get_fd(ssl);
    int err = SSL_accept(ssl);
    if (err <= 0) {
        int ssl_err = SSL_get_error(ssl, err);
        int err_err = ERR_get_error();

        if ((ssl_err == SSL_ERROR_WANT_READ) || (ssl_err == SSL_ERROR_WANT_WRITE)) {
            std::ostringstream out;
            out << "pqissllistenbase::continueSSL() ";
            out << " Connection Not Complete!";
            out << std::endl;

            if (addin) {
                out << "pqissllistenbase::continueSSL() ";
                out << "Adding SSL to incoming!";

                // add to incoming queue.
                incoming_ssl[ssl] = remote_addr;
            }

            pqioutput(PQL_DEBUG_BASIC, pqissllistenzone, out.str().c_str());

            // zero means still continuing....
            return 0;
        }

        /* As part of SSL_accept, connections are rejected that present encryption certificates we do not recognize.
           Therefore, we need to update our certs here in case the error was we don't recognize the peer. */
        pqioutput(PQL_WARNING, pqissllistenzone, "Incoming connection with unrecognized encryption key, disconnecting and updating friend list.");
        {
            std::ostringstream out;
            out << "SSL errors (" << err << ")!" << std::endl;
            printSSLError(ssl, err, ssl_err, err_err, out);
            pqioutput(PQL_DEBUG_ALERT, pqissllistenzone, out.str().c_str());
        }
        librarymixerconnect->downloadFriends();

        // otherwise delete ssl connection.
        // kill connection....
        // so it will be removed from cache.
        SSL_shutdown(ssl);

        // close socket???
        /************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // ie UNIX
        shutdown(fd, SHUT_RDWR);
        close(fd);
#else //WINDOWS_SYS 
        closesocket(fd);
#endif
        /************************** WINDOWS/UNIX SPECIFIC PART ******************/
        // free connection.
        SSL_free(ssl);

        std::ostringstream out;
        out << "Read Error on the SSL Socket on";
        out << fd;
        out << std::endl;
        out << "Shutting it down!" << std::endl;
        pqioutput(PQL_DEBUG_ALERT, pqissllistenzone, out.str().c_str());

        // failure -1, pending 0, sucess 1.
        return -1;
    }

    // if it succeeds
    if (0 < completeConnection(fd, ssl, remote_addr)) {
        return 1;
    }

    /* else we shut it down! */
    pqioutput(LOG_DEBUG_ALERT, pqissllistenzone,
              "pqissllistenbase::completeConnection() Failed!");

    // delete ssl connection.
    SSL_shutdown(ssl);

    // close socket???
    /************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS // ie UNIX
    shutdown(fd, SHUT_RDWR);
    close(fd);
#else //WINDOWS_SYS 
    closesocket(fd);
#endif
    /************************** WINDOWS/UNIX SPECIFIC PART ******************/
    // free connection.
    SSL_free(ssl);

    std::ostringstream out;
    out << "Unable to setup encrypted connection that was incoming from " << std::endl;
    out << inet_ntoa(remote_addr.sin_addr);
    pqioutput(PQL_WARNING, pqissllistenzone, out.str().c_str());

    // failure -1, pending 0, sucess 1.
    return -1;
}

int pqissllistenbase::continueaccepts() {

    // for each of the incoming sockets.... call continue.
    std::map<SSL *, struct sockaddr_in>::iterator it, itd;

    for (it = incoming_ssl.begin(); it != incoming_ssl.end();) {
        pqioutput(PQL_DEBUG_BASIC, pqissllistenzone,
                  "pqissllistenbase::continueaccepts() Continuing SSL");
        if (0 != continueSSL(it->first, it->second, false)) {
            pqioutput(PQL_DEBUG_BASIC, pqissllistenzone,
                      "pqissllistenbase::continueaccepts() no more calls to continueaccepts() required");

            /* save and increment -> so we can delete */
            itd = it++;
            incoming_ssl.erase(itd);
        } else {
            it++;
        }
    }
    return 1;
}


/************************ PQI SSL LISTENER ****************************
 *
 * This is the standard pqissl listener interface....
 *
 * this class only allows connections from
 * specific certificates, which are pre specified.
 *
 */

pqissllistener::pqissllistener(struct sockaddr_in addr, AuthMgr *am, p3ConnectMgr *cm)
    :pqissllistenbase(addr, am, cm) {
    return;
}

pqissllistener::~pqissllistener() {
    return;
}

int     pqissllistener::addlistenaddr(std::string id, pqissl *acc) {
    std::map<std::string, pqissl *>::iterator it;

    std::ostringstream out;

    out << "Adding to Cert Listening Addresses Id: " << id << std::endl;
    out << "Current Certs:" << std::endl;
    for (it = listenaddr.begin(); it != listenaddr.end(); it++) {
        out << it->first << std::endl;
        if (it -> first == id) {
            out << "pqissllistener::addlistenaddr()";
            out << "Already listening for Certificate!";
            out << std::endl;

            pqioutput(PQL_DEBUG_ALERT, pqissllistenzone, out.str().c_str());
            return -1;

        }
    }

    pqioutput(PQL_DEBUG_BASIC, pqissllistenzone, out.str().c_str());

    // not there can accept it!
    listenaddr[id] = acc;
    return 1;
}

int pqissllistener::removeListenPort(std::string id) {
    // check if in list.
    std::map<std::string, pqissl *>::iterator it;
    for (it = listenaddr.begin(); it!=listenaddr.end(); it++) {
        if (it->first == id) {
            listenaddr.erase(it);

            pqioutput(PQL_DEBUG_BASIC, pqissllistenzone,
                      "pqissllisten::removeListenPort() Success!");
            return 1;
        }
    }

    pqioutput(LOG_DEBUG_ALERT, pqissllistenzone,
              "pqissllistener::removeListenPort() Failed to Find a Match");

    return -1;
}


int     pqissllistener::status() {
    pqissllistenbase::status();
    // print certificates we are listening for.
    std::map<std::string, pqissl *>::iterator it;

    std::ostringstream out;
    out << "pqissllistener::status(): ";
    out << " Listening (" << ntohs(laddr.sin_port) << ") for Certs:" << std::endl;
    for (it = listenaddr.begin(); it != listenaddr.end(); it++) {
        out << it -> first << std::endl;
    }
    pqioutput(PQL_DEBUG_ALL, pqissllistenzone, out.str().c_str());

    return 1;
}

int pqissllistener::completeConnection(int fd, SSL *ssl, struct sockaddr_in &remote_addr) {

    // Get the Peer Certificate....
    X509 *peercert = SSL_get_peer_certificate(ssl);

    if (peercert == NULL) {
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone,
                  "pqissllistener::completeConnection() Peer Did Not Provide Cert!");

        // failure -1, pending 0, sucess 1.
        // pqissllistenbase will shutdown!
        return -1;
    }

    // Check cert.
    unsigned char peercert_id[20];
    if (!mAuthMgr->getCertId(peercert, peercert_id)) return false;
    std::string cert_id = std::string((char *)peercert_id, sizeof(peercert_id));
    X509_free(peercert);

    bool found = false;
    //Find cert in our listenaddr list
    std::map<std::string, pqissl *>::iterator it;
    for (it = listenaddr.begin(); (found!=true) && (it!=listenaddr.end());) {
        if (it->first == cert_id) found = true;
        else it++;
    }

    if (found == false) {
        /*This should be a fairly obscure case, as most of the time our list of
          pqissl should be the same as the list of certs we have downloaded.
          There is, however, the potential for a narrow window where the cert
          has been downloaded but the pqissl not yet created.*/
        std::ostringstream out;
        out << "No Matching Certificate/Already Connected";
        out << " for Connection:" << inet_ntoa(remote_addr.sin_addr);
        out << std::endl;
        out << "pqissllistenbase: Will shut it down!" << std::endl;
        pqioutput(LOG_DEBUG_ALERT, pqissllistenzone, out.str().c_str());

        return -1;
    }

    pqissl *pqis = it -> second;

    // dont remove from the list of certificates.
    // want to allow a new connection to replace a faulty one!
    // listenaddr.erase(it);

    // timestamp
    // done in sslroot... npc -> lr_timestamp = time(NULL);

    // hand off ssl conection.
    pqis -> accept(ssl, fd, remote_addr);
    return 1;
}




