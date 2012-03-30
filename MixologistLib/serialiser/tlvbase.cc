/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-8, Robert Fernie, Horatio, Chris Parker
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

#include <iostream>

#include "serialiser/tlvbase.h"
#include "serialiser/baseserial.h"

/*******************************************************************
 * These are the general TLV (un)packing routines.
 *
 * Data is Serialised into the following format
 *
 * -----------------------------------------
 * | TLV TYPE (2 bytes)| TLV LEN (2 bytes) |
 * -----------------------------------------
 * |                                       |
 * |         Data ....                     |
 * |                                       |
 * -----------------------------------------
 *
 * Size is the total size of the TLV Field (including the 4 byte header)
 *
 * Like the lowlevel packing routines. They are usually
 * created in pairs - one to pack the data, the other to unpack.
 *
 * GetTlvXXX(void *data, uint32_t size, uint32_t *offset, XXX *out);
 * SetTlvXXX(void *data, uint32_t size, uint32_t *offset, XXX *in);
 *
 *
 * data - the base pointer to the serialised data.
 * size - size of the memory pointed to by data.
 * *offset - where we want to (un)pack the data.
 *      This is incremented by the datasize.
 *
 * *in / *out - the data to (un)pack.
 *
 ******************************************************************/

//*********************

// A facility func
inline void *right_shift_void_pointer(void *p, uint32_t len) {

    return (void *)( (uint8_t *)p + len);
}
//*********************

/**
 * #define TLV_BASE_DEBUG 1
 **/


const uint32_t TYPE_FIELD_BYTES = 2;

/**** Basic TLV Functions ****/
uint16_t GetTlvSize(void *data) {
    if (!data)
        return 0;

    uint16_t len;

    void *from =right_shift_void_pointer(data, sizeof(uint16_t));

    memcpy((void *)&len, from , sizeof(uint16_t));

    len = ntohs(len);

    return len;
}

uint16_t GetTlvType(void *data) {
    if (!data)
        return 0;

    uint16_t type;

    memcpy((void *)&type, data, TYPE_FIELD_BYTES);

    type = ntohs(type);

    return type;

}

//tested
bool SetTlvBase(void *data, uint32_t size, uint32_t *offset, uint16_t type,
                uint16_t len) {
    if (!data)
        return false;
    if (!offset)
        return false;
    if (size < *offset +4)
        return false;

    uint16_t type_n = htons(type);

    //copy type_n to (data+*offset)
    void *to = right_shift_void_pointer(data, *offset);
    memcpy(to , (void *)&type_n, sizeof(uint16_t));

    uint16_t len_n =htons(len);
    //copy len_n to (data + *offset +2)
    to = right_shift_void_pointer(to, sizeof(uint16_t));
    memcpy((void *)to, (void *)&len_n, sizeof(uint16_t));

    *offset += sizeof(uint16_t)*2;

    return true;
}

//tested
bool SetTlvSize(void *data, uint32_t size, uint16_t len) {
    if (!data)
        return false;

    if (size < sizeof(uint16_t)*2 )
        return false;

    uint16_t len_n = htons(len);

    void *to = (void *)((uint8_t *) data + sizeof(uint16_t));

    memcpy(to, (void *) &len_n, sizeof(uint16_t));

    return true;

}

/**** Generic TLV Functions ****
 * This have the same data (int or string for example),
 * but they can have different types eg. a string could represent a name or a path,
 * so we include a type parameter in the arguments
 */
//tested
bool SetTlvUInt32(void *data, uint32_t size, uint32_t *offset, uint16_t type,
                  uint32_t out) {
    if (!data)
        return false;
    uint16_t tlvsize = GetTlvUInt32Size(); /* this will always be 8 bytes */
    uint32_t tlvend = *offset + tlvsize; /* where the data will extend to */
    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "SetTlvUInt32() FAILED - not enough space. (or earlier)" << std::endl;
        std::cerr << "SetTlvUInt32() size: " << size << std::endl;
        std::cerr << "SetTlvUInt32() tlvsize: " << tlvsize << std::endl;
        std::cerr << "SetTlvUInt32() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    bool ok = true;

    /* Now use the function we got to set the TlvHeader */
    /* function shifts offset to the new start for the next data */
    ok &= SetTlvBase(data, tlvend, offset, type, tlvsize);

#ifdef TLV_BASE_DEBUG
    if (!ok) {
        std::cerr << "SetTlvUInt32() SetTlvBase FAILED (or earlier)" << std::endl;
    }
#endif

    /* now set the UInt32 ( in baseserial.h???) */
    ok &= setRawUInt32(data, tlvend, offset, out);

#ifdef TLV_BASE_DEBUG
    if (!ok) {
        std::cerr << "SetTlvUInt32() setRawUInt32 FAILED (or earlier)" << std::endl;
    }
#endif


    return ok;

}

