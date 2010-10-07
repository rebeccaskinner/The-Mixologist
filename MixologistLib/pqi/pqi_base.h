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

#ifndef PQI_BASE_ITEM_HEADER
#define PQI_BASE_ITEM_HEADER

#include <list>
#include <string>
#include <iostream>
#include <functional>
#include <algorithm>
#include <inttypes.h>

#include "pqi/pqinetwork.h"

/*** Base DataTypes: ****/
#include "serialiser/serial.h"


#define PQI_MIN_PORT 1024
#define PQI_MAX_PORT 50000
#define PQI_MIN_RAND_PORT 10000
#define PQI_MAX_RAND_PORT 30000
#define PQI_DEFAULT_PORT 1680

/**** Consts for pqiperson */

const uint32_t PQI_CONNECT_TCP = 0x0001;
const uint32_t PQI_CONNECT_UDP = 0x0002;

int getPQIsearchId();
int fixme(char *str, int n);

/*********************** PQI INTERFACE ******************************\
The basic exchange interface.
Includes methods for getting and sending items, ongoing maintenance,
notification from NetInterfaces and bandwidth control.
Types include pqistreamer for sending data over a network, pqiperson
which holds multiple pqistreamers and uses PQInterface as an interface
to pass through to them, and pqiloopback.
*/

class NetInterface;

class PQInterface {
public:
    PQInterface(std::string id, int _librarymixer_id)
        :peerId(id), librarymixer_id(_librarymixer_id),bw_in(0), bw_out(0), bwMax_in(0), bwMax_out(0) {
        return;
    }
    virtual ~PQInterface() {
        return;
    }

    virtual int SendItem(NetItem *) = 0;
    virtual NetItem *GetItem() = 0;

    virtual int     tick() {
        return 0;
    }
    virtual int     status() {
        return 0;
    }
    virtual std::string PeerId() {
        return peerId;
    }
    virtual int     LibraryMixerId() {
        return librarymixer_id;
    }

    //Called by NetInterfaces to inform the PQInterface of connection events.
    virtual int notifyEvent(NetInterface *ni, int event) {
        (void)ni;
        (void)(event);
        return 0;
    }

    virtual float   getRate(bool in) {
        if (in) return bw_in;
        return bw_out;
    }

    virtual float   getMaxRate(bool in) {
        if (in) return bwMax_in;
        return bwMax_out;
    }

    virtual void    setMaxRate(bool in, float val) {
        if (in) bwMax_in = val;
        else bwMax_out = val;
    }

protected:

    void    setRate(bool in, float val) {
        if (in) bw_in = val;
        else bw_out = val;
    }

private:

    std::string peerId;
    int librarymixer_id;
    float   bw_in, bw_out, bwMax_in, bwMax_out;
};


/********************** Network INTERFACE ***************************
Basic interface for handling control of a connection.

It is passed a pointer to a PQInterface *parent,
which it uses to notify the system of connect/disconnect Events.
*/

//Possible notification events
static const int NET_CONNECT_RECEIVED     = 1;
static const int NET_CONNECT_SUCCESS      = 2;
static const int NET_CONNECT_UNREACHABLE  = 3;
static const int NET_CONNECT_FIREWALLED   = 4;
static const int NET_CONNECT_FAILED       = 5;

static const uint32_t NET_PARAM_CONNECT_DELAY   = 1;
static const uint32_t NET_PARAM_CONNECT_PERIOD  = 2;
static const uint32_t NET_PARAM_CONNECT_TIMEOUT = 3;

class NetInterface {
public:
    NetInterface(PQInterface *p_in, std::string id, int _librarymixer_id)
        :p(p_in), peerId(id), librarymixer_id(_librarymixer_id) {
        return;
    }

    virtual ~NetInterface() {
        return;
    }

    virtual int connect(struct sockaddr_in raddr) = 0;
    virtual int listen() = 0;
    virtual int stoplistening() = 0;
    virtual int disconnect() = 0;
    virtual int reset() = 0;
    virtual std::string PeerId() {
        return peerId;
    }
    virtual int LibraryMixerId() {
        return librarymixer_id;
    }

    virtual bool connect_parameter(uint32_t type, uint32_t value) = 0;

protected:
    PQInterface *parent() {
        return p;
    }

private:
    PQInterface *p;
    std::string peerId;
    int librarymixer_id;
};

/********************** Binary INTERFACE ****************************
The binary interface used by Network/loopback/file interfaces
*/

#define BIN_FLAGS_NO_CLOSE  0x0001
#define BIN_FLAGS_READABLE  0x0002
#define BIN_FLAGS_WRITEABLE 0x0004
#define BIN_FLAGS_NO_DELETE 0x0008
#define BIN_FLAGS_HASH_DATA 0x0010

class BinInterface {
public:
    BinInterface() {
        return;
    }
    virtual ~BinInterface() {
        return;
    }

    virtual int     tick() = 0;

    virtual int senddata(void *data, int len) = 0;
    virtual int readdata(void *data, int len) = 0;
    virtual int netstatus() = 0;
    virtual int isactive() = 0;
    virtual bool    moretoread() = 0;
    virtual bool    cansend() = 0;

    /* method for streamer to shutdown bininterface */
    virtual int close() = 0;

    /* if hashing data */
    virtual std::string gethash() = 0;
    virtual uint64_t bytecount() {
        return 0;
    }

    /* used by pqistreamer to limit transfers */
    virtual bool    bandwidthLimited() {
        return true;
    }
};

/*
Combined NetInterface with a BinInterface to send information over.
Types include pqissl and pqissludp.
*/

class NetBinInterface: public NetInterface, public BinInterface {
public:
    NetBinInterface(PQInterface *parent, std::string id, int librarymixer_id)
        :NetInterface(parent, id, librarymixer_id) {
        return;
    }
    virtual ~NetBinInterface() {
        return;
    }
};

#define CHAN_SIGN_SIZE 16
#define CERTSIGNLEN 16       /* actual byte length of signature */
#define PQI_PEERID_LENGTH 32 /* When expanded into a string */


#endif // PQI_BASE_ITEM_HEADER

