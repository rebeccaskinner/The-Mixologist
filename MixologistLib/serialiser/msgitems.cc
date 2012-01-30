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

#include <stdexcept>
#include "serialiser/baseserial.h"
#include "serialiser/msgitems.h"
#include "serialiser/tlvbase.h"

/***
#define SERIAL_DEBUG 1
***/

#include <iostream>

/*************************************************************************/

std::ostream &ChatMsgItem::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "ChatMsgItem", indent);
    uint16_t int_Indent = indent + 2;
    printIndent(out, int_Indent);
    out << "QblogMs " << chatFlags << std::endl;

    printIndent(out, int_Indent);
    out << "sendTime:  " << sendTime  << std::endl;

    printIndent(out, int_Indent);

    out << "msg:  " << message.toStdString() << std::endl;

    printNetItemEnd(out, "ChatMsgItem", indent);
    return out;
}

std::ostream &ChatStatusItem::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "ChatStatusItem", indent);
    uint16_t int_Indent = indent + 2;
    printIndent(out, int_Indent);
    out << "Status string: " << status_string.toStdString() << std::endl;

    printNetItemEnd(out, "ChatStatusItem", indent);
    return out;
}

std::ostream &ChatAvatarItem::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "ChatAvatarItem", indent);
    uint16_t int_Indent = indent + 2;
    printIndent(out, int_Indent);
    out << "Image size: " << image_size << std::endl;
    printNetItemEnd(out, "ChatStatusItem", indent);

    return out;
}
NetItem *ChatSerialiser::deserialise(void *data, uint32_t *pktsize) {
    uint32_t rstype = getNetItemId(data);
    uint32_t rssize = getNetItemSize(data);

#ifdef CHAT_DEBUG
    std::cerr << "deserializing packet..."<< std::endl ;
#endif
    // look what we have...
    if (*pktsize < rssize) {  /* check size */
#ifdef CHAT_DEBUG
        std::cerr << "chat deserialisation: not enough size: pktsize=" << *pktsize << ", rssize=" << rssize << std::endl ;
#endif
        return NULL; /* not enough data */
    }

    /* set the packet length */
    *pktsize = rssize;

    /* ready to load */

    if ((PKT_VERSION_SERVICE != getNetItemVersion(rstype)) || (SERVICE_TYPE_CHAT != getNetItemService(rstype))) {
#ifdef CHAT_DEBUG
        std::cerr << "chat deserialisation: wrong type !" << std::endl ;
#endif
        return NULL; /* wrong type */
    }

    switch (getNetItemSubType(rstype)) {
        case PKT_SUBTYPE_DEFAULT:
            return new ChatMsgItem(data,*pktsize);
        case PKT_SUBTYPE_CHAT_STATUS:
            return new ChatStatusItem(data,*pktsize);
        case PKT_SUBTYPE_CHAT_AVATAR:
            return new ChatAvatarItem(data,*pktsize);
        default:
            std::cerr << "Unknown packet type in chat!" << std::endl;
            return NULL;
    }
}

uint32_t ChatMsgItem::serial_size() {
    uint32_t s = 8; /* header */
    s += 4; /* chatFlags */
    s += 4; /* sendTime  */
    s += GetTlvQStringSize(message);
    return s;
}

uint32_t ChatStatusItem::serial_size() {
    uint32_t s = 8; /* header */
    s += GetTlvQStringSize(status_string);

    return s;
}

uint32_t ChatAvatarItem::serial_size() {
    uint32_t s = 8; /* header */
    s += 4 ;                        // size
    s += image_size ;           // data

    return s;
}

ChatAvatarItem::~ChatAvatarItem() {
    free(image_data) ;
    image_data = NULL ;
}

/* serialise the data to the buffer */
bool ChatMsgItem::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size() ;
    uint32_t offset = 0;

    if (pktsize < tlvsize)
        return false; /* not enough space */

    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

#ifdef CHAT_DEBUG
    std::cerr << "ChatSerialiser::serialiseItem() Header: " << ok << std::endl;
    std::cerr << "ChatSerialiser::serialiseItem() Size: " << tlvsize << std::endl;
