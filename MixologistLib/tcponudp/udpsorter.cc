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

    log(LOG_ALERT, UDPSORTERZONE, "Opened UDP port on:" + QString(inet_ntoa(local.sin_addr)) + ":" + QString::number(ntohs(local.sin_port)) + "\n");
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

int  UdpSorter::sendPkt(void *data, int size, struct sockaddr_in &to, int ttl) {
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
    if (UdpStun_isStunRequest(data, size)) {
        log(LOG_DEBUG_ALERT, UDPSORTERZONE, QString("Received STUN request from ") + inet_ntoa(from.sin_addr));

        /* generate a response */
        int len;
        void *pkt = UdpStun_generate_stun_reply(&from, &len);
        if (!pkt) return false;

        int sentlen = sendPkt(pkt, len, from, STUN_TTL);
        free(pkt);

        return (len == sentlen);
    } else if (UdpStun_isStunResponse(data, size)) {
        log(LOG_WARNING, UDPSORTERZONE, QString("Received STUN response from ") + inet_ntoa(from.sin_addr));
        struct sockaddr_in reportedExternalAddress;
        bool good = UdpStun_response(data, size, reportedExternalAddress);
        if (good) {
            log(LOG_WARNING, UDPSORTERZONE,
                QString("Got external address from STUN: ") + inet_ntoa(reportedExternalAddress.sin_addr) +
                QString(":") + QString::number(ntohs(reportedExternalAddress.sin_port)));
            locked_receivedStunResponse(from, reportedExternalAddress);

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

#define MAX_STUN_SIZE 64
bool UdpSorter::doStun(struct sockaddr_in stun_addr) {
    if (!okay()) return false;

    char stundata[MAX_STUN_SIZE];
    int tmplen = MAX_STUN_SIZE;
    bool packetGenerationSuccess = UdpStun_generate_stun_request(stundata, &tmplen);
    if (!packetGenerationSuccess) {
        pqioutput(PQL_ALERT, PQISTUNZONE, "pqistunner::doStun() unable to generate packet");
        return false;
    }

    sendPkt(stundata, tmplen, stun_addr, STUN_TTL);

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

    char stundata[MAX_STUN_SIZE];
    int tmplen = MAX_STUN_SIZE;
    bool packetGenerationSuccess = UdpStun_generate_stun_request(stundata, &tmplen);
    if (!packetGenerationSuccess) {
        pqioutput(PQL_ALERT, PQISTUNZONE, "pqistunner::doStun() unable to generate packet");
        return;
    }

    struct sockaddr_in stun_addr;
    if (!LookupDNSAddr("stun.selbie.com", stun_addr)) return;
    stun_addr.sin_port = htons(3478);

    log(LOG_ALERT, UDPSORTERZONE, "Sending fixed server test to " + QString(inet_ntoa(stun_addr.sin_addr)) + ":" + QString::number(ntohs(stun_addr.sin_port)));
    lastSend = time(NULL);
    sendPkt(stundata, tmplen, stun_addr, STUN_TTL);
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
    LookupDNSAddr("stun.selbie.com", stun_addr);
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

/******************************* STUN Handling ********************************/
/***** These next functions are generic and not dependent on class variables **/
/******************************* STUN Handling ********************************/

bool UdpStun_isStunRequest(void *data, int size) {
    /* Too small to be a stun packet that contains a header. */
    if (size < 20) return false;

    /* Match size field. */
    uint16_t pktsize = ntohs(((uint16_t *) data)[1]) + 20;
    if (size != pktsize) return false;

    /* This matches the profile of a request. */
    if (0x0001 == ntohs(((uint16_t *) data)[0])) return true;

    return false;
}

bool UdpStun_isStunResponse(void *data, int size) {
    /* Too small to be a stun packet that contains a header. */
    if (size < 20) return false;

    /* Match size field. */
    uint16_t pktsize = ntohs(((uint16_t *) data)[1]) + 20;
    if (size != pktsize) return false;

    /* This matches the profile of a response. */
    if (0x0101 == ntohs(((uint16_t *) data)[0])) return true;

    return false;
}

bool UdpStun_response(void *stun_pkt, int size, struct sockaddr_in &addr) {
    if (!UdpStun_isStunResponse(stun_pkt, size)) return false;

    /* We are going to be stepping through the stun_pkt, this is how far in we are in bytes.
       We start at 20 in order to start at the beginning of the attributes, right after the header. */
    int index_in_stun_pkt = 20;


    bool mappedAddressFound = false;
    struct sockaddr_in mappedAddress;

    bool xorMappedAddressFound = false;
    struct sockaddr_in xorMappedAddress;

    while (index_in_stun_pkt < size) {
        /* We need to divide our index in 2 in order to read 16 bit values out (or later 4 for 32 bit values).
           We know that it will be divisible evenly by 4 because we start on 20, and only add in multiples of 4. */
        uint16_t attribute_type = ntohs(((uint16_t *) stun_pkt)[index_in_stun_pkt / 2]);
        uint16_t attribute_byte_length = ntohs(((uint16_t *) stun_pkt)[(index_in_stun_pkt / 2) + 1]);

        /* 0x0001 is the attribute type code for returning a mapped address. */
        if (attribute_type == 0x0001) {
            if (attribute_byte_length != 8) {
                log(LOG_WARNING, UDPSORTERZONE, "Received invalid address in response for STUN address");
                return false;
            }
            /* All stay in netbyteorder! */
            uint16_t addressFamily = ntohs(((uint16_t *) stun_pkt)[(index_in_stun_pkt / 2) + 2]);
            if (addressFamily == 0x01) mappedAddress.sin_family = AF_INET;
            else {
                log(LOG_WARNING, UDPSORTERZONE, "Received non-IPv4 response for STUN address " + QString::number(addressFamily));
                return false;
            }
            mappedAddress.sin_port = ((uint16_t *) stun_pkt)[(index_in_stun_pkt / 2) + 3];
            mappedAddress.sin_addr.s_addr = ((uint32_t *) stun_pkt)[(index_in_stun_pkt / 4) + 2];
            mappedAddressFound = true;
        }
        /* 0x0020 is the attribute type for returning an XOR mapped addressed. */
        else if (attribute_type == 0x0020) {
            if (attribute_byte_length != 8) {
                log(LOG_WARNING, UDPSORTERZONE, "Received invalid address in response for XOR STUN address");
                return false;
            }
            /* All stay in netbyteorder! */
            uint16_t addressFamily = ntohs(((uint16_t *) stun_pkt)[(index_in_stun_pkt / 2) + 2]);
            if (addressFamily == 0x01) xorMappedAddress.sin_family = AF_INET;
            else {
                log(LOG_WARNING, UDPSORTERZONE, "Received non-IPv4 response for XOR STUN address " + QString::number(addressFamily));
                return false;
            }
            /* In order to decode the XOR mapping on the XOR mapped address, we use the magic cookie to XOR the host order bits.
               The port uses only the most significant bits of the magic cookie. */
            xorMappedAddress.sin_port = htons(0x2112 ^ ntohs(((uint16_t *) stun_pkt)[(index_in_stun_pkt / 2) + 3]));
            xorMappedAddress.sin_addr.s_addr = htonl(0x2112A442 ^ ntohl(((uint32_t *) stun_pkt)[(index_in_stun_pkt / 4) + 2]));
            xorMappedAddressFound = true;
        }
        else {
            log(LOG_DEBUG_ALERT, UDPSORTERZONE, "Ignoring STUN response attribute 0x" + QString::number(attribute_type, 16));
        }

        /* Regarding the last bit with the modulus arithmetic, STUN attribute values must end on 4 byte boundaries.
           Therefore, if the length is less than 32 bit it will be padded with ignored values, and we must
           increase the index by an amount between 0-3 bytes. */
        index_in_stun_pkt = index_in_stun_pkt +
                            2 + //The 2 bytes type field
                            2 + //The 2 byte length field
                            attribute_byte_length +
                            ((4 - (attribute_byte_length % 4)) % 4); //The padding, up to 4 bytes
    }

    if (!xorMappedAddressFound && !mappedAddressFound) return false;

    /* XOR mapped addresses are more reliable than regular mapped addresses.
       This is because according to RFC 5389, some NATs tamper with mapped addresses,
       which is what led to the addition of the XOR mapped address attribute in STUN. */
    if (xorMappedAddressFound) {
        addr.sin_family = xorMappedAddress.sin_family;
        addr.sin_port = xorMappedAddress.sin_port;
        addr.sin_addr.s_addr = xorMappedAddress.sin_addr.s_addr;
        if (mappedAddressFound) {
            if (xorMappedAddress.sin_family != mappedAddress.sin_family ||
                xorMappedAddress.sin_port != mappedAddress.sin_port ||
                xorMappedAddress.sin_addr.s_addr != mappedAddress.sin_addr.s_addr) {
                log(LOG_WARNING, UDPSORTERZONE, "STUN response shows signs of tampering by a router or firewall.");
                /* TODO We should probably do something with this information. */
            }
        }
    } else if (mappedAddressFound) {
        addr.sin_family = mappedAddress.sin_family;
        addr.sin_port = mappedAddress.sin_port;
        addr.sin_addr.s_addr = mappedAddress.sin_addr.s_addr;
    }
    return true;
}

bool UdpStun_generate_stun_request(void *stun_pkt, int *len) {
    /* A STUN packet consists of a 20-byte header followed by 0 or more attributes (we have 0).
       The first 2 bits of the header are 0.
       The second 14 bits of the header are the message type. Message type 1 represents an address binding request.
       The next 16 bits of the header are the message length in bytes, not including the 20-byte header.
       The next 32 bits are the "magic cookie", which must equal 0x2112A442.
       The final 96 bits are the transaction ID, which is an arbitrary number that the server will echo back in its response. */

    /* Too small to be a stun packet that contains a header. */
    if (*len < 20) return false;

    /* Just the header, we're not adding any attributes. */
    ((uint16_t *) stun_pkt)[0] = (uint16_t) htons(0x0001); //Set the 00 and the message type to request.
    ((uint16_t *) stun_pkt)[1] = (uint16_t) htons(0); //Set the length to 0, since we have no attributes
    ((uint32_t *) stun_pkt)[1] = (uint32_t) htonl(0x2112A442); //Magic cookie
    /* Transaction ID, just some arbitrary numers */
    ((uint32_t *) stun_pkt)[2] = (uint32_t) htonl(0x0121);
    ((uint32_t *) stun_pkt)[3] = (uint32_t) htonl(0x0111);
    ((uint32_t *) stun_pkt)[4] = (uint32_t) htonl(0x1010);
    *len = 20;
    return true;
}


void *UdpStun_generate_stun_reply(struct sockaddr_in *stun_addr, int *len) {
    /* A STUN binding response consists of a 20-byte header followed by at least the address (which is all we include).
       The header is arranged the same as our generic stun packet, and 20 bytes.
       The mapped address attribute consists of:
       First 16 bits are the attribute type. Attribute type 1 is mapped address type.
       The second 16 bits are the attribute length of the value section. This is 8 for a mapped address.
       The next 8 bits are the beginning of the value section, and are defined 0.
       The next 8 bits are the address family. Address family 1 represents IPv4.
       The next 16 bits are the port numer.
       The final 32 bits are the IPv4 address as an s_addr. */

    /* 20 byte header + 4 byte attribute header + 8 byte attribute */
    void *stun_pkt = malloc(32);

    /* First the header. */
    ((uint16_t *) stun_pkt)[0] = (uint16_t) htons(0x0101); //Set the 00 and the message type to response.
    ((uint16_t *) stun_pkt)[1] = (uint16_t) htons(8); //Skip the 20 byte header + 8 byte mapped address
    ((uint32_t *) stun_pkt)[1] = (uint32_t) htonl(0x2112A442); //Magic cookie
    /* Transaction ID, just some arbitrary numers */
    ((uint32_t *) stun_pkt)[2] = (uint32_t) htonl(0x0121);
    ((uint32_t *) stun_pkt)[3] = (uint32_t) htonl(0x0111);
    ((uint32_t *) stun_pkt)[4] = (uint32_t) htonl(0x1010);

    /* Now add the mapped address type and length. */
    ((uint16_t *) stun_pkt)[10] = htons(0x0001); //Set the attribute type mapped address
    ((uint16_t *) stun_pkt)[11] = htons(8); //Attribute length for mapped address

    /* Now add the value of the mapped address attribute. */
    ((uint16_t *) stun_pkt)[12] = htons(0x0001); //Set the 00 and the address family
    ((uint16_t *) stun_pkt)[13] = stun_addr->sin_port; //Set the port
    ((uint32_t *) stun_pkt)[7] = stun_addr->sin_addr.s_addr;

    *len = 32;
    return stun_pkt;
}
