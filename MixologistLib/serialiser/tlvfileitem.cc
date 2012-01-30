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

#include <iostream>

#include "serialiser/tlvbase.h"
#include "serialiser/tlvtypes.h"
#include "serialiser/baseserial.h"

/***
 * #define TLV_FI_DEBUG 1
 **/


TlvFileItem::TlvFileItem() {
    TlvClear();
}

void TlvFileItem::TlvClear() {
    filesize = 0;
    hash = "";
    name = "";
    path = "";
    pop = 0;
    age = 0;
}

uint16_t    TlvFileItem::TlvSize() {
    uint32_t s = 4; /* header */
    s += 8; /* filesize */
    s += GetTlvQStringSize(hash);
#ifdef TLV_FI_DEBUG
    std::cerr << "TlvFileItem::TlvSize() 8 + Hash: " << s << std::endl;
#endif


    /* now optional ones */
    if (name.length() > 0) {
        s += GetTlvStringSize(name);
#ifdef TLV_FI_DEBUG
        std::cerr << "TlvFileItem::TlvSize() + Name: " << s << std::endl;
#endif
    }

    if (path.length() > 0) {
        s += GetTlvStringSize(path);
#ifdef TLV_FI_DEBUG
        std::cerr << "TlvFileItem::TlvSize() + Path: " << s << std::endl;
#endif
    }

    if (pop != 0) {
        s += GetTlvUInt32Size();
#ifdef TLV_FI_DEBUG
        std::cerr << "TlvFileItem::TlvSize() + Pop: " << s << std::endl;
#endif
    }

    if (age != 0) {
        s += GetTlvUInt32Size();
#ifdef TLV_FI_DEBUG
        std::cerr << "TlvFileItem::TlvSize() 4 + Age: " << s << std::endl;
#endif
    }

#ifdef TLV_FI_DEBUG
    std::cerr << "TlvFileItem::TlvSize() Total: " << s << std::endl;
#endif

    return s;
}

