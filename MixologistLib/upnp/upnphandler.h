/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
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

#ifndef UPNP_IFACE_H
#define UPNP_IFACE_H

#include <string.h>

#include <string>
#include <map>

/* platform independent networking... */
#include "pqi/pqinetwork.h"
#include "pqi/pqiassist.h"

#include "util/threads.h"

class upnpentry {
public:
    std::string name;
    std::string id;
    struct sockaddr_in addr;
    unsigned int flags;
    int status;
    int lastTs;
};

class upnpforward {
public:
    std::string name;
    unsigned int flags;
    struct sockaddr_in iaddr;
    struct sockaddr_in eaddr;
    int status;
    int lastTs;
};


#define UPNP_S_UNINITIALISED  0
#define UPNP_S_UNAVAILABLE    1
#define UPNP_S_READY          2
#define UPNP_S_TCP_FAILED     3
#define UPNP_S_UDP_FAILED     4
#define UPNP_S_ACTIVE         5

class uPnPConfigData;

class upnphandler: public pqiNetAssistFirewall {
public:

    upnphandler();
    virtual ~upnphandler();

    /* External Interface (pqiNetAssistFirewall) */
    virtual void    enable(bool active);
    virtual void    shutdown();
    virtual void    restart();

    virtual bool    getEnabled();
    virtual bool    getActive();

    virtual void    setInternalPort(unsigned short iport_in);
    virtual void    setExternalPort(unsigned short eport_in);
    virtual bool    getInternalAddress(struct sockaddr_in &addr);
    virtual bool    getExternalAddress(struct sockaddr_in &addr);

    /* Public functions - for background thread operation,
         * but effectively private from rest of program, as in derived class
     */

    bool    start_upnp();
    bool    shutdown_upnp();

    bool initUPnPState();
    bool printUPnPState();

private:

    bool background_setup_upnp(bool, bool);
    bool checkUPnPActive();

    /* Mutex for data below */
    MixMutex dataMtx;

    bool toEnable;   /* overall on/off switch */
    bool toStart;  /* if set start forwarding */
    bool toStop;   /* if set stop  forwarding */

    unsigned short iport;
    unsigned short eport;       /* config            */
    unsigned short eport_curr;  /* current forwarded */

    /* info from upnp */
    unsigned int upnpState;
    uPnPConfigData *upnpConfig;

    struct sockaddr_in upnp_iaddr;
    struct sockaddr_in upnp_eaddr;

    /* active port forwarding */
    std::list<upnpforward> activeForwards;

};

#endif /* UPNP_IFACE_H */
