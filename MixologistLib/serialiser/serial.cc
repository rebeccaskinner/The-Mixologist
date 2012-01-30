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

#include "serialiser/baseserial.h"
#include "serialiser/serial.h"

#include <map>
#include <iostream>

/***
#define SERIAL_DEBUG 1
***/

#ifdef SERIAL_DEBUG
#include <sstream>
#endif

NetItem::NetItem(uint32_t t)
    :type(t) {
    return;
}

NetItem::NetItem(uint8_t ver, uint8_t cls, uint8_t t, uint8_t subtype) {
    type = (ver << 24) + (cls << 16) + (t << 8) + subtype;
    return;
}

NetItem::~NetItem() {
    return;
}

std::string NetItem::PeerId() {
    return peerId;
}
void NetItem::PeerId(std::string id) {
    peerId = id;
}

uint32_t    NetItem::PacketId() {
    return type;
}

uint8_t    NetItem::PacketVersion() {
    return (type >> 24);
}


uint8_t    NetItem::PacketClass() {
    return (type >> 16) & 0xFF;
}


uint8_t    NetItem::PacketType() {
    return (type >> 8) & 0xFF;
}


uint8_t    NetItem::PacketSubType() {
    return (type & 0xFF);
}


/* For Service Packets */
NetItem::NetItem(uint8_t ver, uint16_t service, uint8_t subtype) {
    type = (ver << 24) + (service << 8) + subtype;
    return;
}

uint16_t    NetItem::PacketService() {
    return (type >> 8) & 0xFFFF;
}



SerialType::SerialType(uint32_t t)
    :type(t & 0xFFFFFF00) {
    return;
}

SerialType::SerialType(uint8_t ver, uint8_t cls, uint8_t t) {
    type = (ver << 24) + (cls << 16) + (t << 8);
    return;
}

SerialType::SerialType(uint8_t ver, uint16_t service) {
    type = (ver << 24) + (service << 8);
    return;
}

SerialType::~SerialType() {
    return;
}

uint32_t    SerialType::size(NetItem *) {
#ifdef  SERIAL_DEBUG
    std::cerr << "SerialType::size()" << std::endl;
#endif

    /* base size: type + length */
    return 8;
}

bool        SerialType::serialise(NetItem *, void *, uint32_t *) {
#ifdef  SERIAL_DEBUG
    std::cerr << "SerialType::serialise()" << std::endl;
#endif
    return false;
}

NetItem     *SerialType::deserialise(void *, uint32_t *) {
#ifdef  SERIAL_DEBUG
    std::cerr << "SerialType::deserialise()" << std::endl;
#endif
    return NULL;
}

uint32_t    SerialType::PacketId() {
    return type;
}




Serialiser::Serialiser() {
    return;
}


Serialiser::~Serialiser() {
    /* clean up the map */
    std::map<uint32_t, SerialType *>::iterator it;
    for (it = serialisers.begin(); it != serialisers.end(); it++) {
        delete (it->second);
    }
    serialisers.clear();
    return;
}



bool        Serialiser::addSerialType(SerialType *serialiser) {
    uint32_t type = (serialiser->PacketId() & 0xFFFFFF00);
    std::map<uint32_t, SerialType *>::iterator it;
    if (serialisers.end() != (it = serialisers.find(type))) {
#ifdef  SERIAL_DEBUG
        std::cerr << "Serialiser::addSerialType() Error Serialiser already exists!";
        std::cerr << std::endl;
#endif
        return false;
    }

    serialisers[type] = serialiser;
    return true;
}



uint32_t    Serialiser::size(NetItem *item) {
    /* find the type */
    uint32_t type = (item->PacketId() & 0xFFFFFF00);
    std::map<uint32_t, SerialType *>::iterator it;

    if (serialisers.end() == (it = serialisers.find(type))) {
        /* remove 8 more bits -> try again */
        type &= 0xFFFF0000;
        if (serialisers.end() == (it = serialisers.find(type))) {
            /* one more try */
            type &= 0xFF000000;
            if (serialisers.end() == (it = serialisers.find(type))) {

#ifdef  SERIAL_DEBUG
                std::cerr << "Serialiser::size() serialiser missing!";

                std::ostringstream out;
                out << std::hex << item->PacketId();

                std::cerr << "Serialiser::size() PacketId: ";
                std::cerr << out.str();
                std::cerr << std::endl;
#endif
                return 0;
            }
        }
    }

#ifdef  SERIAL_DEBUG
    std::ostringstream out;
    out << std::hex << "Serialiser::size() Item->PacketId(): " << item->PacketId();
    out << " matched to Serialiser Type: " << type;
    std::cerr << out.str() << std::endl;
#endif

    return (it->second)->size(item);
}

