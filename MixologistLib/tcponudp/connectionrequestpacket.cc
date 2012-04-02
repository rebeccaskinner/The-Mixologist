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

#include <tcponudp/connectionrequestpacket.h>

#include <util/debug.h>

#define UDP_TUNNELER_HEADER 0xAF52
#define UDP_CONNECTION_NOTICE_HEADER 0xAF53
#define TCP_CONNECTION_REQUEST_HEADER 0xAF54

/* These utility methods do all the actual work for the various packet types, which are actually all identical other than their headers. */
void parsePacketGeneric(void *data, struct sockaddr_in *friendAddress, unsigned int *librarymixer_id) {
    friendAddress->sin_port = ((uint16_t *) data)[1];
    friendAddress->sin_addr.s_addr = ((uint32_t *) data)[1];
    *librarymixer_id = ntohl(((uint32_t *) data)[2]);
}

bool isPacketGeneric(void *data, int size, uint16_t header) {
    if (size != 12) return false;

    return (((uint16_t *) data)[0] == (uint16_t) htons(header));
}

void* generatePacketGeneric(int* size, const struct sockaddr_in *ownAddress, unsigned int own_librarymixer_id, uint16_t header) {
    void* data = malloc(12);
    *size = 12;

    ((uint16_t *) data)[0] = htons(header); //Set the header
    ((uint16_t *) data)[1] = ownAddress->sin_port; //Set the port
    ((uint32_t *) data)[1] = ownAddress->sin_addr.s_addr; //Set the IP
    ((uint32_t *) data)[2] = htonl(own_librarymixer_id); //Set our LibraryMixer ID

    return data;
}

/* Below here are the public methods that are exposed to the rest of the Mixologist. */
bool parseUdpTunneler(void *data, int size, struct sockaddr_in *friendAddress, unsigned int *librarymixer_id) {
    if (!isUdpTunneler(data, size)) return false;

    parsePacketGeneric(data, friendAddress, librarymixer_id);

    return true;
}

bool isUdpTunneler(void *data, int size) {
    return isPacketGeneric(data, size, UDP_TUNNELER_HEADER);
}

void* generateUdpTunneler(int* size, const struct sockaddr_in *ownAddress, unsigned int own_librarymixer_id) {    
    return generatePacketGeneric(size, ownAddress, own_librarymixer_id, UDP_TUNNELER_HEADER);
}

bool parseUdpConnectionNotice(void *data, int size, struct sockaddr_in *friendAddress, unsigned int *librarymixer_id) {
    if (!isUdpConnectionNotice(data, size)) return false;

    parsePacketGeneric(data, friendAddress, librarymixer_id);

    return true;
}

bool isUdpConnectionNotice(void *data, int size) {
    return isPacketGeneric(data, size, UDP_CONNECTION_NOTICE_HEADER);
}

void* generateUdpConnectionNotice(int* size, const struct sockaddr_in *ownAddress, unsigned int own_librarymixer_id) {
    return generatePacketGeneric(size, ownAddress, own_librarymixer_id, UDP_CONNECTION_NOTICE_HEADER);
}

bool parseTcpConnectionRequest(void *data, int size, struct sockaddr_in *friendAddress, unsigned int *librarymixer_id) {
    if (!isTcpConnectionRequest(data, size)) return false;

    parsePacketGeneric(data, friendAddress, librarymixer_id);

    return true;
}

bool isTcpConnectionRequest(void *data, int size) {
    return isPacketGeneric(data, size, TCP_CONNECTION_REQUEST_HEADER);
}

void* generateTcpConnectionRequest(int *size, const struct sockaddr_in *ownAddress, unsigned int own_librarymixer_id) {
    return generatePacketGeneric(size, ownAddress, own_librarymixer_id, TCP_CONNECTION_REQUEST_HEADER);
}
