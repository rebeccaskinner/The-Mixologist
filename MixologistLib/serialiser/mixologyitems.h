/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
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


#ifndef LIBRARY_MIXER_ITEMS_H
#define LIBRARY_MIXER_ITEMS_H

#include "serialiser/serial.h"
#include "serialiser/serviceids.h"
#include "serialiser/tlvbase.h"

const uint8_t PKT_SUBTYPE_LM_REQUEST = 0x01 ;
const uint8_t PKT_SUBTYPE_LM_RESPONSE = 0x02 ;
const uint8_t PKT_SUBTYPE_LM_SUGGESTION = 0x03 ;
const uint8_t PKT_SUBTYPE_LM_LENDING = 0x04 ;

class MixologyItem: public NetItem {
public:
    MixologyItem(uint8_t subtype) : NetItem(PKT_VERSION_SERVICE,SERVICE_TYPE_MIX,subtype) {}
    virtual ~MixologyItem() {}

    virtual void clear() {}
    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0) = 0 ;
    virtual bool serialise(void *data,uint32_t &size) = 0;
    virtual uint32_t serial_size() = 0;

    int32_t item_id;
};

class MixologyRequest: public MixologyItem {
public:
    MixologyRequest() :MixologyItem(PKT_SUBTYPE_LM_REQUEST) {}
    MixologyRequest(void *data,uint32_t size) ; // deserialization

    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size) ;
    virtual uint32_t serial_size() ;
};

class MixologyResponse: public MixologyItem {
public:
    MixologyResponse() :MixologyItem(PKT_SUBTYPE_LM_RESPONSE) {}
    MixologyResponse(void *data,uint32_t size) ; // deserialization

    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size);
    virtual uint32_t serial_size();

    //For a message auto response, this contains the message
    //For a file auto response, this contains a list of file names seperated by \n
    QString subject;
    //Used in file auto responses, these items are synchronized with the subject, in that item X of each refers to the same item.
    std::string hashes; //List of hashes seperated by \n
    std::string filesizes; //List of filesizes seperated by \n

    uint16_t itemStatus;

    enum itemStatuses {
        ITEM_STATUS_MATCHED_TO_MESSAGE,
        ITEM_STATUS_MATCHED_TO_FILE,
        ITEM_STATUS_MATCHED_TO_LEND,
        ITEM_STATUS_MATCHED_TO_LENT,
        ITEM_STATUS_NO_SUCH_ITEM,
        ITEM_STATUS_UNMATCHED,
        ITEM_STATUS_BROKEN_MATCH,
        ITEM_STATUS_CHAT,
        ITEM_STATUS_INTERNAL_ERROR
    };
};

class MixologySuggestion: public MixologyItem {
public:
    MixologySuggestion() :MixologyItem(PKT_SUBTYPE_LM_SUGGESTION) {}
    MixologySuggestion(void *data,uint32_t size) ; // deserialization

    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size) ;
    virtual uint32_t serial_size() ;

    QString title; //Title for this whole batch of files being suggested.
};

const uint16_t TRANSFER_COMPLETE_BORROWED = 0x01 ;
const uint16_t TRANSFER_COMPLETE_RETURNED = 0x02 ;

class MixologyLending: public MixologyItem {
public:
    MixologyLending() :MixologyItem(PKT_SUBTYPE_LM_LENDING) {}
    MixologyLending(void *data,uint32_t size) ; // deserialization

    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size) ;
    virtual uint32_t serial_size() ;

    uint16_t flags;
};

class MixologySerialiser: public SerialType {
public:
    MixologySerialiser() :SerialType(PKT_VERSION_SERVICE, SERVICE_TYPE_MIX) {}

    virtual uint32_t    size (NetItem *item) {
        return static_cast<MixologyItem *>(item)->serial_size() ;
    }
    virtual bool serialise(NetItem *item, void *data, uint32_t *size) {
        return static_cast<MixologyItem *>(item)->serialise(data,*size) ;
    }
    virtual NetItem *deserialise (void *data, uint32_t *size) ;
};

/**************************************************************************/

#endif /* LIBRARY_MIXER_ITEMS_H */
