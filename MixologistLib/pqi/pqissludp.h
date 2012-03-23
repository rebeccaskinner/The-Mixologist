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

#ifndef MRK_PQI_SSL_UDP_HEADER
#define MRK_PQI_SSL_UDP_HEADER

#include "pqi/pqissl.h"

/*
 * pqissludp
 *
 * Extends an ordinary pqissl to create a pqissl that works over a TCP over UDP connection.
 *
 */

class pqissludp: public pqissl {
public:
    pqissludp(PQInterface *parent);
    virtual ~pqissludp();

    /* Functions declared here are different, all others are identical. */

    /**********************************************************************************
     * NetInterface
     **********************************************************************************/

    /* Begin listening for connections from this friend.
       Returns 1 on success, 0 on not ready, and -1 on error. */
    virtual int listen();

    /* Stops listening for connections from this friend.
       Returns 1 on success or if already not listening, and -1 on error. */
    virtual int stoplistening();

    /* Stops listening for connections from this friend.
       Returns 1 on success or if already not listening, and -1 on error.
       Utilizing this to set the NET_PARAM_CONNECT_PERIOD is essential before attempting connections. */
    virtual bool setConnectionParameter(netParameters type, uint32_t value);

    /**********************************************************************************
     * BinInterface
     **********************************************************************************/

    /* Returns true when there is more data available to be read by the connection. */
    virtual bool moretoread();

    /* Returns true when more data is ready to be sent by the connection. */
    virtual bool cansend();

    /* If a connection is with a friend on the same LAN, then we'll let that connection be excused from bandwidth balancing in pqistreamer.
       TCP over UDP is always through firewalls, so this will always return true. */
    virtual bool bandwidthLimited() {return true;}

protected:
    /**********************************************************************************
     * Internals of the SSL connection
     **********************************************************************************/

    /* Opens a socket, makes it non-blocking, then connects it to the target address.
       Returns 1 on success, 0 if not ready, -1 on errors. */
    virtual int Initiate_Connection();

    /* Completes the basic TCP connection to the target address.
       Returns 1 on success, 0 if not ready, -1 on errors. */
    virtual int Basic_Connection_Complete();

    /* Protected internal functions that are overloaded for TCP over UDP. */
    virtual int net_internal_close(int fd);
    virtual int net_internal_SSL_set_fd(SSL *ssl, int fd);
    virtual int net_internal_fcntl_nonblock(int fd);

private:
    /* Called to initiate a connection. */
    int attach();

    BIO *tou_bio;

    /* Timeout period to try the TCP over UDP connection before giving up. */
    uint32_t mConnectPeriod;
};

#endif // MRK_PQI_SSL_UDP_HEADER
