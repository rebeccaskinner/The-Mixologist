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

#ifndef BASE_SERIALISER_H
#define BASE_SERIALISER_H

#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <iosfwd>
#include <stdlib.h>
#include <stdint.h>

/*******************************************************************
 * This is the Top-Level serialiser/deserialise,
 *
 * Data is Serialised into the following format
 *
 * -----------------------------------------
 * |    TYPE (4 bytes) | Size (4 bytes)    |
 * -----------------------------------------
 * |                                       |
 * |         Data ....                     |
 * |                                       |
 * -----------------------------------------
 *
 * Size is the total size of the packet (including the 8 byte header)
 * Type is composed of:
 *
 * 8 bits: Version (0x01)
 * 8 bits: Class
 * 8 bits: Type
 * 8 bits: SubType
 ******************************************************************/

const uint8_t PKT_VERSION1        = 0x01;
const uint8_t PKT_VERSION_SERVICE = 0x02;

const uint8_t PKT_CLASS_BASE      = 0x01;
const uint8_t PKT_CLASS_CONFIG    = 0x02;

const uint8_t PKT_SUBTYPE_DEFAULT = 0x01; /* if only one subtype */


class NetItem {
public:
    NetItem(uint32_t t);
    NetItem(uint8_t ver, uint8_t cls, uint8_t t, uint8_t subtype);

    virtual ~NetItem();
    virtual void clear() = 0;
    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0) = 0;

    /* source / destination id */
    std::string PeerId() {
        return peerId;
    }
    void  PeerId(std::string id) {
        peerId = id;
    }

    /* complete id */
    uint32_t PacketId();

    /* id parts */
    uint8_t  PacketVersion();
    uint8_t  PacketClass();
    uint8_t  PacketType();
    uint8_t  PacketSubType();

    /* For Service Packets */
    NetItem(uint8_t ver, uint16_t service, uint8_t subtype);
    uint16_t  PacketService(); /* combined Packet class/type (mid 16bits) */

private:
    uint32_t type;
    std::string peerId;
};


class SerialType {
public:
    SerialType(uint32_t t); /* only uses top 24bits */
    SerialType(uint8_t ver, uint8_t cls, uint8_t t);
    SerialType(uint8_t ver, uint16_t service);

    virtual     ~SerialType();

    virtual uint32_t    size(NetItem *);
    virtual bool        serialise  (NetItem *item, void *data, uint32_t *size);
    virtual NetItem     *deserialise(void *data, uint32_t *size);

    uint32_t    PacketId();
protected:
    uint32_t type;
};


class Serialiser {
public:
    Serialiser();
    ~Serialiser();
    bool        addSerialType(SerialType *type);

    uint32_t    size(NetItem *);
    bool        serialise  (NetItem *item, void *data, uint32_t *size);
    NetItem     *deserialise(void *data, uint32_t *size);


private:
    std::map<uint32_t, SerialType *> serialisers;
};

bool     setNetItemHeader(void *data, uint32_t size, uint32_t type, uint32_t pktsize);

/* Extract Header Information from Packet */
uint32_t getNetItemId(void *data);
uint32_t getNetItemSize(void *data);

uint8_t  getNetItemVersion(uint32_t type);
uint8_t  getNetItemClass(uint32_t type);
uint8_t  getNetItemType(uint32_t type);
uint8_t  getNetItemSubType(uint32_t type);

uint16_t  getNetItemService(uint32_t type);

/* size constants */
uint32_t getPktBaseSize();
uint32_t getPktMaxSize();



/* helper fns for printing */
std::ostream &printNetItemBase(std::ostream &o, std::string n, uint16_t i);
std::ostream &printNetItemEnd(std::ostream &o, std::string n, uint16_t i);

/* defined in tlvtypes.cc - redeclared here for ease */
std::ostream &printIndent(std::ostream &out, uint16_t indent);
/* Wrapper class for data that is serialised somewhere else */

class RawItem: public NetItem {
public:
    RawItem(uint32_t t, uint32_t size)
        :NetItem(t), len(size) {
        data = malloc(len);
    }

    virtual ~RawItem() {
        if (data)
            free(data);
        data = NULL;
        len = 0;
    }

    uint32_t    getRawLength() {
        return len;
    }
    void       *getRawData()   {
        return data;
    }

    virtual void clear() {
        return;    /* what can it do? */
    }
    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);

private:
    void *data;
    uint32_t len;
};



#endif /* BASE_SERIALISER_H */
