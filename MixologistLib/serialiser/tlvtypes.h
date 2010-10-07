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

#ifndef TLV_COMPOUND_TYPES_H
#define TLV_COMPOUND_TYPES_H

/*******************************************************************
 * These are the Compound TLV structures that must be (un)packed.
 *
 ******************************************************************/

#include <string>
#include <list>
#include <stdlib.h>
#include <stdint.h>

//#define TLV_TYPE_FILE_ITEM   0x0000

//! A base class for all tlv items
/*! This class is provided to allow the serialisation and deserialization of compund
tlv items
*/
class TlvItem {
public:
    TlvItem() {
        return;
    }
    virtual ~TlvItem() {
        return;
    }
    virtual uint16_t TlvSize() = 0;
    virtual void     TlvClear() = 0;
    virtual void     TlvShallowClear(); /*! Don't delete allocated data */
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset) = 0; /* serialise   */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset) = 0; /* deserialise */
    virtual std::ostream &print(std::ostream &out, uint16_t indent) = 0;
    std::ostream &printBase(std::ostream &out, std::string clsName, uint16_t indent);
    std::ostream &printEnd(std::ostream &out, std::string clsName, uint16_t indent);
};

std::ostream &printIndent(std::ostream &out, uint16_t indent);

//! GENERIC Binary Data TLV
/*! Use to serialise and deserialise binary data, usually included in other compound tlvs
*/
class TlvBinaryData: public TlvItem {
public:
    TlvBinaryData(uint16_t t);
    virtual ~TlvBinaryData();
    virtual uint16_t TlvSize();
    virtual void     TlvClear(); /*! Initialize fields to empty legal values ( "0", "", etc) */
    virtual void     TlvShallowClear(); /*! Don't delete the binary data */

    /// Serialise.
    /*! Serialise Tlv to buffer(*data) of 'size' bytes starting at *offset */
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset);
    /// Deserialise.
    /*! Deserialise Tlv buffer(*data) of 'size' bytes starting at *offset */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset);
    virtual std::ostream &print(std::ostream &out, uint16_t indent); /*! Error/Debug util function */

    bool    setBinData(void *data, uint16_t size);

    uint16_t tlvtype;   /// set/checked against TLV input
    uint16_t bin_len;   /// size of malloc'ed data (not serialised)
    void    *bin_data;  /// mandatory
};

class TlvFileItem: public TlvItem {
public:
    TlvFileItem();
    virtual ~TlvFileItem() {
        return;
    }
    virtual uint16_t TlvSize();
    virtual void     TlvClear();
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset); /* serialise   */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset); /* deserialise */
    virtual std::ostream &print(std::ostream &out, uint16_t indent);

    uint64_t filesize; /// Mandatory: size of file to be downloaded
    std::string hash;  /// Mandatory: to find file
    std::string name;  /// Optional: name of file
    std::string path;  /// Optional: path on host computer
    uint32_t    pop;   /// Optional: Popularity of file
    uint32_t    age;   /// Optional: age of file
};

class TlvFileSet: public TlvItem {
public:
    TlvFileSet() {
        return;
    }
    virtual ~TlvFileSet() {
        return;
    }
    virtual uint16_t TlvSize();
    virtual void     TlvClear();
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset); /* serialise   */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset); /* deserialise */
    virtual std::ostream &print(std::ostream &out, uint16_t indent);

    std::list<TlvFileItem> items; /// Mandatory
    std::wstring title;         /// Optional: title of file set
    std::wstring comment;       /// Optional: comments for file
};


class TlvFileData: public TlvItem {
public:
    TlvFileData();
    virtual ~TlvFileData() {
        return;
    }
    virtual uint16_t TlvSize();
    virtual void     TlvClear();
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset); /* serialise   */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset); /* deserialise */
    virtual std::ostream &print(std::ostream &out, uint16_t indent);

    TlvFileItem   file;         /// Mandatory: file information
    uint64_t        file_offset;  /// Mandatory: where to start in bin data
    TlvBinaryData binData;      /// Mandatory: serialised file info
};


/**** MORE TLV *****
 *
 *
 */


class TlvPeerIdSet: public TlvItem {
public:
    TlvPeerIdSet() {
        return;
    }
    virtual ~TlvPeerIdSet() {
        return;
    }
    virtual uint16_t TlvSize();
    virtual void     TlvClear();
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset); /* serialise   */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset); /* deserialise */
    virtual std::ostream &print(std::ostream &out, uint16_t indent);
    virtual std::ostream &printHex(std::ostream &out, uint16_t indent); /* SPECIAL One */

    std::list<std::string> ids; /* Mandatory */
};


class TlvServiceIdSet: public TlvItem {
public:
    TlvServiceIdSet() {
        return;
    }
    virtual ~TlvServiceIdSet() {
        return;
    }
    virtual uint16_t TlvSize();
    virtual void     TlvClear();
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset); /* serialise   */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset); /* deserialise */
    virtual std::ostream &print(std::ostream &out, uint16_t indent);

    std::list<uint32_t> ids; /* Mandatory */
};




/**** MORE TLV *****
 * Key/Value + Set used for Generic Configuration Parameters.
 *
 */


class TlvKeyValue: public TlvItem {
public:
    TlvKeyValue() {
        return;
    }
    virtual ~TlvKeyValue() {
        return;
    }
    virtual uint16_t TlvSize();
    virtual void     TlvClear();
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset); /* serialise   */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset); /* deserialise */
    virtual std::ostream &print(std::ostream &out, uint16_t indent);

    std::string key;    /// Mandatory : For use in hash tables
    std::string value;  /// Mandatory : For use in hash tables
};

class TlvKeyValueSet: public TlvItem {
public:
    TlvKeyValueSet() {
        return;
    }
    virtual ~TlvKeyValueSet() {
        return;
    }
    virtual uint16_t TlvSize();
    virtual void     TlvClear();
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset); /* serialise   */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset); /* deserialise */
    virtual std::ostream &print(std::ostream &out, uint16_t indent);

    std::list<TlvKeyValue> pairs; /// For use in hash tables
};


class TlvImage: public TlvItem {
public:
    TlvImage();
    virtual ~TlvImage() {
        return;
    }
    virtual uint16_t TlvSize();
    virtual void     TlvClear();
    virtual bool     SetTlv(void *data, uint32_t size, uint32_t *offset); /* serialise   */
    virtual bool     GetTlv(void *data, uint32_t size, uint32_t *offset); /* deserialise */
    virtual std::ostream &print(std::ostream &out, uint16_t indent);

    uint32_t        image_type;   // Mandatory:
    TlvBinaryData binData;      // Mandatory: serialised file info
};

#endif

