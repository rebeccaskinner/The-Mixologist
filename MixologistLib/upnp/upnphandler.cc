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

/* This stuff is actually C */

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef  __cplusplus
} /* extern C */
#endif
/* This stuff is actually C */

#include "upnp/upnphandler.h"
#include "upnp/upnputil.h"
#include "util/debug.h"

class uPnPConfigData {
public:
    struct UPNPDev *devlist;
    struct UPNPUrls urls;
    struct IGDdatas data;
    char lanaddr[16];   /* my ip address on the LAN */
};

#include <iostream>
#include <sstream>

#include "util/net.h"

bool upnphandler::initUPnPState() {
    /* allocate memory */
    uPnPConfigData *upcd = new uPnPConfigData;

    upcd->devlist = upnpDiscover(2000, NULL, NULL, 0);

    if(upcd->devlist) {
        struct UPNPDev *device;
        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "List of UPNP devices found on the network:");
        for(device=upcd->devlist; device; device=device->pNext) {
            log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "desc: " + QString(device->descURL) + " st: " + QString(device->st));
        }
        putchar('\n');
        if(UPNP_GetValidIGD(upcd->devlist, &(upcd->urls),
                            &(upcd->data), upcd->lanaddr,
                            sizeof(upcd->lanaddr))) {
            log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Found valid IGD: " + QString(upcd->urls.controlURL));
            log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Local LAN ip address: " + QString(upcd->lanaddr));

            /* MODIFY STATE */
            dataMtx.lock(); /* LOCK MUTEX */

            /* convert to ipaddress. */
            inet_aton(upcd->lanaddr, &(upnp_iaddr.sin_addr));
            upnp_iaddr.sin_port = htons(iport);

            upnpState = UPNP_S_READY;
            if (upnpConfig) {
                delete upnpConfig;
            }
            upnpConfig = upcd;   /* */

            dataMtx.unlock(); /* UNLOCK MUTEX */


            /* done -> READY */
            return 1;

        } else {
            log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "No valid UPNP Internet Gateway Device found.");
        }


        freeUPNPDevlist(upcd->devlist);
        upcd->devlist = 0;
    } else {
        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "No IGD UPnP Device found on the network!");
    }

    /* MODIFY STATE */
    dataMtx.lock(); /* LOCK MUTEX */

    upnpState = UPNP_S_UNAVAILABLE;
    delete upcd;
    upnpConfig = NULL;

    dataMtx.unlock(); /* UNLOCK MUTEX */

    /* done, FAILED -> NOT AVAILABLE */

    return 0;
}

bool upnphandler::printUPnPState() {
    MixStackMutex stack(dataMtx); /* LOCK STACK MUTEX */

    uPnPConfigData *config = upnpConfig;
    if ((upnpState >= UPNP_S_READY) && (config)) {
        DisplayInfos(&(config -> urls), &(config->data));
        GetConnectionStatus(&(config -> urls), &(config->data));
        ListRedirections(&(config -> urls), &(config->data));
    } else log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "UPNP not ready");

    return 1;
}


bool upnphandler::checkUPnPActive() {
    MixStackMutex stack(dataMtx); /* LOCK STACK MUTEX */

    uPnPConfigData *config = upnpConfig;
    if ((upnpState > UPNP_S_READY) && (config)) {
        char eprot1[] = "TCP";
        char eprot2[] = "UDP";

        char in_addr[256];
        char in_port1[256];
        char in_port2[256];
        char eport1[256];
        char eport2[256];

        struct sockaddr_in localAddr = upnp_iaddr;
        uint32_t linaddr = ntohl(localAddr.sin_addr.s_addr);

        snprintf(in_port1, 256, "%d", ntohs(localAddr.sin_port));
        snprintf(in_port2, 256, "%d", ntohs(localAddr.sin_port));

        snprintf(in_addr, 256, "%d.%d.%d.%d",
                 ((linaddr >> 24) & 0xff),
                 ((linaddr >> 16) & 0xff),
                 ((linaddr >> 8) & 0xff),
                 ((linaddr >> 0) & 0xff));

        snprintf(eport1, 256, "%d", eport_curr);
        snprintf(eport2, 256, "%d", eport_curr);

        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE,
            "UPnPHandler::checkUPnPState Checking Redirection: InAddr: " + QString(in_addr) +
            " InPort: " + QString(in_port1) +
            " ePort: " + QString(eport1) +
            " eProt: " + QString(eprot1));

        bool tcpOk = TestRedirect(&(config -> urls), &(config->data),
                                  in_addr, in_port1, eport1, eprot1);
        bool udpOk = TestRedirect(&(config -> urls), &(config->data),
                                  in_addr, in_port2, eport2, eprot2);

        if ((!tcpOk) || (!udpOk)) {
            log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "upnphandler::checkUPnPState() ... Redirect Expired, restarting");

            toStop = true;
            toStart = true;
        }
    }

    return true;
}

