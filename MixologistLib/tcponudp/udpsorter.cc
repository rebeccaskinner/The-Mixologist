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

//Must be included before udpsorter.h which includes util/net.h to avoid _WIN32_WINNT redefine warnings on Windows
#include "pqi/pqinetwork.h"

#include "tcponudp/udpsorter.h"
#include "tcponudp/stunbasics.h"
#include "util/net.h"
#include "util/debug.h"
#include "time.h"

static const int STUN_TTL = 64;

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
    delete udpLayer;
}

void UdpSorter::recvPkt(void *data, int size, struct sockaddr_in &from) {
    QMutexLocker stack(&sortMtx);

    /* check for STUN packet */
    if (UdpStun_isStunRequest(data, size) || UdpStun_isStunResponse(data, size)) {
        locked_handleStunPkt(data, size, from);
        return;
    }

    /* look for a peer */
    foreach (struct sockaddr_in recognizedAddress, streams.keys()) {
        if (QString(inet_ntoa(recognizedAddress.sin_addr)) == QString(inet_ntoa(from.sin_addr))) {
            streams[recognizedAddress]->recvPkt(data, size);
            return;
        }
    }

    log(LOG_WARNING, UDPSORTERZONE, QString("Received UDP packet from unknown address ") + inet_ntoa(from.sin_addr));

#ifdef false
    QMap<struct sockaddr_in, UdpPeer *>::iterator it;
    it = streams.find(from);

    if (it != streams.end()) {
        /* forward to them */
        it.value()->recvPkt(data, size);
    } else {
        log(LOG_WARNING, UDPSORTERZONE, QString("Received UDP packet from unknown address ") + inet_ntoa(from.sin_addr));
    }
#endif
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

bool UdpSorter::locked_handleStunPkt(void *data, int size, struct sockaddr_in &from) {
    QString transactionId;

    if (UdpStun_isStunRequest(data, size)) {
        if (UdpStun_request(data, size, transactionId, from)) {
            log(LOG_WARNING, UDPSORTERZONE, QString("Received STUN request from ") + inet_ntoa(from.sin_addr) + ", responding");

            int len;
            void *pkt = UdpStun_generate_stun_response(&from, &len, transactionId);
            if (!pkt) return false;

            int sentlen = sendPkt(pkt, len, &from, STUN_TTL);
            free(pkt);

            return (len == sentlen);
        }
    } else if (UdpStun_isStunResponse(data, size)) {
        struct sockaddr_in reportedExternalAddress;
        if (UdpStun_response(data, size, transactionId, reportedExternalAddress)) {
            log(LOG_DEBUG_ALERT, UDPSORTERZONE, QString("Received STUN response on port ") + QString::number(ntohs(localAddress.sin_port)));

            emit receivedStunBindingResponse(transactionId,
                                             inet_ntoa(reportedExternalAddress.sin_addr), ntohs(reportedExternalAddress.sin_port),
                                             ntohs(localAddress.sin_port), inet_ntoa(from.sin_addr));

            return true;
        }
    }

    log(LOG_DEBUG_ALERT, UDPSORTERZONE, "UdpSorter::handleStunPkt() Bad Packet");
    return false;
}

bool UdpSorter::sendStunBindingRequest(const struct sockaddr_in *stunServer, const QString& transactionId, int returnPort) {
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

    log(LOG_DEBUG_ALERT, UDPSORTERZONE, "Sending STUN binding request on port " + QString::number(ntohs(localAddress.sin_port)));

    return sendPkt(stundata, packetLength, stunServer, STUN_TTL) == packetLength;
}
