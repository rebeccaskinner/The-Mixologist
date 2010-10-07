/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-8, Robert Fernie, Chris Parker
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

#include "tlvbase.h"
#include "tlvtypes.h"
#include "baseserial.h"
#include "util/print.h"
#include <ostream>
#include <sstream>
#include <iomanip>
#include <iostream>

void    TlvItem::TlvShallowClear() {
    TlvClear(); /* unless overloaded! */
}

std::ostream &TlvItem::printBase(std::ostream &out, std::string clsName, uint16_t indent) {
    printIndent(out, indent);
    out << "TlvItem: " << clsName << " Size: " << TlvSize() << "  ***********************";
    out << std::endl;
    return out;
}

std::ostream &TlvItem::printEnd(std::ostream &out, std::string clsName, uint16_t indent) {
    printIndent(out, indent);
    out << "********************** " << clsName << " *********************";
    out << std::endl;
    return out;
}

std::ostream &printIndent(std::ostream &out, uint16_t indent) {
    for (int i = 0; i < indent; i++) {
        out << " ";
    }
    return out;
}



/*!********************************** TlvFileBinaryData **********************************/


TlvBinaryData::TlvBinaryData(uint16_t t)
    :tlvtype(t), bin_len(0), bin_data(NULL) {
    return;
}

TlvBinaryData::~TlvBinaryData() {
    TlvClear();
}


/// used to allocate memory andinitialize binary data member
bool     TlvBinaryData::setBinData(void *data, uint16_t size) {
    /* ready to load */
    TlvClear();

    /* get mandatory */
    /* the rest of the TLV size binary data */
    bin_len = size;
    if (bin_len == 0) {
        bin_data = NULL;
        return true;
    }

    bin_data = malloc(bin_len);
    memcpy(bin_data, data, bin_len);
    return true;
}

void TlvBinaryData::TlvClear() {
    if (bin_data) {
        free(bin_data);
    }
    TlvShallowClear();
}

void    TlvBinaryData::TlvShallowClear() {
    bin_data = NULL;
    bin_len = 0;
}

uint16_t TlvBinaryData::TlvSize() {
    uint32_t s = 4; /* header */

    if (bin_data != NULL)
        s += bin_len; // len is the size of data

    return s;
}


bool     TlvBinaryData::SetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    /* must check sizes */
    uint16_t tlvsize = TlvSize();
    uint32_t tlvend  = *offset + tlvsize;

    if (size < tlvend)
        return false; /* not enough space */

    bool ok = true;

    /* start at data[offset] */
    ok &= SetTlvBase(data, tlvend, offset, tlvtype, tlvsize);

    /* add mandatory data */
    if ((bin_data != NULL) && (bin_len)) {
        memcpy(&(((uint8_t *) data)[*offset]), bin_data, bin_len);
        *offset += bin_len;
    }
    return ok;
}



bool     TlvBinaryData::GetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    if (size < *offset + 4) {
        return false; /* not enough space to get the header */
    }

    uint16_t tlvtype_in = GetTlvType( &(((uint8_t *) data)[*offset])  );
    uint16_t tlvsize = GetTlvSize( &(((uint8_t *) data)[*offset])  );
    uint32_t tlvend = *offset + tlvsize;

    if (size < tlvend)    /* check size */
        return false; /* not enough space */

    if (tlvsize < 4) {
        return false; /* bad tlv size */
    }

    if (tlvtype != tlvtype_in) /* check type */
        return false;

    /* skip the header */
    (*offset) += 4;

    bool ok = setBinData(&(((uint8_t *) data)[*offset]), tlvsize - 4);
    (*offset) += bin_len;

    return ok;
}

std::ostream &TlvBinaryData::print(std::ostream &out, uint16_t indent) {
    uint16_t int_Indent = indent + 2;

    uint32_t i;
    std::ostringstream sout;
    printIndent(sout, indent);
    sout << "TlvBinaryData: Type: " << tlvtype << " Size: " << bin_len;
    sout << std::hex;
    for (i = 0; i < bin_len; i++) {
        if (i % 16 == 0) {
            sout << std::endl;
            printIndent(sout, int_Indent);
        }
        sout << std::setw(2) << std::setfill('0')
             << (int) (((unsigned char *) bin_data)[i]) << ":";
    }
    sout << std::endl;
    out << sout.str();

    printEnd(out, "TlvBinaryData", indent);
    return out;

}

