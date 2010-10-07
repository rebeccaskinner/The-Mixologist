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
#include "serialiser/baseitems.h"

/***
#define SERIAL_DEBUG 1
***/

#include <iostream>

/*************************************************************************/

uint32_t    FileItemSerialiser::size(NetItem *i) {
    FileRequest *rfr;
    FileData    *rfd;

    if (NULL != (rfr = dynamic_cast<FileRequest *>(i))) {
        return sizeReq(rfr);
    } else if (NULL != (rfd = dynamic_cast<FileData *>(i))) {
        return sizeData(rfd);
    }

    return 0;
}

/* serialise the data to the buffer */
bool    FileItemSerialiser::serialise(NetItem *i, void *data, uint32_t *pktsize) {
    FileRequest *rfr;
    FileData    *rfd;

    if (NULL != (rfr = dynamic_cast<FileRequest *>(i))) {
        return serialiseReq(rfr, data, pktsize);
    } else if (NULL != (rfd = dynamic_cast<FileData *>(i))) {
        return serialiseData(rfd, data, pktsize);
    }

    return false;
}

NetItem *FileItemSerialiser::deserialise(void *data, uint32_t *pktsize) {
    /* get the type and size */
    uint32_t rstype = getNetItemId(data);

    if ((PKT_VERSION1 != getNetItemVersion(rstype)) ||
            (PKT_CLASS_BASE != getNetItemClass(rstype)) ||
            (PKT_TYPE_FILE != getNetItemType(rstype))) {
        return NULL; /* wrong type */
    }

    switch (getNetItemSubType(rstype)) {
        case PKT_SUBTYPE_FI_REQUEST:
            return deserialiseReq(data, pktsize);
            break;
        case PKT_SUBTYPE_FI_DATA:
            return deserialiseData(data, pktsize);
            break;
        default:
            return NULL;
            break;
    }
    return NULL;
}

/*************************************************************************/

FileRequest::~FileRequest() {
    return;
}

void    FileRequest::clear() {
    file.TlvClear();
    fileoffset = 0;
    chunksize  = 0;
}

std::ostream &FileRequest::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "FileRequest", indent);
    uint16_t int_Indent = indent + 2;
    printIndent(out, int_Indent);
    out << "FileOffset: " << fileoffset << std::endl;
    out << "ChunkSize:  " << chunksize  << std::endl;
    file.print(out, int_Indent);
    printNetItemEnd(out, "FileRequest", indent);
    return out;
}


uint32_t    FileItemSerialiser::sizeReq(FileRequest *item) {
    uint32_t s = 8; /* header */
    s += 8; /* offset */
    s += 4; /* chunksize */
    s += item->file.TlvSize();

    return s;
}

/* serialise the data to the buffer */
bool     FileItemSerialiser::serialiseReq(FileRequest *item, void *data, uint32_t *pktsize) {
    uint32_t tlvsize = sizeReq(item);
    uint32_t offset = 0;

    if (*pktsize < tlvsize)
        return false; /* not enough space */

    *pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, item->PacketId(), tlvsize);

#ifdef SERIAL_DEBUG
    std::cerr << "FileItemSerialiser::serialiseReq() Header: " << ok << std::endl;
    std::cerr << "FileItemSerialiser::serialiseReq() Size: " << tlvsize << std::endl;
#endif

    /* skip the header */
    offset += 8;

    /* add mandatory parts first */
    ok &= setRawUInt64(data, tlvsize, &offset, item->fileoffset);
    ok &= setRawUInt32(data, tlvsize, &offset, item->chunksize);
    ok &= item->file.SetTlv(data, tlvsize, &offset);

    if (offset != tlvsize) {
        ok = false;
#ifdef SERIAL_DEBUG
        std::cerr << "FileItemSerialiser::serialiseReq() Size Error! " << std::endl;
#endif
    }

    return ok;
}

FileRequest *FileItemSerialiser::deserialiseReq(void *data, uint32_t *pktsize) {
    /* get the type and size */
    uint32_t rstype = getNetItemId(data);
    uint32_t rssize = getNetItemSize(data);

    uint32_t offset = 0;


    if ((PKT_VERSION1 != getNetItemVersion(rstype)) ||
            (PKT_CLASS_BASE != getNetItemClass(rstype)) ||
            (PKT_TYPE_FILE  != getNetItemType(rstype)) ||
            (PKT_SUBTYPE_FI_REQUEST != getNetItemSubType(rstype))) {
        return NULL; /* wrong type */
    }

    if (*pktsize < rssize)    /* check size */
        return NULL; /* not enough data */

    /* set the packet length */
    *pktsize = rssize;

    bool ok = true;

    /* ready to load */
    FileRequest *item = new FileRequest();
    item->clear();

    /* skip the header */
    offset += 8;

    /* get mandatory parts first */
    ok &= getRawUInt64(data, rssize, &offset, &(item->fileoffset));
    ok &= getRawUInt32(data, rssize, &offset, &(item->chunksize));
    ok &= item->file.GetTlv(data, rssize, &offset);

    if (offset != rssize) {
        /* error */
        delete item;
        return NULL;
    }

    if (!ok) {
        delete item;
        return NULL;
    }

    return item;
}


