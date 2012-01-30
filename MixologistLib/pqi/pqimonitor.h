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

#ifndef PQI_MONITOR_H
#define PQI_MONITOR_H

/**** Rough sketch of a Monitor class
 * expect it to change significantly
 *
 */

#include <inttypes.h>
#include <string>
#include <list>

#include <QString>

/************** Define Type/Mode/Source ***************/

/* STATE
   Used with PeerConnectionState to indicate the state of friends */
const uint32_t PEER_S_CONNECTED = 0x0002;
const uint32_t PEER_S_UNREACHABLE = 0x0004;
const uint32_t PEER_S_NO_CERT = 0x0008;

/* ACTIONS */
//Signal to pqipersongrp that a new peer has been added, so a new pqiperson must be created
const uint32_t PEER_NEW = 0x0001;
//Signal to ftcontroller that a connection attempt succeeded
const uint32_t PEER_CONNECTED = 0x0002;
//Signal to ftcontroller that a connection attempt failed
const uint32_t PEER_DISCONNECTED = 0x0004;
//Signal to pqipersongrp that it should try the connection again
const uint32_t PEER_CONNECT_REQ = 0x0008;
//Signal to pqipersongrp that an existing peer has been timed out, and their connection must be reset
const uint32_t PEER_TIMEOUT = 0x0010;

/* Stun Status Flags */
//const uint32_t STUN_SRC_DHT       = 0x0001;
//const uint32_t STUN_SRC_PEER  = 0x0002;
const uint32_t STUN_ONLINE      = 0x0010;
const uint32_t STUN_FRIEND      = 0x0020;
const uint32_t STUN_FRIEND_OF_FRIEND    = 0x0040;



#define CONNECT_PASSIVE     1
#define CONNECT_ACTIVE  2

#define CB_DHT      1   /* from dht */
//#define CB_DISC       2   /* from peers */
#define CB_PERSON       3   /* from connection */
#define CB_PROXY        4   /* via proxy */


class pqipeer {
public:
    unsigned int librarymixer_id;
    std::string cert_id;
    QString name;
    uint32_t    state;
    uint32_t    actions;
};

class p3ConnectMgr;

class pqiMonitor {
public:
    pqiMonitor() {return;}
    virtual ~pqiMonitor() {return;}

    /* Called by the p3ConnectMgr's tick function with a list of pqipeers whose statuses have changed. */
    virtual void statusChange(const std::list<pqipeer> &plist) = 0;
};




class pqiConnectCb {
public:
    virtual ~pqiConnectCb() {return;}

    virtual void    peerStatus(std::string id,
                               struct sockaddr_in laddr, struct sockaddr_in raddr,
                               uint32_t type, uint32_t flags, uint32_t source) = 0;

    virtual void    peerConnectRequest(std::string id,
                                       struct sockaddr_in raddr, uint32_t source) = 0;

    virtual void    stunStatus(std::string id, struct sockaddr_in raddr, uint32_t type, uint32_t flags) = 0;
};

#endif
