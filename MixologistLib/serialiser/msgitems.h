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

#ifndef MSG_ITEMS_H
#define MSG_ITEMS_H

#include <map>

#include "serialiser/serviceids.h"
#include "serialiser/serial.h"
#include "serialiser/tlvtypes.h"

#include <QString>

/**************************************************************************/

/* chat Flags */
const uint32_t CHAT_FLAG_PRIVATE            = 0x0001;
const uint32_t CHAT_FLAG_REQUESTS_AVATAR    = 0x0002;
const uint32_t CHAT_FLAG_CONTAINS_AVATAR    = 0x0004;
const uint32_t CHAT_FLAG_AVATAR_AVAILABLE = 0x0008;

const uint8_t PKT_SUBTYPE_CHAT_STATUS = 0x02 ;  // default is 0x01
const uint8_t PKT_SUBTYPE_CHAT_AVATAR = 0x03 ;  // default is 0x01

class ChatItem: public NetItem {
public:
    ChatItem(uint8_t chat_subtype) : NetItem(PKT_VERSION_SERVICE,SERVICE_TYPE_CHAT,chat_subtype) {}

    virtual ~ChatItem() {}
    virtual void clear() {}
    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0) = 0 ;

    virtual bool serialise(void *data,uint32_t &size) = 0 ; // Isn't it better that items can serialize themselves ?
    virtual uint32_t serial_size() = 0 ;                            // deserialise is handled using a constructor
};


class ChatMsgItem: public ChatItem {
public:
    ChatMsgItem() :ChatItem(PKT_SUBTYPE_DEFAULT) {}
    ChatMsgItem(void *data,uint32_t size) ; // deserialization

    virtual ~ChatMsgItem() {}
    virtual void clear() {}
    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);

    virtual bool serialise(void *data,uint32_t &size) ; // Isn't it better that items can serialize themselves ?
    virtual uint32_t serial_size() ;                            // deserialise is handled using a constructor

    uint32_t chatFlags;
    uint32_t sendTime;
    //std::wstring message;
    QString message;
    /* not serialised */
    uint32_t recvTime;
};

// This class contains activity info for the sending peer: active, idle, typing, etc.
//
class ChatStatusItem: public ChatItem {
public:
    ChatStatusItem() :ChatItem(PKT_SUBTYPE_CHAT_STATUS) {}
    ChatStatusItem(void *data,uint32_t size) ; // deserialization

    virtual ~ChatStatusItem() {}
    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);

    virtual bool serialise(void *data,uint32_t &size) ; // Isn't it better that items can serialize themselves ?
    virtual uint32_t serial_size() ;                            // deserialise is handled using a constructor

    QString status_string;
};

// This class contains avatar images in Qt format.
//
class ChatAvatarItem: public ChatItem {
public:
    ChatAvatarItem() :ChatItem(PKT_SUBTYPE_CHAT_AVATAR) {}
    ChatAvatarItem(void *data,uint32_t size) ; // deserialization

    virtual ~ChatAvatarItem() ;
    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);

    virtual bool serialise(void *data,uint32_t &size) ; // Isn't it better that items can serialize themselves ?
    virtual uint32_t serial_size() ;                            // deserialise is handled using a constructor

    uint32_t image_size ;               // size of data in bytes
    unsigned char *image_data ;     // image
};


class ChatSerialiser: public SerialType {
public:
    ChatSerialiser() :SerialType(PKT_VERSION_SERVICE, SERVICE_TYPE_CHAT) {}

    virtual uint32_t    size (NetItem *item) {
        return static_cast<ChatItem *>(item)->serial_size() ;
    }
    virtual bool serialise(NetItem *item, void *data, uint32_t *size) {
        return static_cast<ChatItem *>(item)->serialise(data,*size) ;
    }
    virtual NetItem *deserialise (void *data, uint32_t *size) ;
};

/**************************************************************************/

#endif /* MSG_ITEMS_H */