/*************************************************************************/

FileData::~FileData() {
    return;
}

void    FileData::clear() {
    fd.TlvClear();
}

std::ostream &FileData::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "FileData", indent);
    uint16_t int_Indent = indent + 2;
    printIndent(out, int_Indent);
    fd.print(out, int_Indent);
    printNetItemEnd(out, "FileData", indent);
    return out;
}


uint32_t    FileItemSerialiser::sizeData(FileData *item) {
    uint32_t s = 8; /* header  */
    s += item->fd.TlvSize();

    return s;
}

/* serialise the data to the buffer */
bool     FileItemSerialiser::serialiseData(FileData *item, void *data, uint32_t *pktsize) {
    uint32_t tlvsize = sizeData(item);
    uint32_t offset = 0;

    if (*pktsize < tlvsize)
        return false; /* not enough space */

    *pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, item->PacketId(), tlvsize);

#ifdef SERIAL_DEBUG
    std::cerr << "FileItemSerialiser::serialiseData() Header: " << ok << std::endl;
#endif

    /* skip the header */
    offset += 8;

    /* add mandatory parts first */
    ok &= item->fd.SetTlv(data, tlvsize, &offset);

    if (offset != tlvsize) {
        ok = false;
#ifdef SERIAL_DEBUG
        std::cerr << "FileItemSerialiser::serialiseData() Size Error! " << std::endl;
#endif
    }

    return ok;
}

FileData *FileItemSerialiser::deserialiseData(void *data, uint32_t *pktsize) {
    /* get the type and size */
    uint32_t rstype = getNetItemId(data);
    uint32_t rssize = getNetItemSize(data);

    uint32_t offset = 0;

    if ((PKT_VERSION1 != getNetItemVersion(rstype)) ||
            (PKT_CLASS_BASE != getNetItemClass(rstype)) ||
            (PKT_TYPE_FILE  != getNetItemType(rstype)) ||
            (PKT_SUBTYPE_FI_DATA != getNetItemSubType(rstype))) {
        return NULL; /* wrong type */
    }

    if (*pktsize < rssize)    /* check size */
        return NULL; /* not enough data */

    /* set the packet length */
    *pktsize = rssize;

    bool ok = true;

    /* ready to load */
    FileData *item = new FileData();
    item->clear();

    /* skip the header */
    offset += 8;

    /* get mandatory parts first */
    ok &= item->fd.GetTlv(data, rssize, &offset);

    if (offset != rssize) {
        /* error */
        delete item;
        return NULL;
    }

    if (!ok) {
        delete item;
        return NULL;
    }

    return item;
}


/*************************************************************************/
/*************************************************************************/

uint32_t    ServiceSerialiser::size(NetItem *i) {
    RawItem *item = dynamic_cast<RawItem *>(i);

    if (item) {
        return item->getRawLength();
    }
    return 0;
}

/* serialise the data to the buffer */
bool    ServiceSerialiser::serialise(NetItem *i, void *data, uint32_t *pktsize) {
    RawItem *item = dynamic_cast<RawItem *>(i);
    if (!item) {
        return false;
    }

    uint32_t tlvsize = item->getRawLength();

    if (*pktsize < tlvsize)
        return false; /* not enough space */

    if (tlvsize > getPktMaxSize())
        return false; /* packet too big */

    *pktsize = tlvsize;

    /* its serialised already!!! */
    memcpy(data, item->getRawData(), tlvsize);

    return true;
}

NetItem *ServiceSerialiser::deserialise(void *data, uint32_t *pktsize) {
    /* get the type and size */
    uint32_t rstype = getNetItemId(data);
    uint32_t rssize = getNetItemSize(data);

    if (PKT_VERSION_SERVICE != getNetItemVersion(rstype)) {
        return NULL; /* wrong type */
    }

    if (*pktsize < rssize)    /* check size */
        return NULL; /* not enough data */

    if (rssize > getPktMaxSize())
        return NULL; /* packet too big */

    /* set the packet length */
    *pktsize = rssize;

    RawItem *item = new RawItem(rstype, rssize);
    void *item_data = item->getRawData();

    memcpy(item_data, data, rssize);

    return item;
}


