/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-8, Robert Fernie
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


#include "util/net.h"
#include <string.h>

/* enforce LITTLE_ENDIAN on Windows */
#ifdef WINDOWS_SYS
#define BYTE_ORDER  1234
#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN  4321
#endif

uint64_t ntohll(uint64_t x) {
#ifdef BYTE_ORDER
#if BYTE_ORDER == BIG_ENDIAN
    return x;
#elif BYTE_ORDER == LITTLE_ENDIAN

    uint32_t top = (uint32_t) (x >> 32);
    uint32_t bot = (uint32_t) (0x00000000ffffffffULL & x);

    uint64_t rev = ((uint64_t) ntohl(top)) | (((uint64_t) ntohl(bot)) << 32);

    return rev;
#else
#error "ENDIAN determination Failed"
#endif
#else
#error "ENDIAN determination Failed (BYTE_ORDER not defined)"
#endif

}

uint64_t htonll(uint64_t x) {
    return ntohll(x);
}

void sockaddr_clear(struct sockaddr_in *addr) {
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
}


bool isValidNet(struct in_addr *addr) {
    // invalid address.
    if ((*addr).s_addr == INADDR_NONE)
        return false;
    if ((*addr).s_addr == 0)
        return false;
    // should do more tests.
    return true;
}


bool isLoopbackNet(struct in_addr *addr) {
    in_addr_t taddr = ntohl(addr->s_addr);
    return (taddr == (127 << 24 | 1));
}

bool isPrivateNet(struct in_addr *addr) {
    in_addr_t taddr = ntohl(addr->s_addr);

    // 10.0.0.0/8
    // 172.16.0.0/12
    // 192.168.0.0/16
    // 169.254.0.0/16
    if ((taddr>>24 == 10) ||
        (taddr>>20 == (172<<4 | 16>>4)) ||
        (taddr>>16 == (192<<8 | 168)) ||
        (taddr>>16 == (169<<8 | 254))) {
        return true;
    } else {
        return false;
    }
}

bool isExternalNet(struct in_addr *addr) {
    if (!isValidNet(addr)) {
        return false;
    }
    if (isLoopbackNet(addr)) {
        return false;
    }
    if (isPrivateNet(addr)) {
        return false;
    }
    return true;
}



