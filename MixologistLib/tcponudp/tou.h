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

#ifndef TOU_C_HEADER_H
#define TOU_C_HEADER_H


/* Get OS-specific definitions for:
 * struct sockaddr, socklen_t, ssize_t */

#ifndef WINDOWS_SYS

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#else
#include <stdint.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

typedef int socklen_t;
#endif

/**********************************************************************************
 * Standard C interface (as Unix-like as possible) for the tou (Tcp On Udp) library.
 **********************************************************************************/

#ifdef  __cplusplus
extern "C" {
#endif

    /* The modification to a single UDP socket means that the structure of the TOU interface must be changed.
     *
     * STUN Procedure:
     * (1) choose our local address. (a universal bind)
     * bool TCP_over_UDP_init(const struct sockaddr *my_addr);
     * (2) query if we have determined our external address.
     * int TCP_over_UDP_read_extaddr(struct sockaddr *ext_addr, socklen_t *addrlen, uint8_t *stable);
     * (3) offer more stunpeers, for external address determination.
     * int TCP_over_UDP_add_stunpeer(const struct sockaddr *ext_addr, socklen_t addrlen, const char *id);
     * (4) repeat (2)+(3) until a valid ext_addr is returned. If stable is false this means we have a symmetric NAT and STUN has failed.
     * (5) if stunkeepalive is enabled, then periodically send out stun packets to maintain external firewall port.
     *
     */

    /* Opens the udp port.
       Returns true on success or if already done. */
    bool TCP_over_UDP_init(const struct sockaddr *my_addr, socklen_t addrlen);

    /* Adds the peer to the list of peers that we will attempt to use as STUN servers. */
    int TCP_over_UDP_add_stunpeer(const struct sockaddr *ext_addr, socklen_t addrlen, const char *cert_id);

    /* Populates the external address (and its length), as well as if it is stable.
       If stable is false, means we have a symmetric NAT, and STUN will be ineffective in finding our external port.
       Can be called repeatedly until address is found.
       Returns 1 on success, 0 on failure, -1 if not yet init. */
    int TCP_over_UDP_read_extaddr(struct sockaddr *ext_addr, socklen_t *addrlen, uint8_t *stable);

    /* Sets whether we should periodically send STUN keep alive packets. */
    int TCP_over_UDP_set_stunkeepalive(bool enabled);

    /* Maintains STUN keep alive. */
    int TCP_over_UDP_tick_stunkeepalive();

    /* Connections are as similar to UNIX as possible
     * (1) create a socket: tou_socket() this reserves a socket id.
     * (2) connect: active: tou_connect() or passive: tou_listenfor().
     * (3) use as a normal socket.
     *
     * connect() now has a conn_period parameter - this is the
     * estimate (in seconds) of how slowly the connection should proceed.
     *
     * tou_bind() is not valid. TCP_over_UDP_init performs this role.
     * tou_listen() is not valid. (must listen for a specific address) use tou_listenfor() instead.
     * tou_accept() can still be used.
     */

    /* creation/connections */
    int tou_socket(int domain, int type, int protocol);
    int tou_bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen);    /* null op now */
    int tou_listen(int sockfd, int backlog);                        /* null op now */
    int tou_connect(int sockfd, const struct sockaddr *serv_addr,
                        socklen_t addrlen, uint32_t conn_period);
    int tou_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

    /* non-standard bonuses */
    int tou_connected(int sockfd);
    int tou_listenfor(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen);



    /* UNIX interface: minimum for the SSL BIO interface */
    ssize_t tou_read(int sockfd, void *buf, size_t count);
    ssize_t tou_write(int sockfd, const void *buf, size_t count);
    int tou_close(int sockfd);

    /* non-standard */
    int tou_errno(int sockfd);
    int tou_clear_error(int sockfd);

    /* check stream */
    int tou_maxread(int sockfd);
    int tou_maxwrite(int sockfd);


#ifdef  __cplusplus
}
#endif
#endif

