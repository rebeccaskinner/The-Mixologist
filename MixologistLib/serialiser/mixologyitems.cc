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
#include <QStringList>

/**************************** MixologyFileItem **************************/

bool MixologyFileItem::checkWellFormed() const {
    int fileCount = files().count();
    int hashCount = hashes().count();
    int sizeCount = filesizes().count();
    if (fileCount != hashCount ||
        fileCount != sizeCount) return false;
    if (fileCount < 1) return false;
    if (files().contains("")) return false;
    if (hashes().contains("")) return false;
    return true;
}

QStringList MixologyFileItem::files() const {
    return storedFiles.split("\n");
}

void MixologyFileItem::files(const QStringList &newFiles) {
    storedFiles.clear();
    int finalFile = newFiles.count() - 1;
    for (int i = 0; i <= finalFile; i++) {
        storedFiles.append(newFiles.at(i));
        if (i != finalFile) storedFiles.append("\n");
    }
}

QStringList MixologyFileItem::hashes() const {
    return storedHashes.split("\n");
}

void MixologyFileItem::hashes(const QStringList &newHashes) {
    storedHashes.clear();
    int finalHash = newHashes.count() - 1;
    for (int i = 0; i <= finalHash; i++) {
        storedHashes.append(newHashes.at(i));
        if (i != finalHash) storedHashes.append("\n");
    }
}

QList<qlonglong> MixologyFileItem::filesizes() const {
    QStringList stringFilesizes = storedFilesizes.split("\n");
    QList<qlonglong> intFilesizes;
    QString test;
    for(int i = 0; i < stringFilesizes.count(); i++) {
        intFilesizes.append(stringFilesizes.at(i).toLongLong());
    }
    return intFilesizes;
}

void MixologyFileItem::filesizes(const QList<qlonglong> &newFilesizes) {
    storedFilesizes.clear();
    int finalFilesize = newFilesizes.count() - 1;
    for (int i = 0; i <= finalFilesize; i++) {
        storedFilesizes.append(QString::number(newFilesizes.at(i)));
        if (i != finalFilesize) storedFilesizes.append("\n");
    }
}

/**************************** MixologyRequest ***************************/
MixologyRequest::MixologyRequest(void *data,uint32_t size)
    : MixologyItem(PKT_SUBTYPE_LM_REQUEST) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true;

    ok &= getRawUInt32(data, rssize, &offset, &item_id);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl;
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
    uint32_t tlvsize = serial_size();
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
    : MixologyFileItem(PKT_SUBTYPE_LM_RESPONSE) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true;

    ok &= getRawUInt32(data, rssize, &offset, &item_id);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, storedFiles);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, storedHashes);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, storedFilesizes);
    ok &= getRawUInt16(data, rssize, &offset, &itemStatus);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl;
}

std::ostream &MixologyResponse::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "MixologyResponse", indent);
    uint16_t int_Indent = indent + 2;

    printIndent(out, int_Indent);
    out << "item_id " << item_id << std::endl;

    printIndent(out, int_Indent);

    printIndent(out, int_Indent);
    out << "files:  " << storedFiles.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "hashes:  " << storedHashes.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "filesizes:  " << storedFilesizes.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "itemStatus:  " << itemStatus << std::endl;

    printNetItemEnd(out, "MixologyResponse", indent);
    return out;
}

bool MixologyResponse::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size();
    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    uint32_t offset = 8;

    ok &= setRawInt32(data, tlvsize, &offset, item_id);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, storedFiles);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, storedHashes);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, storedFilesizes);
    ok &= setRawUInt16(data, tlvsize, &offset, itemStatus);

    if (offset != tlvsize) ok = false;
    return ok;
}

uint32_t MixologyResponse::serial_size() {
    uint32_t size = 8; /* header */
    size += 4; /* item_id */
    size += GetTlvQStringSize(storedFiles); /* subject */
    size += GetTlvQStringSize(storedHashes); /*hashes */
    size += GetTlvQStringSize(storedFilesizes); /* filesizes */
    size += 2; /* itemStatus */

    return size;
}

/* The message is actually stored in storedFiles.
   We can do this because storedFiles will never be used at the same time as message. */
QString MixologyResponse::message() const {
    return storedFiles;
}

void MixologyResponse::message(QString newMessage) {
    storedFiles = newMessage;
}

/**************************** MixologySuggestion ***************************/
MixologySuggestion::MixologySuggestion(void *data,uint32_t size)
    : MixologyFileItem(PKT_SUBTYPE_LM_SUGGESTION) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true;

    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, title);
    ok &= getRawUInt16(data, rssize, &offset, &flags);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, storedFiles);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, storedHashes);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, storedFilesizes);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl;
}

std::ostream &MixologySuggestion::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "MixologySuggestion", indent);
    uint16_t int_Indent = indent + 2;

    printIndent(out, int_Indent);
    out << "title:  " << title.toStdString()  << std::endl;

    printIndent(out, int_Indent);
    out << "files:  " << storedFiles.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "hashes:  " << storedHashes.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "filesizes:  " << storedFilesizes.toStdString() << std::endl;

    printNetItemEnd(out, "MixologySuggestion", indent);
    return out;
}

bool MixologySuggestion::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size();
    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    uint32_t offset = 8;

    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, title);
    ok &= setRawUInt16(data, tlvsize, &offset, flags);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, storedFiles);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, storedHashes);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, storedFilesizes);

    if (offset != tlvsize) ok = false;
    return ok;
}

