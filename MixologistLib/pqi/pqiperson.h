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
#ifndef MRK_PQI_PERSON_HEADER
#define MRK_PQI_PERSON_HEADER


#include "pqi/pqi.h"

#include <list>

class pqiperson;

static const int CONNECT_RECEIVED     = 1;
static const int CONNECT_SUCCESS      = 2;
static const int CONNECT_UNREACHABLE  = 3;
static const int CONNECT_FIREWALLED   = 4;
static const int CONNECT_FAILED       = 5;

#include "pqi/pqistreamer.h"

class pqipersongrp;
class pqiconnect;
/*
Each pqiperson belongs to a specific peer, and represents the
aggregated various methods of communicating with the peer.
Each method of communication is stored as a pqiconnect.
More than one connection method is supported, with one active at
any given time.  Passes on calls to the PQInterface through to the
appropriate actual implementation.
*/

class pqiperson: public PQInterface {
public:
    pqiperson(std::string id, unsigned int librarymixer_id, pqipersongrp *ppg);
    virtual ~pqiperson(); // must clean up children.

    // control of the connection.
    int reset();
    int listen();
    int stoplistening();
    int connect(uint32_t type, struct sockaddr_in raddr, uint32_t delay, uint32_t period, uint32_t timeout);

    //Add in connection method to kids.
    //type is a constant from pqibase.h and include PQI_CONNECT_TCP and PQI_CONNECT_UDP
    int addChildInterface(uint32_t type, pqiconnect *pqi);

    // The PQInterface interface.
    virtual int SendItem(NetItem *);
    virtual NetItem *GetItem();

    virtual int status();
    virtual int tick();

    // Called by kids to notify the pqiperson of a connection event.
    int notifyEvent(NetInterface *ni, int event);

    // PQInterface for rate control overloaded....
    virtual float getRate(bool in);
    virtual void setMaxRate(bool in, float val);

private:

    std::map<uint32_t, pqiconnect *> kids; //A type-of-connection/pqiconnect map.  The methods of communication.
    bool active;
    pqiconnect *activepqi;
    bool inConnectAttempt;
    int waittimes;
    pqipersongrp *pqipg; /* parent for callback */
};

/*
A method of communication to be used by the pqiperson.
A combination of a NetBinInterface with a pqistreamer.  Connection control
commands are passed directly to the NetInterface side of it, while PQInterface
events are handled by the pqistreamer half (which uses the BinInterface part
of the NetBinInterface).
*/
class pqiconnect: public pqistreamer, public NetInterface {
public:
    pqiconnect(Serialiser *rss, NetBinInterface *ni_in)
        :pqistreamer(rss, ni_in->PeerId(), ni_in->LibraryMixerId(), ni_in, 0),  // pqistreamer will cleanup NetInterface.
         NetInterface(NULL, ni_in->PeerId(), ni_in->LibraryMixerId()),     // No need for callback.
         ni(ni_in) {
        if (!ni_in) exit(1); //NetInterface cannot be NULL
        return;
    }

    virtual ~pqiconnect() {
        return;
    }

    // presents a virtual NetInterface -> passes to ni.
    virtual int connect(struct sockaddr_in raddr) {
        return ni->connect(raddr);
    }
    virtual int listen() {
        return ni -> listen();
    }
    virtual int stoplistening() {
        return ni -> stoplistening();
    }
    virtual int reset() {
        return ni -> reset();
    }
    virtual int disconnect() {
        return ni -> reset();
    }
    virtual bool connect_parameter(uint32_t type, uint32_t value) {
        return ni -> connect_parameter(type, value);
    }

    // get the contact from the net side.
    virtual std::string PeerId() {
        if (ni) {
            return ni->PeerId();
        } else {
            return PQInterface::PeerId();
        }
    }

    // to check if this is our interface.
    virtual bool thisNetInterface(NetInterface *compare) {
        return (compare == ni);
    }

    NetBinInterface *ni;
};

#endif

