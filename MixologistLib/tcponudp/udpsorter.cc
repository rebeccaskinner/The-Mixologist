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

#include "tcponudp/udpsorter.h"
#include "tcponudp/stunpacket.h"
#include "tcponudp/connectionrequestpacket.h"
#include "util/net.h"
#include "util/debug.h"
#include "time.h"

static const int DEFAULT_TTL = 64;

/* We need this to be able to have streams be a map where the key is a sockaddr_in. */
bool operator<(const struct sockaddr_in &addr, const struct sockaddr_in &addr2) {
    if (addr.sin_family != addr2.sin_family)
        return (addr.sin_family < addr2.sin_family);
    if (addr.sin_addr.s_addr != addr2.sin_addr.s_addr)
        return (addr.sin_addr.s_addr < addr2.sin_addr.s_addr);
    if (addr.sin_port != addr2.sin_port)
        return (addr.sin_port < addr2.sin_port);
    return false;
}

UdpSorter::UdpSorter(struct sockaddr_in &local)
    :localAddress(local) {

    udpLayer = new UdpLayer(this, localAddress);
    udpLayer->start();
}

UdpSorter::~UdpSorter() {
    QMutexLocker stack(&sortMtx);
    udpLayer->stop();
    while(!udpLayer->isFinished()) {}
    delete udpLayer;
}

void UdpSorter::recvPkt(void *data, int size, struct sockaddr_in &from) {
    QMutexLocker stack(&sortMtx);

    if (isUdpTunneler(data, size)) {
        unsigned int librarymixer_id;
        struct sockaddr_in friendAddress;
        if (parseUdpTunneler(data, size, &friendAddress, &librarymixer_id)) {
            log(LOG_WARNING, UDPSORTERZONE,
                QString("Received a packet indicating ") + QString::number(librarymixer_id) +
                " has punched a UDP hole in their firewall for us to connect to at address " + addressToString(&friendAddress));
            emit receivedUdpTunneler(librarymixer_id, inet_ntoa(friendAddress.sin_addr), ntohs(friendAddress.sin_port));
        }
        return;
    } else if (isUdpConnectionNotice(data, size)) {
        unsigned int librarymixer_id;
        struct sockaddr_in friendAddress;
        if (parseUdpConnectionNotice(data, size, &friendAddress, &librarymixer_id)) {
            log(LOG_WARNING, UDPSORTERZONE,
                QString("Received request to connect via UDP from ") + QString::number(librarymixer_id) +
                " at address " + addressToString(&friendAddress));
            emit receivedUdpConnectionNotice(librarymixer_id, inet_ntoa(friendAddress.sin_addr), ntohs(friendAddress.sin_port));
        }
        return;
    } else if (isTcpConnectionRequest(data, size)) {
        unsigned int librarymixer_id;
        struct sockaddr_in friendAddress;
        if (parseTcpConnectionRequest(data, size, &friendAddress, &librarymixer_id)) {
            log(LOG_WARNING, UDPSORTERZONE,
                QString("Received request to connect back via TCP from ") + QString::number(librarymixer_id) +
                " at address " + addressToString(&friendAddress));
            emit receivedTcpConnectionRequest(librarymixer_id, inet_ntoa(friendAddress.sin_addr), ntohs(friendAddress.sin_port));
        }
        return;
    } else if (UdpStun_isStunRequest(data, size)) {
        QString transactionId;
        if (UdpStun_request(data, size, transactionId, from)) {
            log(LOG_WARNING, UDPSORTERZONE, QString("Received STUN request from ") + addressToString(&from) + ", responding");

            int len;
            void *pkt = UdpStun_generate_stun_response(&from, &len, transactionId);
            if (!pkt) return;

            sendPkt(pkt, len, &from, DEFAULT_TTL);
            free(pkt);
        }
        return;
    } else if (UdpStun_isStunResponse(data, size)) {
        QString transactionId;
        struct sockaddr_in reportedExternalAddress;
        if (UdpStun_response(data, size, transactionId, reportedExternalAddress)) {
            log(LOG_DEBUG_ALERT, UDPSORTERZONE, QString("Received STUN response on port ") + QString::number(ntohs(localAddress.sin_port)));

            emit receivedStunBindingResponse(transactionId,
                                             inet_ntoa(reportedExternalAddress.sin_addr), ntohs(reportedExternalAddress.sin_port),
                                             ntohs(localAddress.sin_port), inet_ntoa(from.sin_addr));

        }
        return;
    }

    /* If we get to here, it's not a special UDP packet, but instead should be a part of a TCP over UDP stream. */
    foreach (struct sockaddr_in recognizedAddress, streams.keys()) {
        if (QString(inet_ntoa(recognizedAddress.sin_addr)) == QString(inet_ntoa(from.sin_addr))) {
            streams[recognizedAddress]->recvPkt(data, size);
            return;
        }
    }

    log(LOG_DEBUG_ALERT, UDPSORTERZONE, QString("Received UDP packet from unknown address ") + addressToString(&from));
}

