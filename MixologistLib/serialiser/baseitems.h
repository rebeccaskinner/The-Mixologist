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

#ifndef BASE_ITEMS_H
#define BASE_ITEMS_H

#include <map>

#include "serialiser/serial.h"
#include "serialiser/tlvtypes.h"

const uint8_t PKT_TYPE_FILE          = 0x01;

const uint8_t PKT_SUBTYPE_FI_REQUEST  = 0x01;
const uint8_t PKT_SUBTYPE_FI_DATA     = 0x02;
const uint8_t PKT_SUBTYPE_FI_TRANSFER = 0x03;


/**************************************************************************/

class FileRequest: public NetItem {
public:
    FileRequest()
        :NetItem(PKT_VERSION1, PKT_CLASS_BASE,
                 PKT_TYPE_FILE,
                 PKT_SUBTYPE_FI_REQUEST) {
        return;
    }
    virtual ~FileRequest();
    virtual void clear();
    std::ostream &print(std::ostream &out, uint16_t indent = 0);

    uint64_t fileoffset;  /* start of data requested */
    uint32_t chunksize;   /* size of data requested */
    TlvFileItem file;   /* file information */
};

/**************************************************************************/

class FileData: public NetItem {
public:
    FileData()
        :NetItem(PKT_VERSION1, PKT_CLASS_BASE,
                 PKT_TYPE_FILE,
                 PKT_SUBTYPE_FI_DATA) {
        return;
    }
    virtual ~FileData();
    virtual void clear();
    std::ostream &print(std::ostream &out, uint16_t indent = 0);

    TlvFileData fd;
};

/**************************************************************************/

class FileItemSerialiser: public SerialType {
public:
    FileItemSerialiser()
        :SerialType(PKT_VERSION1, PKT_CLASS_BASE,
                    PKT_TYPE_FILE) {
        return;
    }
    virtual     ~FileItemSerialiser() {
        return;
    }

    virtual uint32_t    size(NetItem *);
    virtual bool        serialise  (NetItem *item, void *data, uint32_t *size);
    virtual NetItem     *deserialise(void *data, uint32_t *size);

private:

    /* sub types */
    virtual uint32_t    sizeReq(FileRequest *);
    virtual bool        serialiseReq (FileRequest *item, void *data, uint32_t *size);
    virtual FileRequest   *deserialiseReq(void *data, uint32_t *size);

    virtual uint32_t    sizeData(FileData *);
    virtual bool        serialiseData (FileData *item, void *data, uint32_t *size);
    virtual FileData   *deserialiseData(void *data, uint32_t *size);

};

class ServiceSerialiser: public SerialType {
public:
    ServiceSerialiser()
        :SerialType(PKT_VERSION_SERVICE, 0, 0) {
        return;
    }
    virtual     ~ServiceSerialiser() {
        return;
    }

    virtual uint32_t    size(NetItem *);
    virtual bool        serialise  (NetItem *item, void *data, uint32_t *size);
    virtual NetItem     *deserialise(void *data, uint32_t *size);

};

/**************************************************************************/

#endif