//tested
bool GetTlvUInt32(void *data, uint32_t size, uint32_t *offset,
                  uint16_t type, uint32_t *in) {
    if (!data)
        return false;

    if (size < *offset + 4)
        return false;

    /* extract the type and size */
    void *tlvstart = right_shift_void_pointer(data, *offset);
    uint16_t tlvtype = GetTlvType(tlvstart);
    uint16_t tlvsize = GetTlvSize(tlvstart);

    /* check that there is size */
    uint32_t tlvend = *offset + tlvsize;
    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvUInt32() FAILED - not enough space." << std::endl;
        std::cerr << "GetTlvUInt32() size: " << size << std::endl;
        std::cerr << "GetTlvUInt32() tlvsize: " << tlvsize << std::endl;
        std::cerr << "GetTlvUInt32() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    if (type != tlvtype) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvUInt32() FAILED - Type mismatch" << std::endl;
        std::cerr << "GetTlvUInt32() type: " << type << std::endl;
        std::cerr << "GetTlvUInt32() tlvtype: " << tlvtype << std::endl;
#endif
        return false;
    }

    *offset += 4; /* step past header */

    bool ok = true;
    ok &= getRawUInt32(data, tlvend, offset, in);

    return ok;
}

uint32_t GetTlvUInt32Size() {
    return 4 + 4;
}

bool SetTlvUInt64(void *data, uint32_t size, uint32_t *offset, uint16_t type,
                  uint64_t out) {
    if (!data)
        return false;
    uint16_t tlvsize = GetTlvUInt64Size();
    uint32_t tlvend = *offset + tlvsize;
    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "SetTlvUInt64() FAILED - not enough space. (or earlier)" << std::endl;
        std::cerr << "SetTlvUInt64() size: " << size << std::endl;
        std::cerr << "SetTlvUInt64() tlvsize: " << tlvsize << std::endl;
        std::cerr << "SetTlvUInt64() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    bool ok = true;

    /* Now use the function we got to set the TlvHeader */
    /* function shifts offset to the new start for the next data */
    ok &= SetTlvBase(data, tlvend, offset, type, tlvsize);

#ifdef TLV_BASE_DEBUG
    if (!ok) {
        std::cerr << "SetTlvUInt64() SetTlvBase FAILED (or earlier)" << std::endl;
    }
#endif

    /* now set the UInt64 ( in baseserial.h???) */
    ok &= setRawUInt64(data, tlvend, offset, out);

#ifdef TLV_BASE_DEBUG
    if (!ok) {
        std::cerr << "SetTlvUInt64() setRawUInt64 FAILED (or earlier)" << std::endl;
    }
#endif

    return ok;

}

bool GetTlvUInt64(void *data, uint32_t size, uint32_t *offset,
                  uint16_t type, uint64_t *in) {
    if (!data)
        return false;

    if (size < *offset + 4)
        return false;

    /* extract the type and size */
    void *tlvstart = right_shift_void_pointer(data, *offset);
    uint16_t tlvtype = GetTlvType(tlvstart);
    uint16_t tlvsize = GetTlvSize(tlvstart);

    /* check that there is size */
    uint32_t tlvend = *offset + tlvsize;
    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvUInt64() FAILED - not enough space." << std::endl;
        std::cerr << "GetTlvUInt64() size: " << size << std::endl;
        std::cerr << "GetTlvUInt64() tlvsize: " << tlvsize << std::endl;
        std::cerr << "GetTlvUInt64() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    if (type != tlvtype) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvUInt64() FAILED - Type mismatch" << std::endl;
        std::cerr << "GetTlvUInt64() type: " << type << std::endl;
        std::cerr << "GetTlvUInt64() tlvtype: " << tlvtype << std::endl;
#endif
        return false;
    }

    *offset += 4; /* step past header */

    bool ok = true;
    ok &= getRawUInt64(data, tlvend, offset, in);

    return ok;
}

