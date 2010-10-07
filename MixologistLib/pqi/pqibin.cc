/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-6, Robert Fernie
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

#include "pqi/pqibin.h"

BinFileInterface::BinFileInterface(const char *fname, int flags)
    :bin_flags(flags), buf(NULL), hash(NULL), bcount(0) {
    /* if read + write - open r+ */
    if ((bin_flags & BIN_FLAGS_READABLE) &&
            (bin_flags & BIN_FLAGS_WRITEABLE)) {
        buf = fopen(fname, "rb+");
        /* if the file don't exist */
        if (!buf) {
            buf = fopen(fname, "wb+");
        }

    } else if (bin_flags & BIN_FLAGS_READABLE) {
        buf = fopen(fname, "rb");
    } else if (bin_flags & BIN_FLAGS_WRITEABLE) {
        // This is enough to remove old file in Linux...
        // but not in windows.... (what to do)
        buf = fopen(fname, "wb");
        fflush(buf); /* this might help windows! */
    } else {
        /* not read or write! */
    }

    if (buf) {
        /* no problem */

        /* go to the end */
        fseek(buf, 0L, SEEK_END);
        /* get size */
        size = ftell(buf);
        /* back to the start */
        fseek(buf, 0L, SEEK_SET);
    } else {
        size = 0;
    }

    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        hash = new pqihash();
    }
}


BinFileInterface::~BinFileInterface() {
    if (buf) {
        fclose(buf);
    }

    if (hash) {
        delete hash;
    }
}

int BinFileInterface::close() {
    if (buf) {
        fclose(buf);
        buf = NULL;
    }
    return 1;
}

int BinFileInterface::senddata(void *data, int len) {
    if (!buf)
        return -1;

    if (1 != fwrite(data, len, 1, buf)) {
        return -1;
    }
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        hash->addData(data, len);
        bcount += len;
    }
    return len;
}

int BinFileInterface::readdata(void *data, int len) {
    if (!buf)
        return -1;

    if (1 != fread(data, len, 1, buf)) {
        return -1;
    }
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        hash->addData(data, len);
        bcount += len;
    }
    return len;
}

std::string  BinFileInterface::gethash() {
    std::string hashstr;
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        hash->Complete(hashstr);
    }
    return hashstr;
}

uint64_t BinFileInterface::bytecount() {
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        return bcount;
    }
    return 0;
}


BinMemInterface::BinMemInterface(int defsize, int flags)
    :bin_flags(flags), buf(NULL), size(defsize),
     recvsize(0), readloc(0), hash(NULL), bcount(0) {
    buf = malloc(defsize);
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        hash = new pqihash();
    }
}


BinMemInterface::BinMemInterface(const void *data, const int defsize, int flags)
    :bin_flags(flags), buf(NULL), size(defsize),
     recvsize(0), readloc(0), hash(NULL), bcount(0) {
    buf = malloc(defsize);
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        hash = new pqihash();
    }

    /* just remove the const
     * *BAD* but senddata don't change it anyway
     */
    senddata((void *) data, defsize);
}

BinMemInterface::~BinMemInterface() {
    if (buf)
        free(buf);

    if (hash)
        delete hash;
    return;
}

int BinMemInterface::close() {
    if (buf) {
        free(buf);
        buf = NULL;
    }
    size = 0;
    recvsize = 0;
    readloc = 0;
    return 1;
}

/* some fns to mess with the memory */
int BinMemInterface::fseek(int loc) {
    if (loc <= recvsize) {
        readloc = loc;
        return 1;
    }
    return 0;
}


int BinMemInterface::senddata(void *data, const int len) {
    if (recvsize + len > size) {
        /* resize? */
        return -1;
    }
    memcpy((char *) buf + recvsize, data, len);
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        hash->addData(data, len);
        bcount += len;
    }
    recvsize += len;
    return len;
}


int BinMemInterface::readdata(void *data, int len) {
    if (readloc + len > recvsize) {
        /* no more stuff? */
        return -1;
    }
    memcpy(data, (char *) buf + readloc, len);
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        hash->addData(data, len);
        bcount += len;
    }
    readloc += len;
    return len;
}



std::string  BinMemInterface::gethash() {
    std::string hashstr;
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        hash->Complete(hashstr);
    }
    return hashstr;
}


uint64_t BinMemInterface::bytecount() {
    if (bin_flags & BIN_FLAGS_HASH_DATA) {
        return bcount;
    }
    return 0;
}

bool    BinMemInterface::writetofile(const char *fname) {
    FILE *fd = fopen(fname, "wb");
    if (!fd) {
        return false;
    }

    if (1 != fwrite(buf, recvsize, 1, fd)) {
        fclose(fd);
        return false;
    }

    fclose(fd);

    return true;
}

bool    BinMemInterface::readfromfile(const char *fname) {
    FILE *fd = fopen(fname, "rb");
    if (!fd) {
        return false;
    }

    /* get size */
    ::fseek(fd, 0L, SEEK_END);
    int fsize = ftell(fd);

    if (fsize > size) {
        /* not enough room */
        std::cerr << "BinMemInterface::readfromfile() not enough room";
        std::cerr << std::endl;

        fclose(fd);
        return false;
    }

    ::fseek(fd, 0L, SEEK_SET);
    if (1 != fread(buf, fsize, 1, fd)) {
        /* not enough room */
        std::cerr << "BinMemInterface::readfromfile() failed fread";
        std::cerr << std::endl;

        fclose(fd);
        return false;
    }

    recvsize = fsize;
    readloc = 0;
    fclose(fd);

    return true;
}


/**************************************************************************/


void printNetBinID(std::ostream &out, std::string id, uint32_t t) {
    out << "NetBinId(" << id << ",";
    if (t == PQI_CONNECT_TCP) {
        out << "TCP)";
    } else {
        out << "UDP)";
    }
}
