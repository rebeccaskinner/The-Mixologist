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

#include "tou.h"

#include <stdlib.h>
#include <string.h>

#include "udplayer.h"
#include "tcpstream.h"
#include "tcponudp/tou_net.h"

#include <vector>
#include <iostream>

#include <errno.h>
#include <time.h>

struct TcpOnUdp_t {
    int tou_fd;
    int lasterrno;
    TcpStream *tcp;
    bool idle;
};

typedef struct TcpOnUdp_t TcpOnUdp;

static std::vector<TcpOnUdp *> tou_streams;

/* If we have already called TCP_over_UDP_init. */
static bool touInitDone = false;

/* The UdpSorter that is our underlying network transport. */
static UdpSorter *udpSorter = NULL;

static int tou_tick_all();

bool TCP_over_UDP_init(const struct sockaddr *my_addr, socklen_t) {
    if (touInitDone) return true;

    /* Initialize with size 5. */
    tou_streams.resize(5);

    /* Initialize the underlying network transport. */
    udpSorter = new UdpSorter( *((struct sockaddr_in *) my_addr));

    /* check the bind succeeded */
    if (!(udpSorter->okay())) {
        delete (udpSorter);
        udpSorter = NULL;
        return false;
    }

    touInitDone = true;
    return true;
}

int TCP_over_UDP_add_stunpeer(const struct sockaddr *my_addr, socklen_t, const char *id) {
    if (!touInitDone) return -1;

    udpSorter->addStunPeer(*(struct sockaddr_in *) my_addr, id);
    return 0;
}

int TCP_over_UDP_set_stunkeepalive(bool enabled) {
    if (!touInitDone) return -1;

    udpSorter->setStunKeepAlive(enabled);
    return 1;
}

int TCP_over_UDP_tick_stunkeepalive() {
    if (!touInitDone) return -1;

    udpSorter->tick();
    return 1;
}

int TCP_over_UDP_read_extaddr(struct sockaddr *ext_addr, socklen_t *, uint8_t *stable) {
    if (!touInitDone) return -1;

    if (udpSorter->readExternalAddress(*(struct sockaddr_in *) ext_addr, *stable)) return 1;

    return 0;
}


/*  open - which does nothing */
int tou_socket(int /*domain*/, int /*type*/, int /*protocol*/) {
    if (!touInitDone) {
        return -1;
    }

    for (unsigned int i = 1; i < tou_streams.size(); i++) {
        if (tou_streams[i] == NULL) {
            tou_streams[i] = new TcpOnUdp();
            tou_streams[i] -> tou_fd = i;
            tou_streams[i] -> tcp = NULL;
            return i;
        }
    }

    TcpOnUdp *tou = new TcpOnUdp();

    tou_streams.push_back(tou);

    if (tou == tou_streams[tou_streams.size() -1]) {
        tou -> tou_fd = tou_streams.size() -1;
        tou -> tcp = NULL;
        return tou->tou_fd;
    }

    tou -> lasterrno = EUSERS;

    return -1;
}

/*  bind - opens the udp port */
int tou_bind(int sockfd, const struct sockaddr *,
                 socklen_t) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];

    /* this now always returns an error! */
    tous -> lasterrno = EADDRINUSE;
    return -1;
}

/*  records peers address, and sends syn pkt
 *      the timeout is very slow initially - to give
 *      the peer a chance to startup
 *
 *      - like a tcp/ip connection, the connect
 *      will return -1 EAGAIN, until connection complete.
 *      - always non blocking.
 */
int tou_connect(int sockfd, const struct sockaddr *serv_addr,
                    socklen_t addrlen, uint32_t conn_period) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];


    if (addrlen != sizeof(struct sockaddr_in)) {
        tous -> lasterrno = EINVAL;
        return -1;
    }

    /* create a TCP stream to connect with. */
    if (!tous->tcp) {
        tous->tcp = new TcpStream(udpSorter);
        udpSorter->addUdpPeer(tous->tcp,
                         *((const struct sockaddr_in *) serv_addr));
    }

    tous->tcp->connect(*(const struct sockaddr_in *) serv_addr, conn_period);
    tous->tcp->tick();
    tou_tick_all();
    if (tous->tcp->isConnected()) {
        return 0;
    }

    tous -> lasterrno = EINPROGRESS;
    return -1;
}

