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

#ifndef TOU_UNIVERSAL_NETWORK_HEADER
#define TOU_UNIVERSAL_NETWORK_HEADER

/* Some Types need to be defined before the interface can be declared */

#include "pqi/pqinetwork.h"

/* C Interface */
#ifdef  __cplusplus
extern "C" {
#endif

    /**********************************************************************************
     * This is the lowest level within the Mixologist for packets that are sent on this interface.
     *
     * It simply provides a (unix-like) universal networking layer that functions cross-platform,
     * passing off the data directly to external OS functions for sending and receiving.
     * It is a C (not C++) interface.
     **********************************************************************************/

    /* Returns the error number from other function calls. */
    int tounet_errno();

    /* Initiates the use of sockets.
       Necessitated by Windows. */
    int tounet_init();

    /* Closes the specified socket. */
    int tounet_close(int sockfd);

    /* Creates a new socket, and return the sockfd, or -1 on failure with errno updated. */
    int tounet_socket(int domain, int type, int protocol);

    /* Binds a listener onto the socket. Returns 0 on success, or -1 on failure with errno updated. */
    int tounet_bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen);

    /* The only supported use of this function is to set a socket to non-blocking. The syntax for this would be:
       tounet_fnctl(sockfd, F_SETFL, O_NONBLOCK)
       Returns -1 on error. */
    int tounet_fcntl(int sockfd, int cmd, long arg);

    /* The only supported use of this function is to set the TTL. The syntax for this would be:
       tounet_setsockopt(sockfd, IPPROTO_IP, IP_TTL, &optval, optlen) */
    int tounet_setsockopt(int sockfd, int level,  int  optname, const  void  *optval, socklen_t optlen);

    /* Retrieves a message on the socket.
       Assuming the socket is created using tounet_fcntl set to non-blocking, returns -1 if no message is available,
       with errno set to EAGAIN or EWOULDBLOCK. Otherwise, returns the amount of data retrieved. */
    ssize_t tounet_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);

    /* Sends the specified data on the socket to the address.
       Returns bytes sent or -1 on error. */
    ssize_t tounet_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);

    /* Check if we can modify the TTL on a UDP packet. */
    int tounet_checkTTL(int sockfd);

    /* Extra stuff to declare for windows error handling (mimics unix errno) */
#ifdef WINDOWS_SYS

    //Some Network functions that are missing from windows.
    //in_addr_t inet_netof(struct in_addr addr);
    //in_addr_t inet_network(char *inet_name);
    //int inet_aton(const char *name, struct in_addr *addr);

    // definitions for fcntl (NON_BLOCK) (random?)
#define F_SETFL     0x1010
#define O_NONBLOCK  0x0100

#include "tou_errno.h"

    int tounet_w2u_errno(int error);

    /* also put the sleep commands in here (where else to go)
     * ms uses millisecs.
     * void Sleep(int ms);
     */
    void sleep(int sec);
    void usleep(int usec);

#endif

#ifdef  __cplusplus
} /* C Interface */
#endif

#endif /* TOU_UNIVERSAL_NETWORK_HEADER */