#endif

    /* skip the header */
    offset += 8;

    /* add mandatory parts first */
    ok &= setRawUInt32(data, tlvsize, &offset, chatFlags);
    ok &= setRawUInt32(data, tlvsize, &offset, sendTime);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_QSTR_MSG, message);

    if (offset != tlvsize) {
        ok = false;
#ifdef CHAT_DEBUG
        std::cerr << "ChatSerialiser::serialiseItem() Size Error! " << std::endl;
#endif
    }
#ifdef CHAT_DEBUG
    std::cerr << "computed size: " << 256*((unsigned char *)data)[6]+((unsigned char *)data)[7] << std::endl ;
#endif

    return ok;
}

bool ChatStatusItem::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size() ;
    uint32_t offset = 0;

    if (pktsize < tlvsize)
        return false; /* not enough space */

    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

#ifdef CHAT_DEBUG
    std::cerr << "ChatSerialiser serialising chat status item." << std::endl;
    std::cerr << "ChatSerialiser::serialiseItem() Header: " << ok << std::endl;
    std::cerr << "ChatSerialiser::serialiseItem() Size: " << tlvsize << std::endl;
#endif

    /* skip the header */
    offset += 8;

    /* add mandatory parts first */
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_QSTR_MSG, status_string);

    if (offset != tlvsize) {
        ok = false;
#ifdef CHAT_DEBUG
        std::cerr << "ChatSerialiser::serialiseItem() Size Error! " << std::endl;
#endif
    }
#ifdef CHAT_DEBUG
    std::cerr << "computed size: " << 256*((unsigned char *)data)[6]+((unsigned char *)data)[7] << std::endl ;
#endif

    return ok;
}

bool ChatAvatarItem::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size() ;
    uint32_t offset = 0;

    if (pktsize < tlvsize)
        return false; /* not enough space */

    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

#ifdef CHAT_DEBUG
    std::cerr << "ChatSerialiser serialising chat avatar item." << std::endl;
    std::cerr << "ChatSerialiser::serialiseItem() Header: " << ok << std::endl;
    std::cerr << "ChatSerialiser::serialiseItem() Size: " << tlvsize << std::endl;
#endif

    /* skip the header */
    offset += 8;

    /* add mandatory parts first */
    ok &= setRawUInt32(data, tlvsize, &offset,image_size);

    memcpy((void *)( (unsigned char *)data + offset),image_data,image_size) ;
    offset += image_size ;

    if (offset != tlvsize) {
        ok = false;
#ifdef CHAT_DEBUG
        std::cerr << "ChatSerialiser::serialiseItem() Size Error! " << std::endl;
#endif
    }
#ifdef CHAT_DEBUG
    std::cerr << "computed size: " << 256*((unsigned char *)data)[6]+((unsigned char *)data)[7] << std::endl ;
#endif

    return ok;
}
ChatMsgItem::ChatMsgItem(void *data,uint32_t size)
    : ChatItem(PKT_SUBTYPE_DEFAULT) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true ;

    /* get mandatory parts first */
    ok &= getRawUInt32(data, rssize, &offset, &chatFlags);
    ok &= getRawUInt32(data, rssize, &offset, &sendTime);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_QSTR_MSG, message);

#ifdef CHAT_DEBUG
    std::cerr << "Building new chat msg item." << std::endl ;
#endif
    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl ;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl ;
}

ChatStatusItem::ChatStatusItem(void *data,uint32_t size)
    : ChatItem(PKT_SUBTYPE_CHAT_STATUS) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true ;

#ifdef CHAT_DEBUG
    std::cerr << "Building new chat status item." << std::endl ;
#endif
    /* get mandatory parts first */
    ok &= GetTlvQString(data, rssize, &offset,TLV_TYPE_QSTR_MSG, status_string);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl ;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl ;
}

ChatAvatarItem::ChatAvatarItem(void *data,uint32_t size)
    : ChatItem(PKT_SUBTYPE_CHAT_STATUS) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true ;

#ifdef CHAT_DEBUG
    std::cerr << "Building new chat status item." << std::endl ;
#endif
    /* get mandatory parts first */
    ok &= getRawUInt32(data, rssize, &offset,&image_size);

    image_data = (unsigned char *)malloc(image_size*sizeof(unsigned char)) ;
    memcpy(image_data,(void *)((unsigned char *)data+offset),image_size) ;
    offset += image_size ;

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl ;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl ;
}