/************************************* Peer Id Set ************************************/

void TlvPeerIdSet::TlvClear() {
    ids.clear();

}

uint16_t TlvPeerIdSet::TlvSize() {

    uint32_t s = 4; /* header */

    /* determine the total size of ids strings in list */

    std::list<std::string>::iterator it;

    for (it = ids.begin(); it != ids.end() ; ++it) {
        if (it->length() > 0)
            s += GetTlvStringSize(*it);
    }

    return s;
}


bool     TlvPeerIdSet::SetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    /* must check sizes */
    uint16_t tlvsize = TlvSize();
    uint32_t tlvend  = *offset + tlvsize;

    if (size < tlvend)
        return false; /* not enough space */

    bool ok = true;


    /* start at data[offset] */
    ok &= SetTlvBase(data, tlvend, offset, TLV_TYPE_PEERSET , tlvsize);

    /* determine the total size of ids strings in list */

    std::list<std::string>::iterator it;

    for (it = ids.begin(); it != ids.end() ; ++it) {
        if (it->length() > 0)
            ok &= SetTlvString(data, tlvend, offset, TLV_TYPE_STR_PEERID, *it);
    }

    return ok;

}


bool     TlvPeerIdSet::GetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    if (size < *offset + 4)
        return false;

    uint16_t tlvtype = GetTlvType( &(((uint8_t *) data)[*offset])  );
    uint16_t tlvsize = GetTlvSize( &(((uint8_t *) data)[*offset])  );
    uint32_t tlvend = *offset + tlvsize;



    if (size < tlvend)    /* check size */
        return false; /* not enough space */

    if (tlvtype != TLV_TYPE_PEERSET) /* check type */
        return false;

    bool ok = true;

    /* ready to load */
    TlvClear();

    /* skip the header */
    (*offset) += 4;



    /* while there is TLV  */
    while ((*offset) + 2 < tlvend) {
        /* get the next type */
        uint16_t tlvsubtype = GetTlvType( &(((uint8_t *) data)[*offset]) );

        if (tlvsubtype == TLV_TYPE_STR_PEERID) {
            std::string newIds;
            ok &= GetTlvString(data, tlvend, offset, TLV_TYPE_STR_PEERID, newIds);
            if (ok) {
                ids.push_back(newIds);

            }
        }

        if (!ok) {
            return false;
        }
    }

    return ok;
}

/// print to screen TlvPeerIdSet contents
std::ostream &TlvPeerIdSet::print(std::ostream &out, uint16_t indent) {
    printBase(out, "TlvPeerIdSet", indent);
    uint16_t int_Indent = indent + 2;

    std::list<std::string>::iterator it;
    for (it = ids.begin(); it != ids.end() ; ++it) {
        printIndent(out, int_Indent);
        out << "id:" << *it;
        out << std::endl;
    }

    printEnd(out, "TlvPeerIdSet", indent);
    return out;

}

/// print to screen TlvPeerIdSet contents
std::ostream &TlvPeerIdSet::printHex(std::ostream &out, uint16_t indent) {
    printBase(out, "TlvPeerIdSet", indent);
    uint16_t int_Indent = indent + 2;

    std::list<std::string>::iterator it;
    for (it = ids.begin(); it != ids.end() ; ++it) {
        printIndent(out, int_Indent);
        out << "id: 0x" << Util::BinToHex(*it);
        out << std::endl;
    }

    printEnd(out, "TlvPeerIdSet", indent);
    return out;

}


/************************************* Service Id Set ************************************/

void TlvServiceIdSet::TlvClear() {
    ids.clear();

}

