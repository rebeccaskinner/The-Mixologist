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
   It boths sends STUN packets over UDP, as well as sends UDP packets for streams of TCP over UDP. */

class TouStunPeer;
class UdpPeer;

class UdpSorter: public QObject, public UdpReceiver {
    Q_OBJECT
public:
    /* Creates a new UdpSorter, which will bind a new listening socket onto local. */
    UdpSorter(struct sockaddr_in &local);
    virtual ~UdpSorter();

    /* Maintains STUN keep alive. */
    int tick();

    /* Returns true while the underlying UdpLayer remains error-free. */
    bool okay();

    /* Pass-through to the UDP layer to send packets.
       Returns the amount of data sent on success, or -1 on failure. */
    int sendPkt(void *data, int size, const struct sockaddr_in &to, int ttl);


    /* TCP over UDP stream functions. */
    /* Add a new TCPonUDP stream.
       Returns false if unable to add. */
    bool addUdpPeer(UdpPeer *peer, const struct sockaddr_in &raddr);

    /* Removes a TCPonUDP stream.
       Returns false if unable to find. */
    bool removeUdpPeer(UdpPeer *peer);


    /* STUN functions. */
    /* Sends a STUN packet to the specified stunServer.
       If returnPort is specified, then will set the Response-Port STUN attribute and request the response go to that port.
       If no returnPort is specified, will return to this UdpSorter's port.
       Before calling this, a listener should be set up for the receivedStunPacket signal.
       Returns true on success or -1 on failure. */
    bool sendStunBindingRequest(const struct sockaddr_in &stunServer, const QString &transactionId, int returnPort = 0);

    /* Adds the peer onto the stun list and if we don't know our external address yet, calls doStun on them. */
    void addStunPeer(const struct sockaddr_in &remote, const char *peerid);

    /* Populates the external address into remote, and sets stable to be 1 if it is stable, or 0 if unstable.
       Returns true if it the external address is known and populated into the variables.
       Returns false if external address is not known. */
    bool readExternalAddress(struct sockaddr_in &remote, uint8_t &stable);

    /* Sets whether we should periodically send STUN keep alive packets. */
    void setStunKeepAlive(bool required);

signals:
    /* When a STUN response packet is received, this is emitted with the mappedAddress. */
    void receivedStunBindingResponse(QString transactionId, QString mappedAddress, int mappedPort, UdpSorter* receivedOn);

private:
    /* Called when there is an incoming packet by UdpLayer.
       If it's a stun packet, passes it to locked_handleStunPkt.
       If it's a UDP packet, passes it to the appropriate UdpPeer. */
    virtual void recvPkt(void *data, int size, struct sockaddr_in &from);

    /* Called when there is an incoming STUN packet. */
    bool locked_handleStunPkt(void *data, int size, struct sockaddr_in &from);

    /* Checks when we last sent and received STUN packets, and if it's not too soon, sends a STUN packet to a STUN peer. */
    bool checkStunKeepAlive();

    /* Sends a stun packet to the target address, and updates the mStunLastSend. */
    bool doStun(struct sockaddr_in stun_addr);

    void fixedServerTest();

    /* Called from locked_handleStunPkt to handle received STUN responses.
       If we have two confirmations on the same reportedExternalAddress,
       we know whether we have good external address info and update internal variables accordingly. */
    bool locked_receivedStunResponse(const struct sockaddr_in &remote, const struct sockaddr_in &reportedExternalAddress);

    /* The underlying UDP layer via which we are sending and receiving packets. */
    UdpLayer *udpLayer;

    mutable QMutex sortMtx;

    /* The local address we are binding a listening socket to. */
    struct sockaddr_in localAddress;

    /* The external address, filled in only once we have discovered it using STUN. */
    struct sockaddr_in externalAddress;

    /* Whether the externalAddress variable has been filled in. */
    bool externalAddressKnown;

    /* If the external address is usable.
       If false, this means we have a symmetric NAT, and STUN will be ineffective in finding our external port. */
    bool externalAddressStable;

    /* Sets whether we should periodically send STUN keep alive packets. */
    bool mStunKeepAlive;

    /* These two variables are used for rate limiting purposes on STUN requests. */
    time_t mStunLastRecv;
    time_t mStunLastSend;

    /* The list of friends that we will attempt to contact as STUN servers. */
    QList<TouStunPeer*> mStunList;

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

/**********************************************************************************
 * The peers that are in mStunList.
 **********************************************************************************/
class TouStunPeer {
public:
    TouStunPeer(std::string id_in, const struct sockaddr_in &addr)
        :cert_id(id_in), peerAddress(addr), responseReceived(false), lastsend(0), failCount(0) {}

    std::string cert_id;

    /* The address of the peer. */
    struct sockaddr_in peerAddress;

    /* Our external address as reported by the STUN peer. */
    struct sockaddr_in reportedExternalAddress;

    /* Whether a response has ever been received from this peer. */
    bool responseReceived;

    /* Time of the last sent STUN attempt. */
    time_t lastsend;

    /* The number of sent STUN packets that received no response.
       When this number gets too high, we drop the peer from mStunList. */
    uint32_t failCount;
};

/**********************************************************************************
 * The peers that are in mStunList.
 **********************************************************************************/
/* Reads the packet, and returns true if it is such a UDP STUN packet. */
bool UdpStun_isStunRequest(void *data, int size);
bool UdpStun_isStunResponse(void *data, int size);

/* Reads the packet, and populates addr with the information from the packet in netbyteorder.
   Returns false if not a STUN response packet and addr couldn't be populated. */
bool UdpStun_response(void *stun_pkt, int size, QString &transaction_id, struct sockaddr_in &addr);

/* On the first call, generates a random 96-bit number stored in a QString in a base-16 representation.
   On subsequent calls, simply returns +1 increments from that number. */
QString UdpStun_generate_transaction_id();

/* Generates a STUN request packet into the memory passed to it by stun_pkt.
   transaction_id is the 12 hexadecimal digits that will represent the transaction ID in the new packet.
   This memory must have length len at least 20 in order to fit the stun packet.
   It is possible to have a memory space larger than 20 and to later use the extra space to append STUN attributes. */
bool UdpStun_generate_stun_request(void *stun_pkt, int len, const QString &transaction_id);

/* Takes an existing STUN request packet generated by UdpStun_generate_stun_request, and adds the Response-Port attribute.
   Not compatible with STUN request packets that have any pre-existing attributes.
   Updates the header value in the length to reflect this change. */
bool UdpStun_add_response_port_attribute(void *stun_pkt, int len, uint16_t port);

/* Allocates memory for and returns a pointer for a new STUN response packet to stun_addr, with length len. */
void* UdpStun_generate_stun_reply(struct sockaddr_in *stun_addr, int *len);

#endif
