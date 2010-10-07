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

#ifndef UNIVERSAL_NETWORK_HEADER
#define UNIVERSAL_NETWORK_HEADER

#include <inttypes.h>
#include <stdlib.h> /* Included because GCC4.4 wants it */
#include <string.h>     /* Included because GCC4.4 wants it */

/********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifndef WINDOWS_SYS

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <errno.h>

#else

#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h> /* for ssize_t */
//typedef uint32_t socklen_t;
typedef uint32_t in_addr_t;

int inet_aton(const char *name, struct in_addr *addr);

#endif
/********************************** WINDOWS/UNIX SPECIFIC PART ******************/

/* 64 bit conversions */
uint64_t ntohll(uint64_t x);
uint64_t htonll(uint64_t x);

/* blank a network address */
void sockaddr_clear(struct sockaddr_in *addr);

/* determine network type (moved from pqi/pqinetwork.cc) */
bool isValidNet(struct in_addr *addr);
bool isLoopbackNet(struct in_addr *addr);
bool isPrivateNet(struct in_addr *addr);
bool isExternalNet(struct in_addr *addr);


#endif /* UNIVERSAL_NETWORK_HEADER */
