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
Specific implementation of a pqilistener that utilizes SSL.
 */

class pqissl;

class pqissllistenbase: public pqilistener {
public:


    pqissllistenbase(struct sockaddr_in addr, AuthMgr *am, p3ConnectMgr *cm);
    virtual ~pqissllistenbase();

    /*************************************/
    /*       LISTENER INTERFACE         **/

    virtual int     tick();
    //Prints to logfile
    virtual int     status();
    virtual int     setListenAddr(struct sockaddr_in addr);
    virtual int setuplisten();
    virtual int     resetlisten();

    /*************************************/

    int acceptconnection();
    int continueaccepts();
    int continueSSL(SSL *ssl, struct sockaddr_in remote_addr, bool);


    virtual int completeConnection(int sockfd, SSL *in_connection, struct sockaddr_in &raddr) = 0;

protected:

    struct sockaddr_in laddr;

private:

    bool active;
    int lsock;

    std::map<SSL *, struct sockaddr_in> incoming_ssl;

protected:

    AuthMgr *mAuthMgr;

    p3ConnectMgr *mConnMgr;

};


class pqissllistener: public pqissllistenbase {
public:

    pqissllistener(struct sockaddr_in addr, AuthMgr *am, p3ConnectMgr *cm);
    virtual ~pqissllistener();

    //Adds a pqissl to listenaddr.  Called by pqissl.
    int     addlistenaddr(std::string id, pqissl *acc);
    //Removes a pqissl from listenaddr.  Called by pqissl.
    int removeListenPort(std::string id);

    virtual int     status();

    //Gets the certificate from ssl and checks it.  If everything is okay, attempts to find it
    //in listenaddr and call pqissl's accept function.
    virtual int completeConnection(int fd, SSL *ssl, struct sockaddr_in &remote_addr);

private:
    //A map of certificates that we're accepting connections from and the pqissl to handle them.
    std::map<std::string, pqissl *> listenaddr;
};


#endif // MRK_PQI_SSL_LISTEN_HEADER