uint16_t TlvServiceIdSet::TlvSize() {
    uint32_t s = 4; /* header */

    /* determine the total size of ids strings in list */
    std::list<uint32_t>::iterator it;
    for (it = ids.begin(); it != ids.end() ; ++it) {
        if (*it > 0)
            s += GetTlvUInt32Size();
    }

    return s;
}

bool     TlvServiceIdSet::SetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    /* must check sizes */
    uint16_t tlvsize = TlvSize();
    uint32_t tlvend  = *offset + tlvsize;

    if (size < tlvend)
        return false; /* not enough space */

    bool ok = true;


    /* start at data[offset] */
    ok &= SetTlvBase(data, tlvend, offset, TLV_TYPE_SERVICESET , tlvsize);

    /* determine the total size of ids strings in list */
    std::list<uint32_t>::iterator it;
    for (it = ids.begin(); it != ids.end() ; ++it) {
        if (*it > 0)
            ok &= SetTlvUInt32(data, tlvend, offset, TLV_TYPE_UINT32_SERID, *it);
    }

    return ok;

}

bool     TlvServiceIdSet::GetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    if (size < *offset + 4)
        return false;

    uint16_t tlvtype = GetTlvType( &(((uint8_t *) data)[*offset])  );
    uint16_t tlvsize = GetTlvSize( &(((uint8_t *) data)[*offset])  );
    uint32_t tlvend = *offset + tlvsize;

    if (size < tlvend)    /* check size */
        return false; /* not enough space */

    if (tlvtype != TLV_TYPE_SERVICESET) /* check type */
        return false;

    bool ok = true;

    /* ready to load */
    TlvClear();

    /* skip the header */
    (*offset) += 4;

    /* while there is TLV  */
    while ((*offset) + 2 < tlvend) {
        /* get the next type */
        uint16_t tlvsubtype = GetTlvType( &(((uint8_t *) data)[*offset]) );

        if (tlvsubtype == TLV_TYPE_UINT32_SERID) {
            uint32_t newIds;
            ok &= GetTlvUInt32(data, tlvend, offset, TLV_TYPE_UINT32_SERID, &newIds);
            if (ok) {
                ids.push_back(newIds);

            }
        }

        if (!ok) {
            return false;
        }
    }

    return ok;
}

/// print to screen TlvServiceSet contents
std::ostream &TlvServiceIdSet::print(std::ostream &out, uint16_t indent) {
    printBase(out, "TlvServiceIdSet", indent);
    uint16_t int_Indent = indent + 2;

    std::list<uint32_t>::iterator it;
    for (it = ids.begin(); it != ids.end() ; ++it) {
        printIndent(out, int_Indent);
        out << "id:" << *it;
        out << std::endl;
    }

    printEnd(out, "TlvServiceIdSet", indent);
    return out;

}

/************************************* TlvKeyValue ************************************/

void TlvKeyValue::TlvClear() {
    key = "";
    value = "";
}

uint16_t TlvKeyValue::TlvSize() {
    uint32_t s = 4; /* header + 4 for size */

    /* now add comment and title length of this tlv object */

    if (key.length() > 0)
        s += GetTlvStringSize(key);
    if (value.length() > 0)
        s += GetTlvStringSize(value);

    return s;

}

bool  TlvKeyValue::SetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    /* must check sizes */
    uint16_t tlvsize = TlvSize();
    uint32_t tlvend  = *offset + tlvsize;

    if (size < tlvend)
        return false; /* not enough space */

    bool ok = true;

    /* start at data[offset] */
    ok &= SetTlvBase(data, tlvend, offset, TLV_TYPE_KEYVALUE, tlvsize);



    /* now optional ones */
    if (key.length() > 0)
        ok &= SetTlvString(data, tlvend, offset, TLV_TYPE_STR_KEY, key);  // no base tlv type for title?
    if (value.length() > 0)
        ok &= SetTlvString(data, tlvend, offset, TLV_TYPE_STR_VALUE, value); // no base tlv type for comment?

    return ok;

}