int tou_listenfor(int sockfd, const struct sockaddr *serv_addr,
                      socklen_t addrlen) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];

    if (addrlen != sizeof(struct sockaddr_in)) {
        tous -> lasterrno = EINVAL;
        return -1;
    }

    /* create a TCP stream to connect with. */
    if (!tous->tcp) {
        tous->tcp = new TcpStream(udpSorter);
        udpSorter->addUdpPeer(tous->tcp,
                         *((const struct sockaddr_in *) serv_addr));
    }

    tous->tcp->listenfor(*((struct sockaddr_in *) serv_addr));
    tous->tcp->tick();
    tou_tick_all();

    return 0;
}

int tou_listen(int, int) {
    tou_tick_all();
    return 1;
}


/* slightly different - returns sockfd on connection */
int tou_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];

    if (*addrlen != sizeof(struct sockaddr_in)) {
        tous -> lasterrno = EINVAL;
        return -1;
    }

    //tous->tcp->connect();
    tous->tcp->tick();
    tou_tick_all();
    if (tous->tcp->isConnected()) {
        // should get remote address
        tous->tcp->getRemoteAddress(*((struct sockaddr_in *) addr));
        return sockfd;
    }

    tous -> lasterrno = EAGAIN;
    return -1;
}


int tou_connected(int sockfd) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];

    tous->tcp->tick();
    tou_tick_all();

    return (tous->tcp->TcpState() == 4);
}


/*  standard  stream read/write  non-blocking of course
 */

ssize_t tou_read(int sockfd, void *buf, size_t count) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];

    tous->tcp->tick();
    tou_tick_all();

    int err = tous->tcp->read((char *) buf, count);
    if (err < 0) {
        tous->lasterrno = tous->tcp->TcpErrorState();
        return -1;
    }
    return err;
}

ssize_t tou_write(int sockfd, const void *buf, size_t count) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];


    int err = tous->tcp->write((char *) buf, count);
    if (err < 0) {
        tous->lasterrno = tous->tcp->TcpErrorState();
        tous->tcp->tick();
        tou_tick_all();
        return -1;
    }
    tous->tcp->tick();
    tou_tick_all();
    return err;
}

/* check stream */
int tou_maxread(int sockfd) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];
    tous->tcp->tick();
    tou_tick_all();

    int ret = tous->tcp->read_pending();
    if (ret < 0) {
        tous->lasterrno = tous->tcp->TcpErrorState();
        return 0; // error detected next time.
    }
    return ret;
}

int tou_maxwrite(int sockfd) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];
    tous->tcp->tick();
    tou_tick_all();

    int ret = tous->tcp->write_allowed();
    if (ret < 0) {
        tous->lasterrno = tous->tcp->TcpErrorState();
        return 0; // error detected next time?
    }
    return ret;
}


/*  close down the tcp over udp connection */
int tou_close(int sockfd) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];

    tou_tick_all();

    if (tous->tcp) {
        tous->tcp->tick();

        /* shut it down */
        tous->tcp->close();
        udpSorter->removeUdpPeer(tous->tcp);
        delete tous->tcp;
    }

    delete tous;
    tou_streams[sockfd] = NULL;
    return 1;
}

/*  get an error number */
int tou_errno(int sockfd) {
    if (!udpSorter) {
        return ENOTSOCK;
    }
    if (tou_streams[sockfd] == NULL) {
        return ENOTSOCK;
    }
    TcpOnUdp *tous = tou_streams[sockfd];
    return tous->lasterrno;
}

int tou_clear_error(int sockfd) {
    if (tou_streams[sockfd] == NULL) {
        return -1;
    }
    TcpOnUdp *tous = tou_streams[sockfd];
    tous->lasterrno = 0;
    return 0;
}

/*  unfortuately the library needs to be ticked. (not running a thread)
 *  you can put it in a thread!
 */

/*
 * Some helper functions for stuff.
 *
 */

static int  tou_passall();
static int  tou_active_rw();

static int nextActiveCycle;
static int nextIdleCheck;
static const int kActiveCycleStep = 1;
static const int kIdleCheckStep = 5;

static int  tou_tick_all() {
    tou_passall();
    return 1;

    /* check timer */
    int ts = time(NULL);
    if (ts > nextActiveCycle) {
        tou_active_rw();
        nextActiveCycle += kActiveCycleStep;
    }
    if (ts > nextIdleCheck) {
        tou_passall();
        nextIdleCheck += kIdleCheckStep;
    }
    return 0;
}


static int  tou_passall() {
    /* iterate through all and clean up old sockets.
     * check if idle are still idle.
     */
    std::vector<TcpOnUdp *>::iterator it;
    for (it = tou_streams.begin(); it != tou_streams.end(); it++) {
        if ((*it) && ((*it)->tcp)) {
            (*it)->tcp->tick();
        }
    }
    return 1;
}

static int  tou_active_rw() {
    /* iterate through actives and tick
     */
    return 1;
}


