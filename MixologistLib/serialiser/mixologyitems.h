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

/* The various types of MixologyItem packets. */
const uint8_t PKT_SUBTYPE_LM_REQUEST = 0x01;
const uint8_t PKT_SUBTYPE_LM_RESPONSE = 0x02;
const uint8_t PKT_SUBTYPE_LM_SUGGESTION = 0x03;
const uint8_t PKT_SUBTYPE_LM_RETURN = 0x04;
const uint8_t PKT_SUBTYPE_LM_LENDING = 0x05;

class MixologyItem: public NetItem {
public:
    MixologyItem(uint8_t subtype) :NetItem(PKT_VERSION_SERVICE,SERVICE_TYPE_MIX,subtype) {}
    virtual ~MixologyItem() {}

    virtual void clear() {}
    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0) = 0;
    virtual bool serialise(void *data,uint32_t &size) = 0;
    virtual uint32_t serial_size() = 0;
};

/* For requesting an item from a friend's LibraryMixer library. */
class MixologyRequest: public MixologyItem {
public:
    MixologyRequest() :MixologyItem(PKT_SUBTYPE_LM_REQUEST) {}
    MixologyRequest(void *data,uint32_t size); // deserialization

    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size);
    virtual uint32_t serial_size();

    uint32_t item_id;
};

/* Abstract class for MixologyItems involving lists of files. */
class MixologyFileItem: public MixologyItem {
public:
    MixologyFileItem(uint8_t subtype) :MixologyItem(subtype) {}

    /* Returns true if there is a title and all of the synchronized lists look well-formed. */
    bool checkWellFormed() const;

    QStringList files() const;
    void files(const QStringList &newFiles);
    QStringList hashes() const;
    void hashes(const QStringList &newHashes);
    QList<qlonglong> filesizes() const;
    void filesizes(const QList<qlonglong> &newFilesizes);

protected:
    //Used in file auto responses, these items are synchronized with the subject, in that item X of each refers to the same item.
    QString storedFiles; //List of file names separated by \n, without a trailing \n
    QString storedHashes; //List of hashes seperated by \n, without a trailing \n
    QString storedFilesizes; //List of filesizes seperated by \n, without a trailing \n
};


/* For responding to a request from the LibraryMixer library. */
class MixologyResponse: public MixologyFileItem {
public:
    MixologyResponse() :MixologyFileItem(PKT_SUBTYPE_LM_RESPONSE) {}
    MixologyResponse(void *data,uint32_t size); // deserialization

    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size);
    virtual uint32_t serial_size();

    uint32_t item_id;

    /* Accessors for the message for when ITEM_STATUS_MATCHED_TO_MESSAGE.
       Cannot be used at the same as any of the file fields. */
    QString message() const;
    void message(const QString newMessage);

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

/* For suggesting a set of files for download. */
class MixologySuggestion: public MixologyFileItem {
public:
    MixologySuggestion() :MixologyFileItem(PKT_SUBTYPE_LM_SUGGESTION) {}
    MixologySuggestion(void *data,uint32_t size); // deserialization

    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size);
    virtual uint32_t serial_size();

    QString title; //Title for this whole batch of files being suggested.

    enum flagStates {
        OFFER_SEND = 0x01,
        OFFER_LEND = 0x02
    };

    uint16_t flags;
};

/* Offer to send files in return of something that was borrowed. */
class MixologyReturn: public MixologyFileItem {
public:
    MixologyReturn() :MixologyFileItem(PKT_SUBTYPE_LM_RETURN) {}
    MixologyReturn(void *data,uint32_t size); // deserialization

    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size);
    virtual uint32_t serial_size();

    uint16_t source_type;
    QString source_id;
};

/* Transfer complete packet for use in borrowing/lending. */
const uint16_t TRANSFER_COMPLETE_BORROWED = 0x01;
const uint16_t TRANSFER_COMPLETE_RETURNED = 0x02;

class MixologyLending: public MixologyItem {
public:
    MixologyLending() :MixologyItem(PKT_SUBTYPE_LM_LENDING) {}
    MixologyLending(void *data,uint32_t size); // deserialization

    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size);
    virtual uint32_t serial_size();

    uint16_t flags;
    uint16_t source_type;
    QString source_id;
};

class MixologySerialiser: public SerialType {
public:
    MixologySerialiser() :SerialType(PKT_VERSION_SERVICE, SERVICE_TYPE_MIX) {}

    virtual uint32_t    size (NetItem *item) {
        return static_cast<MixologyItem *>(item)->serial_size();
    }
    virtual bool serialise(NetItem *item, void *data, uint32_t *size) {
        return static_cast<MixologyItem *>(item)->serialise(data,*size);
    }
    virtual NetItem *deserialise (void *data, uint32_t *size);
};

/**************************************************************************/

#endif /* LIBRARY_MIXER_ITEMS_H */