uint32_t GetTlvUInt64Size() {
    return 4 + 8;
}

bool SetTlvString(void *data, uint32_t size, uint32_t *offset,
                  uint16_t type, std::string out) {
    if (!data)
        return false;
    uint16_t tlvsize = GetTlvStringSize(out);
    uint32_t tlvend = *offset + tlvsize; /* where the data will extend to */

    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "SetTlvString() FAILED - not enough space" << std::endl;
        std::cerr << "SetTlvString() size: " << size << std::endl;
        std::cerr << "SetTlvString() tlvsize: " << tlvsize << std::endl;
        std::cerr << "SetTlvString() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    bool ok = true;
    ok &= SetTlvBase(data, tlvend, offset, type, tlvsize);

    void *to  = right_shift_void_pointer(data, *offset);

    uint16_t strlen = tlvsize - 4;
    memcpy(to, out.c_str(), strlen);

    *offset += strlen;

    return ok;
}

//tested
bool GetTlvString(void *data, uint32_t size, uint32_t *offset,
                  uint16_t type, std::string &in) {
    if (!data)
        return false;

    if (size < *offset + 4) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvString() FAILED - not enough space" << std::endl;
        std::cerr << "GetTlvString() size: " << size << std::endl;
        std::cerr << "GetTlvString() *offset: " << *offset << std::endl;
#endif
        return false;
    }

    /* extract the type and size */
    void *tlvstart = right_shift_void_pointer(data, *offset);
    uint16_t tlvtype = GetTlvType(tlvstart);
    uint16_t tlvsize = GetTlvSize(tlvstart);

    /* check that there is size */
    uint32_t tlvend = *offset + tlvsize;
    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvString() FAILED - not enough space" << std::endl;
        std::cerr << "GetTlvString() size: " << size << std::endl;
        std::cerr << "GetTlvString() tlvsize: " << tlvsize << std::endl;
        std::cerr << "GetTlvString() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    if (type != tlvtype) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvString() FAILED - invalid type" << std::endl;
        std::cerr << "GetTlvString() type: " << type << std::endl;
        std::cerr << "GetTlvString() tlvtype: " << tlvtype << std::endl;
#endif
        return false;
    }

    char *strdata = (char *) right_shift_void_pointer(tlvstart, 4);
    uint16_t strsize = tlvsize - 4; /* remove the header */
    in = std::string(strdata, strsize);

    *offset += tlvsize; /* step along */
    return true;
}

uint32_t GetTlvStringSize(const std::string &in) {
    return 4 + in.size();
}


/* We must use a consistent wchar size for cross platform ness.
 * As unix uses 4bytes, and windows 2bytes? we'll go with 4bytes for maximum flexibility
 */

const uint32_t WCHAR_SIZE = 4;

bool SetTlvWideString(void *data, uint32_t size, uint32_t *offset,
                      uint16_t type, std::wstring out) {
    if (!data)
        return false;
    uint16_t tlvsize = GetTlvWideStringSize(out);
    uint32_t tlvend = *offset + tlvsize; /* where the data will extend to */

    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "SetTlvWideString() FAILED - not enough space" << std::endl;
        std::cerr << "SetTlvWideString() size: " << size << std::endl;
        std::cerr << "SetTlvWideString() tlvsize: " << tlvsize << std::endl;
        std::cerr << "SetTlvWideString() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    bool ok = true;
    ok &= SetTlvBase(data, tlvend, offset, type, tlvsize);

    uint16_t strlen = out.length();

    /* Must convert manually to ensure its always the same! */
    for (uint16_t i = 0; i < strlen; i++) {
        uint32_t widechar = out[i];
        ok &= setRawUInt32(data, tlvend, offset, widechar);
    }
    return ok;
}

