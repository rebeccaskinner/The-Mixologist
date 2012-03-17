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
    :udpLayer(NULL), localAddress(local), externalAddressKnown(false), externalAddressStable(false), mStunKeepAlive(false), mStunLastRecv(0), mStunLastSend(0) {

    sockaddr_clear(&externalAddress);

    udpLayer = new UdpLayer(this, localAddress);
    udpLayer->start();
}

UdpSorter::~UdpSorter() {
    delete udpLayer;
}

void UdpSorter::recvPkt(void *data, int size, struct sockaddr_in &from) {
    QMutexLocker stack(&sortMtx);
    mStunLastRecv = time(NULL);

    /* look for a peer */
    QMap<struct sockaddr_in, UdpPeer *>::iterator it;
    it = streams.find(from);

    /* check for STUN packet */
    if (UdpStun_isStunRequest(data, size) || UdpStun_isStunResponse(data, size)) {
        locked_handleStunPkt(data, size, from);
    } else if (it == streams.end()) {
        log(LOG_WARNING, UDPSORTERZONE, QString("Received UDP packet from unknown address ") + inet_ntoa(from.sin_addr));
    } else {
        /* forward to them */
        it.value()->recvPkt(data, size);
    }
}

int UdpSorter::sendPkt(void *data, int size, const struct sockaddr_in *to, int ttl) {
    return udpLayer->sendPkt(data, size, to, ttl);
}

bool UdpSorter::okay() {
    return udpLayer->okay();
}

int UdpSorter::tick() {
    checkStunKeepAlive();

    fixedServerTest();

    return 1;
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
            log(LOG_WARNING, UDPSORTERZONE, QString("Received STUN response on port ") + QString::number(ntohs(localAddress.sin_port)));

            emit receivedStunBindingResponse(transactionId,
                                             inet_ntoa(reportedExternalAddress.sin_addr), ntohs(reportedExternalAddress.sin_port),
                                             ntohs(localAddress.sin_port));

            return true;
        }
    }

    log(LOG_DEBUG_ALERT, UDPSORTERZONE, "UdpSorter::handleStunPkt() Bad Packet");
    return false;
}


bool UdpSorter::readExternalAddress(struct sockaddr_in &external, uint8_t &stable) {
    if (externalAddressKnown) {
        external = externalAddress;

        if (externalAddressStable) stable = 1;
        else stable = 0;

        return true;
    }
    return false;
}

#define STUN_REQUEST_SIZE 20
bool UdpSorter::doStun(struct sockaddr_in stun_addr) {
    if (!okay()) return false;

    char stundata[STUN_REQUEST_SIZE];
    int tmplen = STUN_REQUEST_SIZE;
    bool packetGenerationSuccess = UdpStun_generate_stun_request(stundata, tmplen, UdpStun_generate_transaction_id());
    if (!packetGenerationSuccess) {
        log(LOG_ALERT, PQISTUNZONE, "pqistunner::doStun() unable to generate packet");
        return false;
    }

    sendPkt(stundata, tmplen, &stun_addr, STUN_TTL);

    {
        QMutexLocker stack(&sortMtx);
        mStunLastSend = time(NULL);
    }

    return true;
}

/******************************* STUN Handling ********************************/

#define TOU_STUN_MAX_FAIL_COUNT 10 /* 10 tries (could be higher?) */
#define TOU_STUN_MAX_SEND_RATE 5  /* every 5 seconds */
#define TOU_STUN_MAX_RECV_RATE 25 /* every 25 seconds */

/******************************* STUN Handling ********************************/

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

    log(LOG_WARNING, UDPSORTERZONE, "Sending STUN binding request on port " + QString::number(ntohs(localAddress.sin_port)));

    return sendPkt(stundata, packetLength, stunServer, STUN_TTL) == packetLength;
}

void UdpSorter::setStunKeepAlive(bool required) {
    QMutexLocker stack(&sortMtx);
    mStunKeepAlive = required;
}

void UdpSorter::addStunPeer(const struct sockaddr_in &peerAddress, const char *peerid) {
    /* First add peer into mStunList if it's not already. */
    {
        QMutexLocker stack(&sortMtx);

        bool alreadyExists = false;
        foreach (TouStunPeer* currentPeer, mStunList) {
            if ((peerAddress.sin_addr.s_addr == currentPeer->peerAddress.sin_addr.s_addr) &&
                (peerAddress.sin_port == currentPeer->peerAddress.sin_port)) {
                alreadyExists = true;
                break;
            }
        }

        if (!alreadyExists) {
            TouStunPeer* peer = new TouStunPeer(std::string(peerid), peerAddress);
            mStunList.push_back(peer);
        }
    }

    /* Then contact the peer if we need to stun. */
    bool needStun;
    {
        QMutexLocker stack(&sortMtx);
        needStun = (!externalAddressKnown);
    }
    if (needStun) doStun(peerAddress);
}