bool        Serialiser::serialise  (NetItem *item, void *data, uint32_t *size) {
    /* find the type */
    uint32_t type = (item->PacketId() & 0xFFFFFF00);
    std::map<uint32_t, SerialType *>::iterator it;

    if (serialisers.end() == (it = serialisers.find(type))) {
        /* remove 8 more bits -> try again */
        type &= 0xFFFF0000;
        if (serialisers.end() == (it = serialisers.find(type))) {
            /* one more try */
            type &= 0xFF000000;
            if (serialisers.end() == (it = serialisers.find(type))) {

#ifdef  SERIAL_DEBUG
                std::cerr << "Serialiser::serialise() serialiser missing!";
                std::ostringstream out;
                out << std::hex << item->PacketId();

                std::cerr << "Serialiser::serialise() PacketId: ";
                std::cerr << out.str();
                std::cerr << std::endl;
#endif
                return false;
            }
        }
    }

#ifdef  SERIAL_DEBUG
    std::ostringstream out;
    out << std::hex << "Serialiser::serialise() Item->PacketId(): " << item->PacketId();
    out << " matched to Serialiser Type: " << type;
    std::cerr << out.str() << std::endl;
#endif

    return (it->second)->serialise(item, data, size);
}



NetItem     *Serialiser::deserialise(void *data, uint32_t *size) {
    /* find the type */
    if (*size < 8) {
#ifdef  SERIAL_DEBUG
        std::cerr << "Serialiser::deserialise() Not Enough Data(1)";
        std::cerr << std::endl;
#endif
        return NULL;
    }

    uint32_t type = (getNetItemId(data) & 0xFFFFFF00);
    uint32_t pkt_size = getNetItemSize(data);

    if (pkt_size < *size) {
#ifdef  SERIAL_DEBUG
        std::cerr << "Serialiser::deserialise() Not Enough Data(2)";
        std::cerr << std::endl;
#endif
        return NULL;
    }

    /* store the packet size to return the amount we should use up */
    *size = pkt_size;

    std::map<uint32_t, SerialType *>::iterator it;
    if (serialisers.end() == (it = serialisers.find(type))) {
        /* remove 8 more bits -> try again */
        type &= 0xFFFF0000;
        if (serialisers.end() == (it = serialisers.find(type))) {
            /* one more try */
            type &= 0xFF000000;
            if (serialisers.end() == (it = serialisers.find(type))) {

#ifdef  SERIAL_DEBUG
                std::cerr << "Serialiser::deserialise() deserialiser missing!";
                std::ostringstream out;
                out << std::hex << getNetItemId(data);

                std::cerr << "Serialiser::deserialise() PacketId: ";
                std::cerr << out.str();
                std::cerr << std::endl;
#endif
                return NULL;
            }
        }
    }

    NetItem *item = (it->second)->deserialise(data, &pkt_size);
    if (!item) {
#ifdef  SERIAL_DEBUG
        std::cerr << "Serialiser::deserialise() Failed!";
        std::cerr << std::endl;
#endif
        return NULL;
    }

    if (pkt_size != *size) {
#ifdef  SERIAL_DEBUG
        std::cerr << "Serialiser::deserialise() Warning: size mismatch!";
        std::cerr << std::endl;
#endif
    }
    return item;
}


bool   setNetItemHeader(void *data, uint32_t size, uint32_t type, uint32_t pktsize) {
    if (size < 8)
        return false;

    uint32_t offset = 0;
    bool ok = true;
    ok &= setRawUInt32(data, 8, &offset, type);
    ok &= setRawUInt32(data, 8, &offset, pktsize);

    return ok;
}



uint32_t getNetItemId(void *data) {
    uint32_t type = 0;
    uint32_t offset = 0;
    getRawUInt32(data, 4, &offset, &type);
    return type;
}


uint32_t getNetItemSize(void *data) {
    uint32_t size = 0;
    uint32_t offset = 4;
    getRawUInt32(data, 8, &offset, &size);
    return size;
}

uint8_t  getNetItemVersion(uint32_t type) {
    return (type >> 24);
}

uint8_t  getNetItemClass(uint32_t type) {
    return (type >> 16) & 0xFF;
}

uint8_t  getNetItemType(uint32_t type) {
    return (type >> 8) & 0xFF;
}

uint8_t  getNetItemSubType(uint32_t type) {
    return (type & 0xFF);
}

uint16_t  getNetItemService(uint32_t type) {
    return (type >> 8) & 0xFFFF;
}


std::ostream &printNetItemBase(std::ostream &out, std::string clsName, uint16_t indent) {
    printIndent(out, indent);
    out << "NetItem: " << clsName << " ####################################";
    out << std::endl;
    return out;
}

std::ostream &printNetItemEnd(std::ostream &out, std::string clsName, uint16_t indent) {
    printIndent(out, indent);
    out << "###################### " << clsName << " #####################";
    out << std::endl;
    return out;
}

std::ostream &RawItem::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "RawItem", indent);
    printIndent(out, indent);
    out << "Size: " << len << std::endl;
    printNetItemEnd(out, "RawItem", indent);
    return out;
}


uint32_t getPktMaxSize() {
    //return 65535; /* 2^16 (old artifical low size) */
    //return 1048575; /* 2^20 -1 (Too Big! - must remove fixed static buffers first) */
    /* Remember that every pqistreamer allocates an input buffer of this size!
     * So don't make it too big!
     */
    return 262143; /* 2^18 -1 */
}


uint32_t getPktBaseSize() {
    return 8; /* 4 + 4 */
}

