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


#include "tou_net.h"
#include "util/debug.h"

const int tounetzone = 58302;

#ifndef WINDOWS_SYS

/* Unix Version is easy -> just call the unix fn */

#include <unistd.h> /* for close definition */

int tounet_errno() {
    return errno;
}

int tounet_init() {
    return 0;
}

/* check if we can modify the TTL on a UDP packet */
int tounet_checkTTL(int) {
    return 1;
}

int tounet_close(int sockfd) {
    return close(sockfd);
}

int tounet_socket(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

int tounet_bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen) {
    return bind(sockfd, my_addr, addrlen);
}

int tounet_fcntl(int sockfd, int cmd, long arg) {
    return fcntl(sockfd, cmd, arg);
}

int tounet_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    return setsockopt(sockfd, level, optname, optval, optlen);
}

ssize_t tounet_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen) {
    return recvfrom(sockfd, buf, len, flags, from, fromlen);
}

ssize_t tounet_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return sendto(sockfd, buf, len, flags, to, tolen);
}

#else /* WINDOWS OS */

#include <iostream>

/* error handling */
int tounet_int_errno;

int tounet_errno() {
    return tounet_int_errno;
}

int tounet_init() {
    tounet_int_errno = 0;

    // Windows Networking Init.
    WORD wVerReq = MAKEWORD(2,2);
    WSADATA wsaData;

    if (0 != WSAStartup(wVerReq, &wsaData)) return -1;
    return 0;
}

int tounet_checkTTL(int sockfd) {
    int optlen = 4;
    char optval[optlen];

    int ret = getsockopt(sockfd, IPPROTO_IP, IP_TTL, optval, &optlen);

    if (ret == SOCKET_ERROR) {
        ret = -1;
        tounet_int_errno = tounet_w2u_errno(WSAGetLastError());
    }/* else {
        std::cerr << "tounet_checkTTL() :";
        std::cerr << (int) optval[0] << ":";
        std::cerr << (int) optval[1] << ":";
        std::cerr << (int) optval[2] << ":";
        std::cerr << (int) optval[3] << ": RET: ";
        std::cerr << ret << ":";
        std::cerr << std::endl;
    }*/
    return ret;
}

int tounet_close(int sockfd) {
    log(LOG_WARNING, tounetzone, QString("Closing TCP over UDP socket ").append(QString::number(sockfd)));
    return closesocket(sockfd);
}

int tounet_socket(int domain, int type, int protocol) {
    int osock = socket(domain, type, protocol);

    if ((unsigned) osock == INVALID_SOCKET) {
        // Invalidate socket Unix style.
        osock = -1;
        tounet_int_errno = tounet_w2u_errno(WSAGetLastError());
    }
    tounet_checkTTL(osock);
    return osock;
}

int tounet_bind(int  sockfd,  const  struct  sockaddr  *my_addr,  socklen_t addrlen) {
    int ret = bind(sockfd,my_addr,addrlen);
    if (ret != 0) {
        ret = -1;
        tounet_int_errno = tounet_w2u_errno(WSAGetLastError());
    }
    return ret;
}

int tounet_fcntl(int fd, int cmd, long arg) {
    int ret;

    unsigned long int on = 1;
    /* We only support setting the socket to non-blocking. */
    if ((cmd != F_SETFL) || (arg != O_NONBLOCK)) {
        tounet_int_errno =  EOPNOTSUPP;
        return -1;
    }

    ret = ioctlsocket(fd, FIONBIO, &on);

    if (ret != 0) {
        /* store unix-style error
         */

        ret = -1;
        tounet_int_errno = tounet_w2u_errno(WSAGetLastError());
    }
    return ret;
}

int tounet_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    /* We only support setting IPPROTO_IP with IP_TTL. */
    if ((level != IPPROTO_IP) || (optname != IP_TTL)) {
        tounet_int_errno =  EOPNOTSUPP;
        return -1;
    }

    int ret = setsockopt(sockfd, level, optname, (const char *) optval, optlen);

    if (ret == SOCKET_ERROR) {
        ret = -1;
        tounet_int_errno = tounet_w2u_errno(WSAGetLastError());
    }
    tounet_checkTTL(sockfd);
    return ret;
}

ssize_t tounet_recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *from, socklen_t *fromlen) {
    int ret = recvfrom(sockfd, (char *) buf, len, flags, from, fromlen);
    if (ret == SOCKET_ERROR) {
        ret = -1;
        tounet_int_errno = tounet_w2u_errno(WSAGetLastError());
    }
    return ret;
}

ssize_t tounet_sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *to, socklen_t tolen) {
    int ret = sendto(sockfd, (const char *) buf, len, flags, to, tolen);
    if (ret == SOCKET_ERROR) {
        ret = -1;
        tounet_int_errno = tounet_w2u_errno(WSAGetLastError());
    }
    return ret;
}

int tounet_w2u_errno(int err) {
    switch (err) {
        case WSAEINPROGRESS:
            return EINPROGRESS;
            break;
        case WSAEWOULDBLOCK:
            return EINPROGRESS;
            break;
        case WSAENETUNREACH:
            return ENETUNREACH;
            break;
        case WSAETIMEDOUT:
            return ETIMEDOUT;
            break;
        case WSAEHOSTDOWN:
            return EHOSTDOWN;
            break;
        case WSAECONNREFUSED:
            return ECONNREFUSED;
            break;
        case WSAEADDRINUSE:
            return EADDRINUSE;
            break;
        case WSAEUSERS:
            return EUSERS;
            break;
            /* This one is returned for UDP recvfrom, when nothing there
             * but not a real error... translate into EINPROGRESS
             */
        case WSAECONNRESET:
            return EINPROGRESS;
            break;
            /***
             *
            case WSAECONNRESET:
                return ECONNRESET;
                break;
             *
             ***/

        default:
            break;
    }

    return ECONNREFUSED; /* sensible default? */
}

void sleep(int sec) {
    Sleep(sec * 1000);
}

void usleep(int usec) {
    Sleep(usec / 1000);
}

#endif
