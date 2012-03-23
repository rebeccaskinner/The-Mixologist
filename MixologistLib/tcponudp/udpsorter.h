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

#ifndef TOU_UDP_SORTER_H
#define TOU_UDP_SORTER_H

/* universal networking functions */
#include "tcponudp/udplayer.h"
#include "util/net.h"

#include <QList>
#include <QMap>

/* UdpSorter is the principal interface to the UDP layer.
   It sends/receives UDP packets for streams of TCP over UDP.
   In addition, it can send/receive STUN packets. */

class UdpPeer;

class UdpSorter: public QObject, public UdpReceiver {
    Q_OBJECT
public:
    /* Creates a new UdpSorter, which will bind a new listening socket onto local. */
    UdpSorter(struct sockaddr_in &local);
    virtual ~UdpSorter();

    /* Returns true while the underlying UdpLayer remains error-free. */
    bool okay();

    /* Sends a STUN packet to the specified stunServer.
       If returnPort is specified, then will set the Response-Port STUN attribute and request the response go to that port.
       If no returnPort is specified, will return to this UdpSorter's port.
       Before calling this, a listener should be set up for the receivedStunBindingResponse signal.
       Returns true on success or -1 on failure. */
    bool sendStunBindingRequest(const struct sockaddr_in *stunServer, const QString &transactionId, int returnPort = 0);

    /* Pass-through to the UDP layer to send packets.
       Returns the amount of data sent on success, or -1 on failure. */
    int sendPkt(void *data, int size, const struct sockaddr_in *to, int ttl);

    /* TCP over UDP stream functions. */
    /* Add a new TCPonUDP stream.
       Returns false if unable to add. */
    bool addUdpPeer(UdpPeer *peer, const struct sockaddr_in &raddr);

    /* Removes a TCPonUDP stream.
       Returns false if unable to find. */
    bool removeUdpPeer(UdpPeer *peer);

signals:
    /* When a STUN response packet is received, this is emitted with the mappedAddress. */
    void receivedStunBindingResponse(QString transactionId, QString mappedAddress, int mappedPort, ushort receivedOnPort, QString receivedFromAddress);

private:
    /* Called when there is an incoming packet by UdpLayer.
       If it's a stun packet, passes it to locked_handleStunPkt.
       If it's a UDP packet, passes it to the appropriate UdpPeer. */
    virtual void recvPkt(void *data, int size, struct sockaddr_in &from);

    /* Called when there is an incoming STUN packet. */
    bool locked_handleStunPkt(void *data, int size, struct sockaddr_in &from);

    /* The underlying UDP layer via which we are sending and receiving packets. */
    UdpLayer *udpLayer;

    mutable QMutex sortMtx;

    /* The local address we are binding a listening socket to. */
    struct sockaddr_in localAddress;

    /* These are friends that we are communicating with via TCP over UDP. */
    QMap<struct sockaddr_in, UdpPeer*> streams;
};

/**********************************************************************************
 * Interface class for TCP over UDP streams that wish to be able to receive incoming packets from
 * the UdpSorter.
 **********************************************************************************/
class UdpPeer {
public:
    virtual void recvPkt(void *data, int size) = 0;
};

#endif