/* serialise the data to the buffer */
bool     TlvFileItem::SetTlv(void *data, uint32_t size, uint32_t *offset) {
    /* must check sizes */
    uint16_t tlvsize = TlvSize();
    uint32_t tlvend  = *offset + tlvsize;

    if (size < tlvend) {
#ifdef TLV_FI_DEBUG
        std::cerr << "TlvFileItem::SetTlv() Failed size (" << size;
        std::cerr << ") < tlvend (" << tlvend << ")" << std::endl;
#endif
        return false; /* not enough space */
    }

    bool ok = true;

    /* start at data[offset] */
    ok &= SetTlvBase(data, tlvend, offset, TLV_TYPE_FILEITEM, tlvsize);

#ifdef TLV_FI_DEBUG
    if (!ok) {
        std::cerr << "TlvFileItem::SetTlv() SetTlvBase Failed (or earlier)" << std::endl;
    }
    std::cerr << "TlvFileItem::SetTlv() PostBase:" << std::endl;
    std::cerr << "TlvFileItem::SetTlv() Data: " << data << " size: " << size << " offset: " << *offset;
    std::cerr << std::endl;
#endif


    /* add mandatory parts first */
    ok &= setRawUInt64(data, tlvend, offset, filesize);

#ifdef TLV_FI_DEBUG
    if (!ok) {
        std::cerr << "TlvFileItem::SetTlv() SetRawUInt32(FILESIZE) Failed (or earlier)" << std::endl;
    }
    std::cerr << "TlvFileItem::SetTlv() PostSize:" << std::endl;
    std::cerr << "TlvFileItem::SetTlv() Data: " << data << " size: " << size << " offset: " << *offset;
    std::cerr << std::endl;
#endif

    ok &= SetTlvQString(data, tlvend, offset, TLV_TYPE_STR_HASH_SHA1, hash);


#ifdef TLV_FI_DEBUG
    if (!ok) {
        std::cerr << "TlvFileItem::SetTlv() SetTlvString(HASH) Failed (or earlier)" << std::endl;
    }
    std::cerr << "TlvFileItem::SetTlv() PostHash:" << std::endl;
    std::cerr << "TlvFileItem::SetTlv() Data: " << data << " size: " << size << " offset: " << *offset;
    std::cerr << std::endl;
#endif

    /* now optional ones */
    if (name.length() > 0)
        ok &= SetTlvString(data, tlvend, offset, TLV_TYPE_STR_NAME, name);
#ifdef TLV_FI_DEBUG
    if (!ok) {
        std::cerr << "TlvFileItem::SetTlv() Setting Option:Name Failed (or earlier)" << std::endl;
    }
    std::cerr << "TlvFileItem::SetTlv() PostName:" << std::endl;
    std::cerr << "TlvFileItem::SetTlv() Data: " << data << " size: " << size << " offset: " << *offset;
    std::cerr << std::endl;
#endif

    if (path.length() > 0)
        ok &= SetTlvString(data, tlvend, offset, TLV_TYPE_STR_PATH, path);
#ifdef TLV_FI_DEBUG
    if (!ok) {
        std::cerr << "TlvFileItem::SetTlv() Setting Option:Path Failed (or earlier)" << std::endl;
    }
    std::cerr << "TlvFileItem::SetTlv() Pre Pop:" << std::endl;
    std::cerr << "TlvFileItem::SetTlv() Data: " << data << " size: " << size << " offset: " << *offset;
    std::cerr << " type: " << TLV_TYPE_UINT32_POP << " value: " << pop;
    std::cerr << std::endl;
#endif

    if (pop != 0)
        ok &= SetTlvUInt32(data, tlvend, offset, TLV_TYPE_UINT32_POP,  pop);
#ifdef TLV_FI_DEBUG
    if (!ok) {
        std::cerr << "TlvFileItem::SetTlv() Setting Option:Pop Failed (or earlier)" << std::endl;
    }
    std::cerr << "TlvFileItem::SetTlv() Post Pop/Pre Age:" << std::endl;
    std::cerr << "TlvFileItem::SetTlv() Data: " << data << " size: " << size << " offset: " << *offset;
    std::cerr << " type: " << TLV_TYPE_UINT32_AGE << " value: " << age;
    std::cerr << std::endl;
#endif

    if (age != 0)
        ok &= SetTlvUInt32(data, tlvend, offset, TLV_TYPE_UINT32_AGE,  age);
#ifdef TLV_FI_DEBUG
    if (!ok) {
        std::cerr << "TlvFileItem::SetTlv() Setting Option:Age Failed (or earlier)" << std::endl;
    }
    std::cerr << "TlvFileItem::SetTlv() Post Age:" << std::endl;
    std::cerr << "TlvFileItem::SetTlv() Data: " << data << " size: " << size << " offset: " << *offset;
    std::cerr << std::endl;
#endif



#ifdef TLV_FI_DEBUG
    if (!ok) {
        std::cerr << "TlvFileItem::SetTlv() Setting Options Failed (or earlier)" << std::endl;
    }
#endif

    return ok;
}

bool     TlvFileItem::GetTlv(void *data, uint32_t size, uint32_t *offset) {
    uint16_t tlvtype = GetTlvType( &(((uint8_t *) data)[*offset])  );
    uint16_t tlvsize = GetTlvSize( &(((uint8_t *) data)[*offset])  );
    uint32_t tlvend = *offset + tlvsize;

    if (size < tlvend)    /* check size */
        return false; /* not enough space */

    if (tlvtype != TLV_TYPE_FILEITEM) /* check type */
        return false;

    bool ok = true;

    /* ready to load */
    TlvClear();

    /* skip the header */
    (*offset) += 4;

    /* get mandatory parts first */
    ok &= getRawUInt64(data, tlvend, offset, &filesize);
    ok &= GetTlvQString(data, tlvend, offset, TLV_TYPE_STR_HASH_SHA1, hash);

    /* while there is more TLV (optional part) */
    while ((*offset) + 2 < tlvend) {
        /* get the next type */
        uint16_t tlvsubtype = GetTlvType( &(((uint8_t *) data)[*offset]) );
        switch (tlvsubtype) {
            case TLV_TYPE_STR_NAME:
                ok &= GetTlvString(data, tlvend, offset, TLV_TYPE_STR_NAME, name);
                break;
            case TLV_TYPE_STR_PATH:
                ok &= GetTlvString(data, tlvend, offset, TLV_TYPE_STR_PATH, path);
                break;
            case TLV_TYPE_UINT32_POP:
                ok &= GetTlvUInt32(data, tlvend, offset, TLV_TYPE_UINT32_POP, &pop);
                break;
            case TLV_TYPE_UINT32_AGE:
                ok &= GetTlvUInt32(data, tlvend, offset, TLV_TYPE_UINT32_AGE, &age);
                break;
            default:
                ok = false;
        }
        if (!ok) {
            return false;
        }
    }
    return ok;
}