int UdpSorter::sendPkt(void *data, int size, const struct sockaddr_in *to, int ttl) {
    return udpLayer->sendPkt(data, size, to, ttl);
}

bool UdpSorter::okay() {
    return udpLayer->okay();
}

bool UdpSorter::addUdpPeer(UdpPeer *peer, const struct sockaddr_in &raddr) {
    QMutexLocker stack(&sortMtx);
    if (streams.contains(raddr)) {
        log(LOG_WARNING, UDPSORTERZONE, "Attempted to add already existing UdpPeer!");
        return false;
    }
    streams[raddr] = peer;
    return true;
}

bool UdpSorter::removeUdpPeer(UdpPeer *peer) {
    QMutexLocker stack(&sortMtx);

    QMap<struct sockaddr_in, UdpPeer *>::iterator it;
    for (it = streams.begin(); it != streams.end(); it++) {
        if (it.value() == peer) {
            streams.erase(it);
            return true;
        }
    }

    return false;
}

bool UdpSorter::sendStunBindingRequest(const struct sockaddr_in *stunServer, const QString& transactionId, int returnPort) {
    QMutexLocker stack(&sortMtx);
    if (!okay()) return false;

    int packetLength = 20;
    if (returnPort != 0) packetLength += 8;

    char stundata[packetLength];

    bool packetGenerationSuccess = UdpStun_generate_stun_request(stundata, packetLength, transactionId);
    if (!packetGenerationSuccess) {
        log(LOG_ALERT, PQISTUNZONE, "Unable to generate STUN packet");
        return false;
    }

    if (returnPort != 0) {
        packetGenerationSuccess = UdpStun_add_response_port_attribute(stundata, packetLength, returnPort);
        if (!packetGenerationSuccess) {
            log(LOG_ALERT, PQISTUNZONE, "Unable to add response-port attribute to STUN packet");
            return false;
        }
    }

    log(LOG_DEBUG_ALERT, UDPSORTERZONE,
        "Sending STUN binding request on port " + QString::number(ntohs(localAddress.sin_port)) +
        " to " + addressToString(stunServer));

    return sendPkt(stundata, packetLength, stunServer, DEFAULT_TTL) == packetLength;
}

bool UdpSorter::sendUdpTunneler(const struct sockaddr_in *friendAddress, const struct sockaddr_in *ownExternalAddress, unsigned int own_librarymixer_id) {
    QMutexLocker stack(&sortMtx);
    if (!okay()) return false;

    int packetLength;
    void* newPacket = generateUdpTunneler(&packetLength, ownExternalAddress, own_librarymixer_id);

    log(LOG_DEBUG_ALERT, UDPSORTERZONE,
        QString("Sending UDP Tunneler to ") + addressToString(friendAddress) +
        " via " + QString::number(ntohs(localAddress.sin_port)));

    return sendPkt(newPacket, packetLength, friendAddress, DEFAULT_TTL);
}

bool UdpSorter::sendUdpConnectionNotice(const struct sockaddr_in *friendAddress, const struct sockaddr_in *ownExternalAddress, unsigned int own_librarymixer_id) {
    QMutexLocker stack(&sortMtx);
    if (!okay()) return false;

    int packetLength;
    void* newPacket = generateUdpConnectionNotice(&packetLength, ownExternalAddress, own_librarymixer_id);

    log(LOG_DEBUG_ALERT, UDPSORTERZONE,
        QString("Sending UDP Connection Notice to ") + addressToString(friendAddress) +
        " via " + QString::number(ntohs(localAddress.sin_port)));

    return sendPkt(newPacket, packetLength, friendAddress, DEFAULT_TTL);
}

bool UdpSorter::sendTcpConnectionRequest(const struct sockaddr_in *friendAddress, const struct sockaddr_in *ownExternalAddress, unsigned int own_librarymixer_id) {
    QMutexLocker stack(&sortMtx);
    if (!okay()) return false;

    int packetLength;
    void* newPacket = generateTcpConnectionRequest(&packetLength, ownExternalAddress, own_librarymixer_id);

    log(LOG_DEBUG_ALERT, UDPSORTERZONE,
        QString("Sending TCP Connection Request to ") + addressToString(friendAddress) +
        " via " + QString::number(ntohs(localAddress.sin_port)));

    return sendPkt(newPacket, packetLength, friendAddress, DEFAULT_TTL);
}
