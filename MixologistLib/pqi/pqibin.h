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

#ifndef PQI_BIN_INTERFACE_HEADER
#define PQI_BIN_INTERFACE_HEADER

#include "pqi/pqi_base.h"
#include "pqi/pqihash.h"
#include <stdio.h>

class BinFileInterface: public BinInterface {
public:
    BinFileInterface(const char *fname, int flags);
    virtual ~BinFileInterface();

    virtual int     tick() {
        return 1;
    }

    virtual int senddata(void *data, int len);
    virtual int readdata(void *data, int len);
    virtual int netstatus() {
        return 1;
    }
    virtual int isactive()  {
        return (buf != NULL);
    }
    virtual bool    moretoread() {
        if ((buf) && (bin_flags | BIN_FLAGS_READABLE)) {
            if ((size - ftell(buf)) > 0) {
                return true;
            }
        }
        return false;
    }

    virtual int close();
    virtual bool    cansend() {
        return (bin_flags | BIN_FLAGS_WRITEABLE);
    }
    virtual bool    bandwidthLimited() {
        return false;
    }

    /* if HASHing is switched on */
    virtual std::string gethash();
    virtual uint64_t bytecount();

private:
    int   bin_flags;
    FILE *buf;
    int   size;
    pqihash *hash;
    uint64_t bcount;
};


class BinMemInterface: public BinInterface {
public:
    BinMemInterface(int defsize, int flags);
    BinMemInterface(const void *data, const int size, int flags);
    virtual ~BinMemInterface();

    /* Extra Interfaces */
    int fseek(int loc);
    int memsize() {
        return recvsize;
    }
    void   *memptr() {
        return buf;
    }

    /* file interface */
    bool    writetofile(const char *fname);
    bool    readfromfile(const char *fname);

    virtual int     tick() {
        return 1;
    }

    virtual int senddata(void *data, int len);
    virtual int readdata(void *data, int len);
    virtual int netstatus() {
        return 1;
    }
    virtual int isactive()  {
        return 1;
    }
    virtual bool    moretoread() {
        if ((buf) && (bin_flags | BIN_FLAGS_READABLE )) {
            if (readloc < recvsize) {
                return true;
            }
        }
        return false;
    }

    virtual int close();
    virtual bool    cansend()    {
        return (bin_flags | BIN_FLAGS_WRITEABLE);
    }
    virtual bool    bandwidthLimited() {
        return false;
    }

    virtual std::string gethash();
    virtual uint64_t bytecount();

private:
    int   bin_flags;
    void *buf;
    int   size;
    int   recvsize;
    int   readloc;
    pqihash *hash;
    uint64_t bcount;
};

#endif // PQI_BINARY_INTERFACES_HEADER

