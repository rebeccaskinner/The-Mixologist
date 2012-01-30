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

#include <serialiser/statusitems.h>
#include <iostream>
#include <serialiser/baseserial.h>

/**************************** BasicStatusItem ***************************/

BasicStatusItem::BasicStatusItem(void *data, uint32_t /*size*/)
    :StatusItem(PKT_SUBTYPE_BASIC_STATUS) {

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true ;

    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, offLMXmlHash);
    ok &= getRawUInt64(data, rssize, &offset, &offLMXmlSize);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl ;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl ;
}

std::ostream &BasicStatusItem::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "BasicStatusItem", indent);
    printNetItemEnd(out, "BasicStatusItem", indent);
    return out;
}

bool BasicStatusItem::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size();
    uint32_t offset = 0;

    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    offset += 8;

    /* add mandatory parts first */
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, offLMXmlHash);
    ok &= setRawUInt64(data, tlvsize, &offset, offLMXmlSize);

    if (offset != tlvsize) {
        ok = false;
    }

    return ok;
}

uint32_t BasicStatusItem::serial_size() {
    uint32_t size = 8; /* header */
    size += GetTlvQStringSize(offLMXmlHash);
    size += 8; /* offLMXmlSize */
    return size;
}

/**************************** OnConnectStatusItem ***************************/

OnConnectStatusItem::OnConnectStatusItem(void *data, uint32_t /*size*/)
    :StatusItem(PKT_SUBTYPE_ON_CONNECT) {

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true ;

    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, offLMXmlHash);
    ok &= getRawUInt64(data, rssize, &offset, &offLMXmlSize);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, clientName);
    ok &= getRawUInt64(data, rssize, &offset, &clientVersion);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl ;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl ;
}

std::ostream &OnConnectStatusItem::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "OnConnectStatusItem", indent);
    printNetItemEnd(out, "OnConnectStatusItem", indent);
    return out;
}

bool OnConnectStatusItem::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size();
    uint32_t offset = 0;

    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    offset += 8;

    /* add mandatory parts first */
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, offLMXmlHash);
    ok &= setRawUInt64(data, tlvsize, &offset, offLMXmlSize);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, clientName);
    ok &= setRawUInt64(data, tlvsize, &offset, clientVersion);

    if (offset != tlvsize) {
        ok = false;
    }

    return ok;
}

uint32_t OnConnectStatusItem::serial_size() {
    uint32_t size = 8; /* header */
    size += GetTlvQStringSize(offLMXmlHash);
    size += 8; /* offLMXmlSize */
    size += GetTlvQStringSize(clientName);
    size += 8; /* clientVersion */
    return size;
}
/****************************Serialiser*********************************/
NetItem *StatusSerialiser::deserialise(void *data, uint32_t *pktsize) {
    uint32_t rstype = getNetItemId(data);
    uint32_t rssize = getNetItemSize(data);

    // look what we have...
    if (*pktsize < rssize) {  /* check size */
        return NULL; /* not enough data */
    }

    /* set the packet length */
    *pktsize = rssize;

    /* ready to load */

    if ((PKT_VERSION_SERVICE != getNetItemVersion(rstype)) || (SERVICE_TYPE_STATUS != getNetItemService(rstype))) {
        return NULL; /* wrong type */
    }

    switch (getNetItemSubType(rstype)) {
        case PKT_SUBTYPE_BASIC_STATUS:
            return new BasicStatusItem(data, *pktsize);
        case PKT_SUBTYPE_ON_CONNECT:
            return new OnConnectStatusItem(data, *pktsize);
        default:
            std::cerr << "Unknown packet type in status!" << std::endl;
            return NULL;
    }
}