class upnpThreadData {
public:
    upnphandler *handler;
    bool start;
    bool stop;
};

/* Thread routines */
extern "C" void *doSetupUPnP(void *p) {
    upnpThreadData *data = (upnpThreadData *) p;
    if ((!data) || (!data->handler)) {
        pthread_exit(NULL);
    }

    /* publish it! */
    if (data -> stop) {
        data->handler->shutdown_upnp();
    }

    if (data -> start) {
        data->handler->initUPnPState();
        data->handler->start_upnp();
    }

    data->handler->printUPnPState();

    delete data;
    pthread_exit(NULL);

    return NULL;
}

bool upnphandler::background_setup_upnp(bool start, bool stop) {
    pthread_t tid;

    /* launch thread */
    upnpThreadData *data = new upnpThreadData();
    data->handler = this;
    data->start = start;
    data->stop = stop;

    pthread_create(&tid, 0, &doSetupUPnP, (void *) data);
    pthread_detach(tid); /* so memory is reclaimed in linux */

    return true;
}

bool upnphandler::start_upnp() {
    MixStackMutex stack(dataMtx); /* LOCK STACK MUTEX */

    uPnPConfigData *config = upnpConfig;
    if (!((upnpState >= UPNP_S_READY) && (config))) {
        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "upnphandler::start_upnp() Not Ready");
        return false;
    }

    char eprot1[] = "TCP";
    char eprot2[] = "UDP";

    /* if we're to load -> load */
    /* select external ports */
    eport_curr = eport;
    if (!eport_curr) {
        /* use local port if eport is zero */
        eport_curr = iport;
        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "Using LocalPort for extPort!");
    }

    if (!eport_curr) {
        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "Invalid eport ... ");
        return false;
    }


    /* our port */
    char in_addr[256];
    char in_port1[256];
    char in_port2[256];
    char eport1[256];
    char eport2[256];

    upnp_iaddr.sin_port = htons(iport);
    struct sockaddr_in localAddr = upnp_iaddr;
    uint32_t linaddr = ntohl(localAddr.sin_addr.s_addr);

    snprintf(in_port1, 256, "%d", ntohs(localAddr.sin_port));
    snprintf(in_port2, 256, "%d", ntohs(localAddr.sin_port));
    snprintf(in_addr, 256, "%d.%d.%d.%d",
             ((linaddr >> 24) & 0xff),
             ((linaddr >> 16) & 0xff),
             ((linaddr >> 8) & 0xff),
             ((linaddr >> 0) & 0xff));

    snprintf(eport1, 256, "%d", eport_curr);
    snprintf(eport2, 256, "%d", eport_curr);

    log(LOG_DEBUG_ALERT, UPNPHANDLERZONE,
        "Attempting Redirection: InAddr: " + QString(in_addr) +
        " InPort: " + QString(in_port1) +
        " ePort: " + QString(eport1) +
        " eProt: " + QString(eprot1));

    if (!SetRedirectAndTest(&(config -> urls), &(config->data),
                            in_addr, in_port1, eport1, eprot1)) {
        upnpState = UPNP_S_TCP_FAILED;
    } else if (!SetRedirectAndTest(&(config -> urls), &(config->data),
                                   in_addr, in_port2, eport2, eprot2)) {
        upnpState = UPNP_S_UDP_FAILED;
    } else {
        upnpState = UPNP_S_ACTIVE;
    }


    /* now store the external address */
    char externalIPAddress[32];
    UPNP_GetExternalIPAddress(config -> urls.controlURL,
                              //config->data.first.servicetype,
                              config->data.servicetype,
                              externalIPAddress);

    sockaddr_clear(&upnp_eaddr);

    if(externalIPAddress[0]) {
        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Stored external address: " + QString(externalIPAddress) + ":" + QString::number(eport_curr));

        inet_aton(externalIPAddress, &(upnp_eaddr.sin_addr));
        upnp_eaddr.sin_family = AF_INET;
        upnp_eaddr.sin_port = htons(eport_curr);
    } else {
        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Failed to get external address");
    }

    toStart = false;

    return true;

}