bool UdpSorter::checkStunKeepAlive() {
    TouStunPeer* peer;
    time_t now;
    {
        QMutexLocker stack(&sortMtx);

        if (!mStunKeepAlive) return false;

        /* Check if it's time to send a keep alive STUN packet. */
        now = time(NULL);
        if ((now - mStunLastSend < TOU_STUN_MAX_SEND_RATE) ||
            (now - mStunLastRecv < TOU_STUN_MAX_RECV_RATE)) {
            /* Too soon for another STUN packet. */
            return false;
        }

        if (mStunList.size() == 0) return false;

        /* extract entry */
        peer = mStunList.front();
        mStunList.pop_front();
    }

    doStun(peer->peerAddress);

    {
        QMutexLocker stack(&sortMtx);

        if (peer->failCount < TOU_STUN_MAX_FAIL_COUNT) {
            /* We preemptively raise the failCount.
               If we end up receiving a STUN response, that will reset this to 0 anyway. */
            peer->failCount++;
            peer->lastsend = now;

            /* Push them onto the back of mStunList for more tries in the future. */
            mStunList.push_back(peer);
        } else {
            delete peer;
        }
    }

    return true;
}

void UdpSorter::fixedServerTest() {
    if (externalAddressKnown) return;

    static time_t lastSend;
    if (time(NULL) - lastSend < 20) return;

    if (!okay()) return;

    char stundata[STUN_REQUEST_SIZE];
    int tmplen = STUN_REQUEST_SIZE;
    bool packetGenerationSuccess = UdpStun_generate_stun_request(stundata, tmplen, UdpStun_generate_transaction_id());
    if (!packetGenerationSuccess) {
        log(LOG_ALERT, PQISTUNZONE, "pqistunner::doStun() unable to generate packet");
        return;
    }

    struct sockaddr_in stun_addr;
    if (!LookupDNSAddr("stun.selbie.com", &stun_addr)) return;
    stun_addr.sin_port = htons(3478);

    log(LOG_ALERT, UDPSORTERZONE, "Sending fixed server test to " + QString(inet_ntoa(stun_addr.sin_addr)) + ":" + QString::number(ntohs(stun_addr.sin_port)));
    lastSend = time(NULL);
    sendPkt(stundata, tmplen, &stun_addr, STUN_TTL);
}

bool UdpSorter::locked_receivedStunResponse(const struct sockaddr_in &peerAddress, const struct sockaddr_in &reportedExternalAddress) {
    bool found = true;

    /* Find and update the appropriate TouStunPeer. */
    foreach (TouStunPeer* currentPeer, mStunList) {
        if ((peerAddress.sin_addr.s_addr == currentPeer->peerAddress.sin_addr.s_addr) &&
            (peerAddress.sin_port == currentPeer->peerAddress.sin_port)) {
            currentPeer->failCount = 0;
            currentPeer->reportedExternalAddress = reportedExternalAddress;
            currentPeer->responseReceived = true;

            found = true;
            break;
        }
    }

    struct sockaddr_in stun_addr;
    LookupDNSAddr("stun.selbie.com", &stun_addr);
    if (peerAddress.sin_addr.s_addr == stun_addr.sin_addr.s_addr) {
        log(LOG_ALERT, UDPSORTERZONE, "Received response from selbie:" + QString(inet_ntoa(reportedExternalAddress.sin_addr)) + ":" + QString::number(ntohs(reportedExternalAddress.sin_port)) + "\n");
        externalAddress = reportedExternalAddress;
        externalAddressStable = true;
        externalAddressKnown = true;
    }

    if (!externalAddressKnown) {
        /* We need to find two peers with responses so that we can compare them to each other
           and ensure we are getting consistent responses. */
        TouStunPeer* firstPeerWithResponse = NULL;
        TouStunPeer* secondPeerWithResponse = NULL;
        foreach (TouStunPeer* currentPeer, mStunList) {
            if (currentPeer->responseReceived && isExternalNet(&currentPeer->reportedExternalAddress.sin_addr)) {
                if (firstPeerWithResponse == NULL) {
                    firstPeerWithResponse = currentPeer;
                } else {
                    secondPeerWithResponse = currentPeer;
                    break;
                }
            }
        }

        /* If we've got two responses, we can now compare them and find out our external address.
           If they differ, that means that we are dealing with a symmetric NAT, and STUN will be unable to map an external port for us. */
        if (firstPeerWithResponse && secondPeerWithResponse) {
            externalAddressKnown = true;
            externalAddress = reportedExternalAddress;
            if ((firstPeerWithResponse->reportedExternalAddress.sin_addr.s_addr == secondPeerWithResponse->reportedExternalAddress.sin_addr.s_addr) &&
                (firstPeerWithResponse->reportedExternalAddress.sin_port == secondPeerWithResponse->reportedExternalAddress.sin_port)) {
                externalAddressStable = true;
            } else {
                externalAddressStable = false;
            }
        }
    }

    return found;
}
