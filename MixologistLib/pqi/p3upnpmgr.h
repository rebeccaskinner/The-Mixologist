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


#ifndef MRK_P3_UPNP_MANAGER_H
#define MRK_P3_UPNP_MANAGER_H

/* platform independent networking... */
#include "pqi/pqinetwork.h"

class p3UpnpMgr: public pqiNetAssistFirewall {
public:

    virtual ~p3UpnpMgr() {
        return;
    }

    /* External Interface */
    virtual void    enable(bool on) = 0;  /* launches thread to start it up */
    virtual void    shutdown() = 0;       /* blocking shutdown call */
    virtual void    restart() = 0;        /* must be called if ports change */

    virtual bool    getEnabled() = 0;
    virtual bool    getActive() = 0;

    /* the address that the listening port is on */
    virtual void    setInternalPort(unsigned short iport_in) = 0;
    virtual void    setExternalPort(unsigned short eport_in) = 0;

    /* as determined by uPnP */
    virtual bool    getInternalAddress(struct sockaddr_in &addr) = 0;
    virtual bool    getExternalAddress(struct sockaddr_in &addr) = 0;

};

#endif /* MRK_P3_UPNP_MANAGER_H */