/* print it out */
std::ostream &TlvFileItem::print(std::ostream &out, uint16_t indent) {
    printBase(out, "TlvFileItem", indent);
    uint16_t int_Indent = indent + 2;


    printIndent(out, int_Indent);
    out << "Mandatory:  FileSize: " << filesize << " Hash: " << hash.toStdString();
    out << std::endl;

    printIndent(out, int_Indent);
    out << "Optional:" << std::endl;

    /* now optional ones */
    if (name.length() > 0) {
        printIndent(out, int_Indent);
        out << "Name: " << name << std::endl;
    }
    if (path.length() > 0) {
        printIndent(out, int_Indent);
        out << "Path: " << path << std::endl;
    }
    if (pop != 0) {
        printIndent(out, int_Indent);
        out << "Pop: " << pop << std::endl;
    }
    if (age != 0) {
        printIndent(out, int_Indent);
        out << "Age: " << age << std::endl;
    }

    printEnd(out, "TlvFileItem", indent);

    return out;
}


/************************************* TlvFileSet ************************************/

void TlvFileSet::TlvClear() {
    title.clear();
    comment.clear();
    items.clear();
}

uint16_t TlvFileSet::TlvSize() {
    uint32_t s = 4; /* header */

    /* first determine the total size of TlvFileItems in list */

    std::list<TlvFileItem>::iterator it;

    for (it = items.begin(); it != items.end() ; ++it) {
        s += (*it).TlvSize();
    }

    /* now add comment and title length of this tlv object */

    if (title.length() > 0)
        s += GetTlvWideStringSize(title);
    if (comment.length() > 0)
        s += GetTlvWideStringSize(comment);

    return s;
}


/* serialize data to the buffer */

bool     TlvFileSet::SetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    /* must check sizes */
    uint16_t tlvsize = TlvSize();
    uint32_t tlvend  = *offset + tlvsize;

    if (size < tlvend)
        return false; /* not enough space */

    bool ok = true;

    /* start at data[offset] */
    ok &= SetTlvBase(data, tlvend, offset, TLV_TYPE_FILESET, tlvsize);

    /* add mandatory parts first */
    std::list<TlvFileItem>::iterator it;

    for (it = items.begin(); it != items.end() ; ++it) {
        ok &= (*it).SetTlv(data, size, offset);
        /* drop out if fails */
        if (!ok)
            return false;
    }

    /* now optional ones */
    if (title.length() > 0)
        ok &= SetTlvWideString(data, tlvend, offset, TLV_TYPE_WSTR_TITLE, title);
    if (comment.length() > 0)
        ok &= SetTlvWideString(data, tlvend, offset, TLV_TYPE_WSTR_COMMENT, comment);

    return ok;

}


bool     TlvFileSet::GetTlv(void *data, uint32_t size, uint32_t *offset) {
    if (size < *offset + 4)
        return false;

    uint16_t tlvtype = GetTlvType( &(((uint8_t *) data)[*offset])  );
    uint16_t tlvsize = GetTlvSize( &(((uint8_t *) data)[*offset])  );
    uint32_t tlvend = *offset + tlvsize;

    if (size < tlvend)    /* check size */
        return false; /* not enough space */

    if (tlvtype != TLV_TYPE_FILESET) /* check type */
        return false;

    bool ok = true;

    /* ready to load */
    TlvClear();

    /* skip the header */
    (*offset) += 4;

    /* while there is more TLV  */
    while ((*offset) + 2 < tlvend) {
        /* get the next type */
        uint16_t tlvsubtype = GetTlvType( &(((uint8_t *) data)[*offset]) );
        if (tlvsubtype == TLV_TYPE_FILEITEM) {
            TlvFileItem newitem;
            ok &= newitem.GetTlv(data, size, offset);
            if (ok) {
                items.push_back(newitem);
            }
        } else if (tlvsubtype == TLV_TYPE_WSTR_TITLE) {
            ok &= GetTlvWideString(data, tlvend, offset,
                                   TLV_TYPE_WSTR_TITLE, title);
        } else if (tlvsubtype == TLV_TYPE_WSTR_COMMENT) {
            ok &= GetTlvWideString(data, tlvend, offset,
                                   TLV_TYPE_WSTR_COMMENT, comment);
        } else {
            /* unknown subtype -> error */
            ok = false;
        }

        if (!ok) {
            return false;
        }
    }

    return ok;
}

