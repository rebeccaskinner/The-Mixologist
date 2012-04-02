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

#ifndef CONNECTION_REQUEST_H
#define CONNECTION_REQUEST_H

#include <pqi/pqinetwork.h>
#include <QString>

/* UdpTunneler
   UdpTunneler packets are sent at a constant interval to non-connected friends when we are behind a non-symmetric NAT firewall.
   This should tunnel a UDP hole in the firewall to them.
   On receipt, the receiver can take them as knowledge that they have an online friend at that address behind a hole-punched firewall. */

/* Reads the packet, and populates the contents of friendAddress and librarymixer_id.
   Returns false if it couldn't be populated. */
bool parseUdpTunneler(void *data, int size, struct sockaddr_in *friendAddress, unsigned int *librarymixer_id);

/* Reads the packet, and returns true if it is a UDP Tunneler packet. */
bool isUdpTunneler(void *data, int size);

/* Generates a UDP Tunneler and returns a pointer to it, with size populated with the size of the packet. */
void* generateUdpTunneler(int *size, const struct sockaddr_in *ownAddress, unsigned int own_librarymixer_id);

/* UdpConnectionNotice
   Upon receiving a UdpTunneler packet, we now know that there is a firewall hole-punched friend ready to connect to us.
   Our TCP over UDP connections require that both sides begin attempting a connection to each other at the same time.
   Therefore, upon receipt of a UdpTunneler packet, we begin attempting a TCP over UDP connection and send a UdpConnectionNotice so they'll do the same.  */

/* Reads the packet, and populates the contents of friendAddress and librarymixer_id.
   Returns false if it couldn't be populated. */
bool parseUdpConnectionNotice(void *data, int size, struct sockaddr_in *friendAddress, unsigned int *librarymixer_id);

/* Reads the packet, and returns true if it is a UDP Tunneler packet. */
bool isUdpConnectionNotice(void *data, int size);

/* Generates a UDP Tunneler and returns a pointer to it, with size populated with the size of the packet. */
void* generateUdpConnectionNotice(int *size, const struct sockaddr_in *ownAddress, unsigned int own_librarymixer_id);

/* TcpConnectionRequest
   TcpConnectionRequest packets are sent out over UDP to friends when we are not firewalled in order to test for their presence.
   If they are behind a full-cone firewall that they have already punched, they will now know to connect back to us over TCP. */

/* Reads the packet, and populates the contents of friendAddress and librarymixer_id.
   Returns false if it couldn't be populated. */
bool parseTcpConnectionRequest(void *data, int size, struct sockaddr_in *friendAddress, unsigned int *librarymixer_id);

/* Reads the packet, and returns true if it is a Connection Request packet. */
bool isTcpConnectionRequest(void *data, int size);

/* Generates a Connection Request and returns a pointer to it, with size populated with the size of the packet. */
void* generateTcpConnectionRequest(int *size, const struct sockaddr_in *ownAddress, unsigned int own_librarymixer_id);

#endif //CONNECTION_REQUEST_H
