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

#include "tcponudp/stunbasics.h"

#include <util/debug.h>

#include <time.h>

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

QString convertToHexString(qint16 input) {
    QString stringifiedNumber = QString::number(input, 16);
    while(stringifiedNumber.length() < 4) {
        stringifiedNumber.prepend("0");
    }
    return stringifiedNumber;
}

bool UdpStun_request(void *stun_pkt, int size, QString &transaction_id, struct sockaddr_in &sendResponseToAddr) {
    if (!UdpStun_isStunRequest(stun_pkt, size)) return false;

    /* Pull out of the header out of the transaction_id, which are bytes 9-20 of the header. */
    transaction_id.clear();
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[4])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[5])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[6])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[7])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[8])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[9])));

    /* We are going to be stepping through the stun_pkt, this is how far in we are in bytes.
       We start at 20 in order to start at the beginning of the attributes, right after the header. */
    int index_in_stun_pkt = 20;

    while (index_in_stun_pkt < size) {
        /* We need to divide our index in 2 in order to read 16 bit values out (or later 4 for 32 bit values).
           We know that it will be divisible evenly by 4 because we start on 20, and only add in multiples of 4. */
        uint16_t attribute_type = ntohs(((uint16_t *) stun_pkt)[index_in_stun_pkt / 2]);
        uint16_t attribute_byte_length = ntohs(((uint16_t *) stun_pkt)[(index_in_stun_pkt / 2) + 1]);

        /* 0x0027 is the attribute type for requesting a Response-Port. */
        if (attribute_type == 0x0027) {
            if (attribute_byte_length != 2) {
                log(LOG_WARNING, UDPSORTERZONE, "Received invalid Response-Port attribute in STUN request");
                return false;
            }
            sendResponseToAddr.sin_port = ((uint16_t *) stun_pkt)[(index_in_stun_pkt / 2) + 2];
        } else {
            log(LOG_DEBUG_ALERT, UDPSORTERZONE, "Ignoring STUN request attribute 0x" + QString::number(attribute_type, 16));
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

    return true;
}

bool UdpStun_response(void *stun_pkt, int size, QString &transaction_id, struct sockaddr_in &addr) {
    if (!UdpStun_isStunResponse(stun_pkt, size)) return false;

    /* Pull out of the header out of the transaction_id, which are bytes 9-20 of the header. */
    transaction_id.clear();
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[4])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[5])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[6])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[7])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[8])));
    transaction_id.append(convertToHexString(ntohs(((qint16 *) stun_pkt)[9])));

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

QString UdpStun_generate_transaction_id() {
    qsrand(time(NULL));
    static qint16 transaction_id_1 = qrand() % 32767;
    static qint16 transaction_id_2 = qrand() % 32767;
    static qint16 transaction_id_3 = qrand() % 32767;
    static qint16 transaction_id_4 = qrand() % 32767;
    static qint16 transaction_id_5 = qrand() % 32767;
    static qint16 transaction_id_6 = qrand() % 32767;
    /* We increment the least significant bit.
       As we are only incrementing 16 of our bits, we will be limited to only generating 64k unique ids.
       That should be plenty. */
    transaction_id_6++;

    /* Convert the random numbers into a QString. */
    QString transaction_id_string;
    transaction_id_string
            .append(convertToHexString(transaction_id_1))
            .append(convertToHexString(transaction_id_2))
            .append(convertToHexString(transaction_id_3))
            .append(convertToHexString(transaction_id_4))
            .append(convertToHexString(transaction_id_5))
            .append(convertToHexString(transaction_id_6));
    return transaction_id_string;
}

bool UdpStun_generate_stun_request(void *stun_pkt, int len, const QString &transaction_id) {
    /* A STUN packet consists of a 20-byte header followed by 0 or more attributes (we have 0).
       The first 2 bits of the header are 0.
       The second 14 bits of the header are the message type. Message type 1 represents an address binding request.
       The next 16 bits of the header are the message length in bytes, not including the 20-byte header.
       The next 32 bits are the "magic cookie", which must equal 0x2112A442.
       The final 96 bits are the transaction ID, which is an arbitrary number that the server will echo back in its response. */

    /* Too small to be a stun packet that contains a header. */
    if (len < 20) return false;

    if (transaction_id.length() != 24) return false;

    /* Just the header, we're not adding any attributes. */
    ((uint16_t *) stun_pkt)[0] = (uint16_t) htons(0x0001); //Set the 00 and the message type to request.
    ((uint16_t *) stun_pkt)[1] = (uint16_t) htons(0); //Set the length to 0, since we have no attributes
    ((uint32_t *) stun_pkt)[1] = (uint32_t) htonl(0x2112A442); //Magic cookie
    /* Transaction ID */
    bool ok;
    ((uint32_t *) stun_pkt)[2] = (uint32_t) htonl(transaction_id.left(8).toLong(&ok, 16));
    ((uint32_t *) stun_pkt)[3] = (uint32_t) htonl(transaction_id.mid(8, 8).toLong(&ok, 16));
    ((uint32_t *) stun_pkt)[4] = (uint32_t) htonl(transaction_id.right(8).toLong(&ok, 16));

    return true;
}

bool UdpStun_add_response_port_attribute(void *stun_pkt, int len, uint16_t port) {
    /* Too small to contain the response port attribute. */
    if (len < 28) return false;

    /* Update the header, first checking to make sure the size is zero and there aren't any pre-existing attributes (which are unsupported for now). */
    if (((uint16_t *) stun_pkt)[1] != (uint16_t) htons(0)) return false;
    ((uint16_t *) stun_pkt)[1] = (uint16_t) htons(8); //Update the length to 8, the length of the response-port attribute

    /* The first 16 bits are the attribute type. Attribute type 0x0027 is response-port type.
       The second 16 bits are the attribute length of the value section. This is 4 for a response-port.
       The next 16 bits are the port that we want the response to be sent to.
       The final 16 bits are ignored padding. */
    ((uint16_t *) stun_pkt)[10] = htons(0x0027); //Set the attribute type response-port
    ((uint16_t *) stun_pkt)[11] = htons(2); //Attribute length for response-port
    ((uint16_t *) stun_pkt)[12] = htons(port);
    ((uint16_t *) stun_pkt)[13] = htons(0); //The padding

    return true;
}

void *UdpStun_generate_stun_response(struct sockaddr_in *stun_addr, int *len, const QString &transaction_id) {
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
    ((uint16_t *) stun_pkt)[1] = (uint16_t) htons(12); //Skip the 20 byte header + 4 byte mapped address header + 8 byte mapped address
    ((uint32_t *) stun_pkt)[1] = (uint32_t) htonl(0x2112A442); //Magic cookie
    /* Transaction ID */
    bool ok;
    ((uint32_t *) stun_pkt)[2] = (uint32_t) htonl(transaction_id.left(8).toLong(&ok, 16));
    ((uint32_t *) stun_pkt)[3] = (uint32_t) htonl(transaction_id.mid(8, 8).toLong(&ok, 16));
    ((uint32_t *) stun_pkt)[4] = (uint32_t) htonl(transaction_id.right(8).toLong(&ok, 16));

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
