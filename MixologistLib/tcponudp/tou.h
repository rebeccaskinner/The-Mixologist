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
#include <pqi/pqinetwork.h>

#include <stdint.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

typedef int socklen_t;
#endif

/**********************************************************************************
 * Standard C interface (as Unix-like as possible) for the tou (Tcp On Udp) library.
 **********************************************************************************/

class UdpSorter;

#ifdef  __cplusplus
extern "C" {
#endif

    /* Sets the UDP port on which the streams will run.
       Returns true on success or if already done. */
    bool TCP_over_UDP_init(UdpSorter* udpConnection);

    void TCP_over_UDP_shutdown();

    /* Connections are as similar to UNIX as possible
     * (1) create a socket: tou_socket() this reserves a socket id.
     * (2) connect: active: tou_connect() or passive: tou_listenfor().
     * (3) use as a normal socket.
     *
     * connect() now has a conn_period parameter - this is the
     * estimate (in seconds) of how slowly the connection should proceed.
     *
     * tou_bind() is not valid. TCP_over_UDP_init performs this role.
     * tou_accept() can still be used.
     */

    /* creation/connections */
    int tou_socket(int domain, int type, int protocol);
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