uint32_t MixologySuggestion::serial_size() {
    uint32_t size = 8; /* header */
    size += GetTlvQStringSize(title);
    size += 2; /* flags */
    size += GetTlvQStringSize(storedFiles);
    size += GetTlvQStringSize(storedHashes);
    size += GetTlvQStringSize(storedFilesizes);

    return size;
}

/**************************** MixologyReturn ***************************/
MixologyReturn::MixologyReturn(void *data,uint32_t size)
    : MixologyFileItem(PKT_SUBTYPE_LM_RETURN) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true;

    ok &= getRawUInt16(data, rssize, &offset, &source_type);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, source_id);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, storedFiles);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, storedHashes);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, storedFilesizes);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl;
}

std::ostream &MixologyReturn::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "MixologyReturn", indent);
    uint16_t int_Indent = indent + 2;

    printIndent(out, int_Indent);
    out << "source_type" << source_type << std::endl;
    out << "source_id " << source_id.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "files:  " << storedFiles.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "hashes:  " << storedHashes.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "filesizes:  " << storedFilesizes.toStdString() << std::endl;

    printNetItemEnd(out, "MixologyReturn", indent);
    return out;
}

bool MixologyReturn::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size();
    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    uint32_t offset = 8;

    ok &= setRawUInt16(data, tlvsize, &offset, source_type);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, source_id);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, storedFiles);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, storedHashes);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, storedFilesizes);

    if (offset != tlvsize) ok = false;
    return ok;
}

uint32_t MixologyReturn::serial_size() {
    uint32_t size = 8; /* header */
    size += 2; /* source_type */
    size += GetTlvQStringSize(source_id); /* source_id */
    size += GetTlvQStringSize(storedFiles);
    size += GetTlvQStringSize(storedHashes);
    size += GetTlvQStringSize(storedFilesizes);

    return size;
}

/**************************** MixologyLending ***************************/
MixologyLending::MixologyLending(void *data,uint32_t size)
    : MixologyItem(PKT_SUBTYPE_LM_LENDING) {
    (void)size;

    uint32_t offset = 8; // skip the header
    uint32_t rssize = getNetItemSize(data);
    bool ok = true;

    ok &= getRawUInt16(data, rssize, &offset, &flags);
    ok &= getRawUInt16(data, rssize, &offset, &source_type);
    ok &= GetTlvQString(data, rssize, &offset, TLV_TYPE_STR_MSG, source_id);

    if (offset != rssize)
        std::cerr << "Size error while deserializing." << std::endl;
    if (!ok)
        std::cerr << "Unknown error while deserializing." << std::endl;
}

std::ostream &MixologyLending::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "MixologyLending", indent);
    uint16_t int_Indent = indent + 2;

    printIndent(out, int_Indent);
    out << "source_type" << source_type << std::endl;
    out << "source_id " << source_id.toStdString() << std::endl;

    printIndent(out, int_Indent);
    out << "flags:  " << flags << std::endl;

    printNetItemEnd(out, "MixologyLending", indent);
    return out;
}

bool MixologyLending::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size();
    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    /* skip the header */
    uint32_t offset = 8;

    ok &= setRawUInt16(data, tlvsize, &offset, flags);
    ok &= setRawUInt16(data, tlvsize, &offset, source_type);
    ok &= SetTlvQString(data, tlvsize, &offset, TLV_TYPE_STR_MSG, source_id);

    if (offset != tlvsize) ok = false;
    return ok;
}

uint32_t MixologyLending::serial_size() {
    uint32_t size = 8; /* header */
    size += 2; /* flags */
    size += 2; /* source_type */
    size += GetTlvQStringSize(source_id); /* source_id */

    return size;
}

/****************************Serialiser*********************************/
NetItem *MixologySerialiser::deserialise(void *data, uint32_t *pktsize) {
    uint32_t rstype = getNetItemId(data);
    uint32_t rssize = getNetItemSize(data);

#ifdef DEBUG_LIBRARY_MIXER_ITEMS
    std::cerr << "deserializing packet..."<< std::endl;
#endif
    // look what we have...
    if (*pktsize < rssize) {  /* check size */
#ifdef DEBUG_LIBRARY_MIXER_ITEMS
        std::cerr << "Mixology deserialisation: not enough size: pktsize=" << *pktsize << ", rssize=" << rssize << std::endl;
#endif
        return NULL; /* not enough data */
    }

    /* set the packet length */
    *pktsize = rssize;

    /* ready to load */

    if ((PKT_VERSION_SERVICE != getNetItemVersion(rstype)) || (SERVICE_TYPE_MIX != getNetItemService(rstype))) {
#ifdef DEBUG_LIBRARY_MIXER_ITEMS
        std::cerr << "Mixology deserialisation: wrong type !" << std::endl;
#endif
        return NULL; /* wrong type */
    }

    switch (getNetItemSubType(rstype)) {
        case PKT_SUBTYPE_LM_REQUEST:
            return new MixologyRequest(data,*pktsize);
        case PKT_SUBTYPE_LM_RESPONSE:
            return new MixologyResponse(data,*pktsize);
        case PKT_SUBTYPE_LM_SUGGESTION:
            return new MixologySuggestion(data, *pktsize);
        case PKT_SUBTYPE_LM_RETURN:
            return new MixologyReturn(data, *pktsize);
        case PKT_SUBTYPE_LM_LENDING:
            return new MixologyLending(data, *pktsize);
        default:
#ifdef DEBUG_LIBRARY_MIXER_ITEMS
            std::cerr << "Unknown packet type in mixology!" << std::endl;
#endif
            return NULL;
    }
}
