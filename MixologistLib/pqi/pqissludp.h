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

#include <openssl/ssl.h>

// operating system specific network header.
#include "pqi/pqinetwork.h"

#include <string>
#include <map>

#include "pqi/pqissl.h"

/* So pqissludp is the special firewall breaking protocol.
 * This class will implement the basics of streaming
 * ssl over udp using a tcponudp library....
 * and a small extension to ssl.
 */

class pqissludp;
class cert;

/* This provides a NetBinInterface, which is
 * primarily inherited from pqissl.
 * fns declared here are different -> all others are identical.
 */

class pqissludp: public pqissl {
public:
    pqissludp(PQInterface *parent);

    virtual ~pqissludp();

    // NetInterface.
    // listen fns call the udpproxy.
    virtual int listen();
    virtual int stoplistening();
    virtual int tick();
    virtual int reset();

    virtual bool connect_parameter(uint32_t type, uint32_t value);

    // BinInterface.
    // These are reimplemented.
    virtual bool moretoread();
    virtual bool cansend();
    /* UDP always through firewalls -> always bandwidth Limited */
    virtual bool bandwidthLimited() {
        return true;
    }

    // pqissludp specific.
    // called to initiate a connection;
    int     attach();

protected:

    virtual int Initiate_Connection();
    virtual int Basic_Connection_Complete();

    //protected internal fns that are overloaded for udp case.
    virtual int net_internal_close(int fd);
    virtual int net_internal_SSL_set_fd(SSL *ssl, int fd);
    virtual int net_internal_fcntl_nonblock(int fd);

private:

    BIO *tou_bio;  // specific to ssludp.

    //int remote_timeout;
    //int proxy_timeout;

    long listen_checktime;

    uint32_t mConnectPeriod;
};

#endif // MRK_PQI_SSL_UDP_HEADER