//tested
bool GetTlvWideString(void *data, uint32_t size, uint32_t *offset,
                      uint16_t type, std::wstring &in) {
    if (!data)
        return false;

    if (size < *offset + 4) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvWideString() FAILED - not enough space" << std::endl;
        std::cerr << "GetTlvWideString() size: " << size << std::endl;
        std::cerr << "GetTlvWideString() *offset: " << *offset << std::endl;
#endif
        return false;
    }

    /* extract the type and size */
    void *tlvstart = right_shift_void_pointer(data, *offset);
    uint16_t tlvtype = GetTlvType(tlvstart);
    uint16_t tlvsize = GetTlvSize(tlvstart);

    /* check that there is size */
    uint32_t tlvend = *offset + tlvsize;
    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvWideString() FAILED - not enough space" << std::endl;
        std::cerr << "GetTlvWideString() size: " << size << std::endl;
        std::cerr << "GetTlvWideString() tlvsize: " << tlvsize << std::endl;
        std::cerr << "GetTlvWideString() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    if (type != tlvtype) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvWideString() FAILED - invalid type" << std::endl;
        std::cerr << "GetTlvWideString() type: " << type << std::endl;
        std::cerr << "GetTlvWideString() tlvtype: " << tlvtype << std::endl;
#endif
        return false;
    }


    bool ok = true;
    /* remove the header, calc string length */
    *offset += 4;
    uint16_t strlen = (tlvsize - 4) / WCHAR_SIZE;

    /* Must convert manually to ensure its always the same! */
    for (uint16_t i = 0; i < strlen; i++) {
        uint32_t widechar;
        ok &= getRawUInt32(data, tlvend, offset, &widechar);
        in += widechar;
    }
    return ok;
}

uint32_t GetTlvWideStringSize(std::wstring &in) {
    return 4 + in.size() * WCHAR_SIZE;
}

bool SetTlvQString(void *data, uint32_t size, uint32_t *offset,
                   uint16_t type, QString out) {
    if (!data) return false;
    uint16_t tlvsize = GetTlvQStringSize(out);
    uint32_t tlvend = *offset + tlvsize; /* where the data will extend to */

    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "SetTlvQString() FAILED - not enough space" << std::endl;
        std::cerr << "SetTlvQString() size: " << size << std::endl;
        std::cerr << "SetTlvQString() tlvsize: " << tlvsize << std::endl;
        std::cerr << "SetTlvQString() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    bool ok = true;
    ok &= SetTlvBase(data, tlvend, offset, type, tlvsize);

    void *to  = right_shift_void_pointer(data, *offset);

    uint16_t strlen = tlvsize - 4;
    memcpy(to, out.toUtf8().data(), strlen);

    *offset += strlen;

    return ok;
}

//tested
bool GetTlvQString(void *data, uint32_t size, uint32_t *offset,
                   uint16_t type, QString &in) {
    if (!data) return false;

    if (size < *offset + 4) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvQString() FAILED - not enough space" << std::endl;
        std::cerr << "GetTlvQString() size: " << size << std::endl;
        std::cerr << "GetTlvQString() *offset: " << *offset << std::endl;
#endif
        return false;
    }

    /* extract the type and size */
    void *tlvstart = right_shift_void_pointer(data, *offset);
    uint16_t tlvtype = GetTlvType(tlvstart);
    uint16_t tlvsize = GetTlvSize(tlvstart);

    /* check that there is size */
    uint32_t tlvend = *offset + tlvsize;
    if (size < tlvend) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvQString() FAILED - not enough space" << std::endl;
        std::cerr << "GetTlvQString() size: " << size << std::endl;
        std::cerr << "GetTlvQString() tlvsize: " << tlvsize << std::endl;
        std::cerr << "GetTlvQString() tlvend: " << tlvend << std::endl;
#endif
        return false;
    }

    if (type != tlvtype) {
#ifdef TLV_BASE_DEBUG
        std::cerr << "GetTlvQString() FAILED - invalid type" << std::endl;
        std::cerr << "GetTlvQString() type: " << type << std::endl;
        std::cerr << "GetTlvQString() tlvtype: " << tlvtype << std::endl;
#endif
        return false;
    }

#ifdef false
    bool ok = true;
    /* remove the header, calc string length */
    *offset += 4;

    QByteArray temp;
    /* Must convert manually to ensure its always the same! */
    for (uint16_t i = 0; i < tlvsize; i++) {
        uint32_t widechar;
        ok &= getRawUInt32(data, tlvend, offset, &widechar);
        temp += widechar;
    }
    in = QString(temp);
#endif

    char *strdata = (char *) right_shift_void_pointer(tlvstart, 4);
    uint16_t strsize = tlvsize - 4; /* remove the header */
    in = QString::fromUtf8(strdata, strsize);

    *offset += tlvsize; /* step along */

    return true;
}

uint32_t GetTlvQStringSize(QString &in) {
    return 4 + in.toUtf8().size();
}