bool upnphandler::shutdown_upnp() {
    MixStackMutex stack(dataMtx); /* LOCK STACK MUTEX */

    uPnPConfigData *config = upnpConfig;
    if (!((upnpState >= UPNP_S_READY) && (config))) {
        return false;
    }

    char eprot1[] = "TCP";
    char eprot2[] = "UDP";

    /* always attempt this (unless no port number) */
    if (eport_curr > 0) {

        char eport1[256];
        char eport2[256];

        snprintf(eport1, 256, "%d", eport_curr);
        snprintf(eport2, 256, "%d", eport_curr);

        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Attempting to remove redirection port: " + QString(eprot1));

        RemoveRedirect(&(config -> urls), &(config->data),
                       eport1, eprot1);

        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Attempting to remove redirection port: " + QString(eprot2));

        RemoveRedirect(&(config -> urls), &(config->data),
                       eport2, eprot2);

        upnpState = UPNP_S_READY;
        toStop = false;
    }

    return true;

}

/************************ External Interface *****************************
 *
 *
 *
 */

upnphandler::upnphandler()
    :toEnable(false), toStart(false), toStop(false),
    eport(0), eport_curr(0),
    upnpState(UPNP_S_UNINITIALISED),
    upnpConfig(NULL) {
    return;
}

upnphandler::~upnphandler() {
    return;
}

/* Interface */
void  upnphandler::enable(bool active) {
    dataMtx.lock();   /***  LOCK MUTEX  ***/

    if (active != toEnable) {
        if (active) {
            toStart = true;
        } else {
            toStop = true;
        }
    }
    toEnable = active;

    bool start = toStart;

    dataMtx.unlock(); /*** UNLOCK MUTEX ***/

    if (start) {
        /* make background thread to startup UPnP */
        background_setup_upnp(true, false);
    }


}


void    upnphandler::shutdown() {
    /* blocking call to shutdown upnp */

    shutdown_upnp();
}


void    upnphandler::restart() {
    /* non-blocking call to shutdown upnp, and startup again. */
    background_setup_upnp(true, true);
}



bool    upnphandler::getEnabled() {
    // no need to lock for reading a boolean
    return toEnable;
}

bool    upnphandler::getActive() {
    // no need to lock for reading a boolean
    return (upnpState == UPNP_S_ACTIVE);
}

/* the address that the listening port is on */
void    upnphandler::setInternalPort(unsigned short iport_in) {
    dataMtx.lock();   /***  LOCK MUTEX  ***/

    log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "UPnPHandler::setInternalPort(" + QString::number(iport_in) + ") current port: " + QString::number(iport));

    if (iport != iport_in) {
        iport = iport_in;
        if ((toEnable) &&
                (upnpState == UPNP_S_ACTIVE)) {
            toStop  = true;
            toStart = true;
        }
    }
    dataMtx.unlock(); /*** UNLOCK MUTEX ***/
}

void    upnphandler::setExternalPort(unsigned short eport_in) {
    dataMtx.lock();   /***  LOCK MUTEX  ***/

    log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "UPnPHandler::setExternalPort(" + QString::number(eport_in) + ") current port: " + QString::number(eport));

    /* flag both shutdown/start -> for restart */
    if (eport != eport_in) {
        eport = eport_in;
        if ((toEnable) &&
                (upnpState == UPNP_S_ACTIVE)) {
            toStop  = true;
            toStart = true;
        }
    }

    dataMtx.unlock(); /*** UNLOCK MUTEX ***/
}

/* as determined by uPnP */
bool    upnphandler::getInternalAddress(struct sockaddr_in &addr) {
    dataMtx.lock();   /***  LOCK MUTEX  ***/

    log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "UPnPHandler::getInternalAddress()");

    addr = upnp_iaddr;
    bool valid = (upnpState >= UPNP_S_ACTIVE);

    dataMtx.unlock(); /*** UNLOCK MUTEX ***/

    return valid;
}

bool    upnphandler::getExternalAddress(struct sockaddr_in &addr) {
    dataMtx.lock();   /***  LOCK MUTEX  ***/

    log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "UPnPHandler::getExternalAddress()");
    addr = upnp_eaddr;
    bool valid = (upnpState == UPNP_S_ACTIVE);

    dataMtx.unlock(); /*** UNLOCK MUTEX ***/

    return valid;
}