bool  TlvKeyValue::GetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    if (size < *offset + 4)
        return false;

    uint16_t tlvtype = GetTlvType( &(((uint8_t *) data)[*offset])  );
    uint16_t tlvsize = GetTlvSize( &(((uint8_t *) data)[*offset])  );
    uint32_t tlvend = *offset + tlvsize;

    if (size < tlvend)    /* check size */
        return false; /* not enough space */

    if (tlvtype != TLV_TYPE_KEYVALUE) /* check type */
        return false;

    bool ok = true;

    /* ready to load */
    TlvClear();

    /* skip the header */
    (*offset) += 4;

    /* while there is TLV  */
    while ((*offset) + 2 < tlvend) {
        /* get the next type */
        uint16_t tlvsubtype = GetTlvType( &(((uint8_t *) data)[*offset]) );

        switch (tlvsubtype) {
            case TLV_TYPE_STR_KEY:
                ok &= GetTlvString(data, tlvend, offset, TLV_TYPE_STR_KEY, key);
                break;
            case TLV_TYPE_STR_VALUE:
                ok &= GetTlvString(data, tlvend, offset,  TLV_TYPE_STR_VALUE, value);
                break;
            default:
                break;

        }

        if (!ok) {
            return false;
        }
    }

    return ok;

}

std::ostream &TlvKeyValue::print(std::ostream &out, uint16_t indent) {
    printBase(out, "TlvKeyValue", indent);
    uint16_t int_Indent = indent + 2;

    printIndent(out, int_Indent);
    out << "Key:" << key;
    printIndent(out, int_Indent);
    out << "Value:" << value;
    out << std::endl;


    printEnd(out, "TlvKeyValue", indent);
    return out;
}

/************************************* TlvKeyValueSet ************************************/

void TlvKeyValueSet::TlvClear() {
    pairs.clear(); //empty list
}

uint16_t TlvKeyValueSet::TlvSize() {

    uint32_t s = 4; /* header + 4 for size */

    std::list<TlvKeyValue>::iterator it;

    if (!pairs.empty()) {

        for (it = pairs.begin(); it != pairs.end() ; ++it)
            s += it->TlvSize();

    }

    return s;
}

bool  TlvKeyValueSet::SetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    /* must check sizes */
    uint16_t tlvsize = TlvSize();
    uint32_t tlvend  = *offset + tlvsize;

    if (size < tlvend)
        return false; /* not enough space */

    bool ok = true;

    /* start at data[offset] */
    ok &= SetTlvBase(data, tlvend, offset, TLV_TYPE_KEYVALUESET , tlvsize);


    if (!pairs.empty()) {
        std::list<TlvKeyValue>::iterator it;

        for (it = pairs.begin(); it != pairs.end() ; ++it)
            ok &= it->SetTlv(data, size, offset);
    }


    return ok;

}


bool  TlvKeyValueSet::GetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    if (size < *offset + 4)
        return false;

    uint16_t tlvtype = GetTlvType( &(((uint8_t *) data)[*offset])  );
    uint16_t tlvsize = GetTlvSize( &(((uint8_t *) data)[*offset])  );
    uint32_t tlvend = *offset + tlvsize;

    if (size < tlvend)    /* check size */
        return false; /* not enough space */

    if (tlvtype != TLV_TYPE_KEYVALUESET) /* check type */
        return false;

    bool ok = true;

    /* ready to load */
    TlvClear();

    /* skip the header */
    (*offset) += 4;

    /* while there is TLV  */
    while ((*offset) + 2 < tlvend) {
        /* get the next type */
        GetTlvType( &(((uint8_t *) data)[*offset]) );

        TlvKeyValue kv;
        ok &= kv.GetTlv(data, size, offset);
        if (ok) {
            pairs.push_back(kv);
        }

        if (!ok) {
            return false;
        }
    }

    return ok;
}

/// prints out contents of TlvKeyValueSet
std::ostream &TlvKeyValueSet::print(std::ostream &out, uint16_t indent) {
    printBase(out, "TlvKeyValue", indent);

    std::list<TlvKeyValue>::iterator it;

    for (it = pairs.begin(); it != pairs.end() ; ++it)
        it->print(out, indent);

    printEnd(out, "TlvKeyValue", indent);
    return out;
}

