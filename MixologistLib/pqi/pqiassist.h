/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-7, Robert Fernie
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

#ifndef MRK_PQI_ASSIST_H
#define MRK_PQI_ASSIST_H

#include <string>
#include <map>
#include "pqi/pqinetwork.h"
#include "pqi/pqimonitor.h"

/**********
 * This header file provides two interfaces for assisting
 * the connections to friends.
 *
 * pqiNetAssistFirewall - which will provides interfaces
 * to functionality like upnp and apple's equivalent.
 *
 * pqiNetAssistConnect - which will provides interfaces
 * to other networks (DHT) etc that can provide information.
 * These classes would be expected to use the pqiMonitor
 * callback system to notify the connectionMgr.
 *
 ***/

class pqiNetAssist {
public:

    virtual ~pqiNetAssist() {
        return;
    }

    /* External Interface */
    virtual void    enable(bool on) = 0;
    virtual void    shutdown() = 0; /* blocking call */
    virtual void    restart() = 0;

    virtual bool    getEnabled() = 0;
    virtual bool    getActive() = 0;

};

class pqiNetAssistFirewall: public pqiNetAssist {
public:

    virtual ~pqiNetAssistFirewall() {
        return;
    }

    /* the address that the listening port is on */
    virtual void    setInternalPort(unsigned short iport_in) = 0;
    virtual void    setExternalPort(unsigned short eport_in) = 0;

    /* as determined by uPnP */
    virtual bool    getInternalAddress(struct sockaddr_in &addr) = 0;
    virtual bool    getExternalAddress(struct sockaddr_in &addr) = 0;

};




class pqiNetAssistConnect: public pqiNetAssist {
    /*
     */
public:
    pqiNetAssistConnect(std::string id, pqiConnectCb *cb)
        :mPeerId(id), mConnCb(cb) {
        return;
    }

    /********** External DHT Interface ************************
     * These Functions are the external interface
     * for the DHT, and must be non-blocking and return quickly
     */

    virtual void    setBootstrapAllowed(bool on) = 0;
    virtual bool    getBootstrapAllowed() = 0;

    /* set key data */
    virtual bool    setExternalInterface(struct sockaddr_in laddr,
                                         struct sockaddr_in raddr, uint32_t type) = 0;

    /* add / remove peers */
    virtual bool    findPeer(std::string id) = 0;
    virtual bool    dropPeer(std::string id) = 0;

    /* post DHT key saying we should connect (callback when done) */
    virtual bool    notifyPeer(std::string id) = 0;

    /* extract current peer status */
    virtual bool    getPeerStatus(std::string id,
                                  struct sockaddr_in &laddr, struct sockaddr_in &raddr,
                                  uint32_t &type, uint32_t &mode) = 0;

    /* stun */
    virtual bool    enableStun(bool on)     = 0;
    virtual bool    addStun(std::string id)     = 0;

protected:
    std::string  mPeerId;
    pqiConnectCb *mConnCb;
};

#endif /* MRK_PQI_ASSIST_H */

