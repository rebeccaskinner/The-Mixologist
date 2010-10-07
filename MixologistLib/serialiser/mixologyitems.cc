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


/***
 * #DEFINE DEBUG_LIBRARY_MIXER_ITEMS
 **/

#include <serialiser/mixologyitems.h>
#include <iostream>
#include <serialiser/baseserial.h>


/**************************** MixologyRequest ***************************/
MixologyRequest::MixologyRequest(void *data,uint32_t size)
    : MixologyItem(PKT_SUBTYPE_LM_REQUEST) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true ;

    ok &= getRawInt32(data, rssize, &offset, &item_id);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl ;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl ;
}

std::ostream &MixologyRequest::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "MixologyRequest", indent);
    uint16_t int_Indent = indent + 2;

    printIndent(out, int_Indent);
    out << "item_id " << item_id << std::endl;

    printNetItemEnd(out, "MixologyRequest", indent);
    return out;
}

bool MixologyRequest::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size() ;
    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    uint32_t offset = 8;

    ok &= setRawInt32(data, tlvsize, &offset, item_id);

    if (offset != tlvsize) ok = false;
    return ok;
}

uint32_t MixologyRequest::serial_size() {
    uint32_t size = 8; /* header */
    size += 4; /* item_id */

    return size;
}

/**************************** MixologyResponse ***************************/
MixologyResponse::MixologyResponse(void *data,uint32_t size)
    : MixologyItem(PKT_SUBTYPE_LM_RESPONSE) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true ;

    ok &= getRawInt32(data, rssize, &offset, &item_id);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, subject);
    ok &= GetTlvString(data, rssize, &offset, TLV_TYPE_STR_MSG, hashes);
    ok &= GetTlvString(data, rssize, &offset, TLV_TYPE_STR_MSG, filesizes);
    ok &= getRawUInt16(data, rssize, &offset, &itemStatus);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl ;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl ;
}

std::ostream &MixologyResponse::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "MixologyResponse", indent);
    uint16_t int_Indent = indent + 2;

    printIndent(out, int_Indent);
    out << "item_id " << item_id << std::endl;

    printIndent(out, int_Indent);

    printIndent(out, int_Indent);
    out << "subject:  " << subject.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "hashes:  " << hashes  << std::endl;

    printIndent(out, int_Indent);
    out << "filesizes:  " << filesizes  << std::endl;

    printNetItemEnd(out, "MixologyResponse", indent);
    return out;
}

bool MixologyResponse::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size() ;
    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    uint32_t offset = 8;

    ok &= setRawInt32(data, tlvsize, &offset, item_id);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, subject);
    ok &= SetTlvString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, hashes);
    ok &= SetTlvString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, filesizes);
    ok &= setRawUInt16(data, tlvsize, &offset, itemStatus);

    if (offset != tlvsize) ok = false;
    return ok;
}

uint32_t MixologyResponse::serial_size() {
    uint32_t size = 8; /* header */
    size += 4; /* item_id */
    size += GetTlvQStringSize(subject); /* subject */
    size += GetTlvStringSize(hashes); /*hashes */
    size += GetTlvStringSize(filesizes); /* filesizes */
    size += 2; /* itemStatus */

    return size;
}

/**************************** MixologySuggestion ***************************/
MixologySuggestion::MixologySuggestion(void *data,uint32_t size)
    : MixologyItem(PKT_SUBTYPE_LM_SUGGESTION) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true ;

    ok &= getRawInt32(data, rssize, &offset, &item_id);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, title);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl ;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl ;
}

std::ostream &MixologySuggestion::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "MixologySuggestion", indent);
    uint16_t int_Indent = indent + 2;

    printIndent(out, int_Indent);
    out << "item_id " << item_id << std::endl;

    printIndent(out, int_Indent);
    out << "title:  " << title.toStdString()  << std::endl;

    printNetItemEnd(out, "MixologySuggestion", indent);
    return out;
}

bool MixologySuggestion::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size() ;
    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    uint32_t offset = 8;

    ok &= setRawInt32(data, tlvsize, &offset, item_id);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, title);

    if (offset != tlvsize) ok = false;
    return ok;
}

uint32_t MixologySuggestion::serial_size() {
    uint32_t size = 8; /* header */
    size += 4; /* item_id */
    size += GetTlvQStringSize(title); /* title */

    return size;
}

/**************************** MixologyLending ***************************/
MixologyLending::MixologyLending(void *data,uint32_t size)
    : MixologyItem(PKT_SUBTYPE_LM_LENDING) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true ;

    ok &= getRawInt32(data, rssize, &offset, &item_id);
    ok &= getRawUInt16(data, rssize, &offset, &flags);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl ;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl ;
}

std::ostream &MixologyLending::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "MixologyLending", indent);
    uint16_t int_Indent = indent + 2;

    printIndent(out, int_Indent);
    out << "item_id " << item_id << std::endl;

    printIndent(out, int_Indent);
    out << "flags:  " << flags << std::endl;

    printNetItemEnd(out, "MixologyLending", indent);
    return out;
}

bool MixologyLending::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size() ;
    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    uint32_t offset = 8;

    ok &= setRawInt32(data, tlvsize, &offset, item_id);
    ok &= setRawUInt16(data, tlvsize, &offset, flags);

    if (offset != tlvsize) ok = false;
    return ok;
}

uint32_t MixologyLending::serial_size() {
    uint32_t size = 8; /* header */
    size += 4; /* item_id */
    size += 2; /* flags */

    return size;
}

/****************************Serialiser*********************************/
NetItem *MixologySerialiser::deserialise(void *data, uint32_t *pktsize) {
    uint32_t rstype = getNetItemId(data);
    uint32_t rssize = getNetItemSize(data);

#ifdef DEBUG_LIBRARY_MIXER_ITEMS
    std::cerr << "deserializing packet..."<< std::endl ;
#endif
    // look what we have...
    if (*pktsize < rssize) {  /* check size */
#ifdef DEBUG_LIBRARY_MIXER_ITEMS
        std::cerr << "Mixology deserialisation: not enough size: pktsize=" << *pktsize << ", rssize=" << rssize << std::endl ;
#endif
        return NULL; /* not enough data */
    }

    /* set the packet length */
    *pktsize = rssize;

    /* ready to load */

    if ((PKT_VERSION_SERVICE != getNetItemVersion(rstype)) || (SERVICE_TYPE_MIX != getNetItemService(rstype))) {
#ifdef DEBUG_LIBRARY_MIXER_ITEMS
        std::cerr << "Mixology deserialisation: wrong type !" << std::endl ;
#endif
        return NULL; /* wrong type */
    }

    switch (getNetItemSubType(rstype)) {
        case PKT_SUBTYPE_LM_REQUEST:
            return new MixologyRequest(data,*pktsize) ;
        case PKT_SUBTYPE_LM_RESPONSE:
            return new MixologyResponse(data,*pktsize) ;
        case PKT_SUBTYPE_LM_SUGGESTION:
            return new MixologySuggestion(data, *pktsize) ;
        case PKT_SUBTYPE_LM_LENDING:
            return new MixologyLending(data, *pktsize) ;
        default:
#ifdef DEBUG_LIBRARY_MIXER_ITEMS
            std::cerr << "Unknown packet type in mixology!" << std::endl ;
#endif
            return NULL ;
    }
}
