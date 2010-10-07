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

//Used for variable waiting to indicate state of connection
#define WAITING_NOT            0 //Uninitialized
#define WAITING_DELAY          1 //Has been set by Init_Or_Delay_Connection()
#define WAITING_SOCK_CONNECT   2
#define WAITING_SSL_CONNECTION 3
#define WAITING_SSL_AUTHORISE  4
#define WAITING_FAIL_INTERFACE 5

#define PQISSL_PASSIVE  0x00
#define PQISSL_ACTIVE   0x01

const int PQISSL_LOCAL_FLAG = 0x01;
const int PQISSL_REMOTE_FLAG = 0x02;
const int PQISSL_DNS_FLAG = 0x04;

/* not sure about the value? */
const int PQISSL_UDP_FLAG = 0x02;


/***************************** pqi Net SSL Interface *********************************
 * This provides the base SSL interface class,
 * and handles most of the required functionality.
 *
 * there are a series of small fn's that can be overloaded
 * to provide alternative behaviour....
 *
 * Classes expected to inherit from this are:
 *
 * pqissllistener   -> pqissllistener  (tcp only)
 *          -> pqixpgplistener (tcp only)
 *
 * pqissl       -> pqissltcp
 *          -> pqissludp
 *          -> pqixpgptcp
 *          -> pqixpgpudp
 *
 */

class pqissl;
class cert;

class pqissllistener;

class pqissl: public NetBinInterface {
public:
    pqissl(pqissllistener *l, PQInterface *parent,
           AuthMgr *am, p3ConnectMgr *cm);
    virtual ~pqissl();

    // NetInterface
    virtual int connect(struct sockaddr_in raddr);
    virtual int listen();
    virtual int stoplistening();
    virtual int reset();
    virtual int disconnect();

    virtual bool connect_parameter(uint32_t type, uint32_t value);

    // BinInterface
    virtual int tick();
    virtual int     status();

    virtual int senddata(void *, int);
    virtual int readdata(void *, int);
    virtual int netstatus();
    virtual int isactive();
    virtual bool moretoread();
    virtual bool cansend();

    virtual int close(); /* BinInterface version of reset() */
    virtual std::string gethash(); /* not used here */
    virtual bool bandwidthLimited() {
        return true ;    // replace by !sameLAN to avoid bandwidth limiting on lAN
    }

protected:
    // A little bit of information to describe
    // the SSL state, this is needed
    // to allow full Non-Blocking Connect behaviour.

    //Variable that indicates state of the connection.
    int     waiting;

    //This loops through the following functions based on the variable waiting to complete a connection.
    int     ConnectAttempt();

    virtual int Failed_Connection();

    //If variable waiting indicates connection WAITING_NOT (uninitialized),
    //sets it to WAITING_DELAY (initialized).  If mConnectDelay is 0, calls
    //Initiate_Connection(), otherwise sets mConnectTS to the appropriate
    //amount of delay.
    //If waiting is already WAITING_DELAY, then if mConnectTS delay has been
    //met, calls Initiate_Connect().
    int Init_Or_Delay_Connection();

    /*These two fns are overloaded for udp/etc connections.
      Opens a socket, makes it non-blocking, then calls pqi_network::unix_connect which calls some external connect
      If it's in progress, sets waiting to WAITING_SOCK_CONNECT,
      if it fails sets waiting to WAITING_FAIL_INTERFACE and closes the socket */
    virtual int Initiate_Connection();
    virtual int Basic_Connection_Complete();

    // These should be identical for all cases,
    // differences are achieved via the net_internal_* fns.
    int Initiate_SSL_Connection();
    int SSL_Connection_Complete();
    int Authorise_SSL_Connection();

public:

    /* Completion of the SSL connection,
     * this is public, so it can be called by
     * the listener (should make friends??)
     */

    int accept(SSL *ssl, int fd, struct sockaddr_in foreign_addr);

protected:

    //protected internal fns that are overloaded for udp case.
    virtual int net_internal_close(int fd_to_close) {
        return unix_close(fd_to_close);
    }
    virtual int net_internal_SSL_set_fd(SSL *ssl, int fd) {
        return SSL_set_fd(ssl, fd);
    }
    virtual int net_internal_fcntl_nonblock(int fd) {
        return unix_fcntl_nonblock(fd);
    }


    /* data */
    bool active;
    bool certvalid;

    // addition for udp (tcp version == ACTIVE).
    int sslmode;

    SSL *ssl_connection;
    int sockfd;

    pqissllistener *pqil;
    struct sockaddr_in remote_addr;

    void *readpkt;
    int pktlen;
    int total_len ; // saves the reading state accross successive calls.

    int attempt_ts;

    // Some flags to indicate
    // the status of the various interfaces
    // (local), (server)
    unsigned int net_attempt;
    unsigned int net_failure;
    unsigned int net_unreachable;

    bool sameLAN; /* flag use to allow high-speed transfers */

    int n_read_zero; /* a counter to determine if the connection is really dead */

    int ssl_connect_timeout; /* timeout to ensure that we don't get stuck (can happen on udp!) */

    uint32_t mConnectDelay;
    time_t   mConnectTS;
    uint32_t mConnectTimeout;
    time_t   mTimeoutTS;


    AuthMgr *mAuthMgr;

    p3ConnectMgr *mConnMgr;

private:
    // ssl only fns.
    int connectInterface(sockaddr_in &);

    MixMutex pqisslMtx; /* accept can be called from a separate thread, so we need mutex protection */

};




#endif // MRK_PQI_SSL_HEADER