/* print it out */

std::ostream &TlvFileSet::print(std::ostream &out, uint16_t indent) {
    printBase(out, "TlvFileSet", indent);
    uint16_t int_Indent = indent + 2;


    printIndent(out, int_Indent);
    out << "Mandatory:"  << std::endl;
    std::list<TlvFileItem>::iterator it;

    for (it = items.begin(); it != items.end() ; ++it) {
        it->print(out, int_Indent);
    }
    printIndent(out, int_Indent);
    out << "Optional:" << std::endl;

    /* now optional ones */
    if (title.length() > 0) {
        printIndent(out, int_Indent);
        std::string cnv_title(title.begin(), title.end());
        out << "Title: " << cnv_title << std::endl;
    }
    if (comment.length() > 0) {
        printIndent(out, int_Indent);
        std::string cnv_comment(comment.begin(), comment.end());
        out << "Comment: " << cnv_comment << std::endl;
    }

    printEnd(out, "TlvFileSet", indent);

    return out;
}


/************************************* TlvFileData ************************************/

TlvFileData::TlvFileData()
    :TlvItem(), file_offset(0), binData(TLV_TYPE_BIN_FILEDATA) {
    return;
}

void TlvFileData::TlvClear() {
    file.TlvClear();
    binData.TlvClear();
    file_offset = 0;
}


uint16_t TlvFileData::TlvSize() {
    uint32_t s = 4; /* header */

    /* collect sizes for both uInts and data length */
    s+= file.TlvSize();
    s+= GetTlvUInt64Size();
    s+= binData.TlvSize();

    return s;
}


bool TlvFileData::SetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    /* must check sizes */
    uint16_t tlvsize = TlvSize();
    uint32_t tlvend  = *offset + tlvsize;

    if (size < tlvend)
        return false; /* not enough space */

    bool ok = true;

    /* start at data[offset] */
    ok &= SetTlvBase(data, tlvend, offset, TLV_TYPE_FILEDATA , tlvsize);

    /* add mandatory part */
    ok &= file.SetTlv(data, size, offset);
    ok &= SetTlvUInt64(data,size,offset,
                       TLV_TYPE_UINT64_OFFSET,file_offset);
    ok &= binData.SetTlv(data, size, offset);

    return ok;


}

bool TlvFileData::GetTlv(void *data, uint32_t size, uint32_t *offset) { /* serialise   */
    if (size < *offset + 4) {
        return false;
    }

    uint16_t tlvtype = GetTlvType( &(((uint8_t *) data)[*offset])  );
    uint16_t tlvsize = GetTlvSize( &(((uint8_t *) data)[*offset])  );
    uint32_t tlvend = *offset + tlvsize;

    if (size < tlvend)    /* check size */
        return false; /* not enough space */

    if (tlvtype != TLV_TYPE_FILEDATA) /* check type */
        return false;

    bool ok = true;

    /* ready to load */
    TlvClear();

    /* skip the header */
    (*offset) += 4;

    ok &= file.GetTlv(data, size, offset);
    ok &= GetTlvUInt64(data,size,offset,
                       TLV_TYPE_UINT64_OFFSET,&file_offset);
    ok &= binData.GetTlv(data, size, offset);

    return ok;

}

/* print it out */
std::ostream &TlvFileData::print(std::ostream &out, uint16_t indent) {
    printBase(out, "TlvFileData", indent);
    uint16_t int_Indent = indent + 2;

    file.print(out, int_Indent);

    printIndent(out, int_Indent);
    out << "FileOffset: " << file_offset;
    out << std::endl;

    binData.print(out, int_Indent);

    printEnd(out, "TlvFileData", indent);
    return out;

}

