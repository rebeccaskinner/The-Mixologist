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

#include "upnp/upnphandler.h"
#include "util/debug.h"
#include <pqi/pqinetwork.h>

#include <QThread>

upnpHandler::upnpHandler()
    :upnpState(UPNP_STATE_UNINITIALISED), upnpConfig(NULL), targetPort(0){}

/* Interface */
void  upnpHandler::startup() {
    UPnPAsynchronizer::createWorker(this)->requestStart();
}


void upnpHandler::shutdown() {
    shutdown_upnp();
}

void upnpHandler::restart() {
    UPnPAsynchronizer::createWorker(this)->requestRestart();;
}

upnpHandler::upnpStates upnpHandler::getUpnpState() {
    QMutexLocker stack(&upnpMtx);

    /* If we aren't active yet, just return the non-operational state right off the bat. */
    if (upnpState != UPNP_STATE_ACTIVE) return upnpState;

    /* However, if we believe we are active, we should recheck to make sure everything is still working. */

    char internalAddress[256];
    char internalPortTCP[256];
    char internalPortUDP[256];
    char externalPortTCP[256];
    char externalPortUDP[256];

    struct sockaddr_in localAddr = upnp_internalAddress;
    uint32_t numericIP = ntohl(localAddr.sin_addr.s_addr);

    snprintf(internalPortTCP, 256, "%d", ntohs(localAddr.sin_port));
    snprintf(internalPortUDP, 256, "%d", ntohs(localAddr.sin_port));

    snprintf(internalAddress, 256, "%d.%d.%d.%d",
             ((numericIP >> 24) & 0xff),
             ((numericIP >> 16) & 0xff),
             ((numericIP >> 8) & 0xff),
             ((numericIP >> 0) & 0xff));

    snprintf(externalPortTCP, 256, "%d", targetPort);
    snprintf(externalPortUDP, 256, "%d", targetPort);

    bool tcpOk = TestRedirect(&(upnpConfig->urls), &(upnpConfig->data), internalAddress, internalPortTCP, externalPortTCP, "TCP");
    bool udpOk = TestRedirect(&(upnpConfig->urls), &(upnpConfig->data), internalAddress, internalPortUDP, externalPortUDP, "UDP");

    if ((!tcpOk) || (!udpOk)) {
        upnpState = UPNP_STATE_FAILED;
        log(LOG_WARNING, UPNPHANDLERZONE, "Detected Universal Plug and Play mapping has been removed");
    }

    return upnpState;
}

void upnpHandler::setTargetPort(unsigned short newPort) {
    QMutexLocker stack(&upnpMtx);
    targetPort = newPort;
}

#define UPNP_DISCOVERY_TIMEOUT 2000
bool upnpHandler::initUPnPState() {
    /* allocate memory */
    UPnPConfigData *newConfigData = new UPnPConfigData;

    int error;
    newConfigData->devlist = upnpDiscover(UPNP_DISCOVERY_TIMEOUT, NULL, NULL, NULL, NULL, &error);

    if (error != UPNPDISCOVER_SUCCESS) {
        log(LOG_ERROR, UPNPHANDLERZONE, "Error initializing UPNP discovery, error code: " + QString::number(error));
        return false;
    }

    if (newConfigData->devlist) {
        struct UPNPDev *device;
        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "List of UPNP devices found on the network:");
        for(device=newConfigData->devlist; device; device=device->pNext) {
            log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "desc: " + QString(device->descURL) + " st: " + QString(device->st));
        }

        /* Search for an Internet Gateway Device that we can configure with. */
        if(UPNP_GetValidIGD(newConfigData->devlist, &(newConfigData->urls), &(newConfigData->data),
                            newConfigData->lanaddr, sizeof(newConfigData->lanaddr))) {
            log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Found valid IGD: " + QString(newConfigData->urls.controlURL));
            log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Local LAN ip address: " + QString(newConfigData->lanaddr));
            {
                QMutexLocker stack(&upnpMtx);
                /* convert to ipaddress. */
                inet_aton(newConfigData->lanaddr, &(upnp_internalAddress.sin_addr));
                upnp_internalAddress.sin_port = htons(targetPort);

                upnpState = UPNP_STATE_READY;

                if (upnpConfig) delete upnpConfig;
                upnpConfig = newConfigData;
            }

            return true;

        } else {
            log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "No valid UPNP Internet Gateway Device found.");
        }

        freeUPNPDevlist(newConfigData->devlist);
        newConfigData->devlist = 0;
    } else {
        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "No UPnP Devices found on the network!");
    }

    /* Failure - Cleanup */
    delete newConfigData;

    {
        QMutexLocker stack(&upnpMtx);
        upnpState = UPNP_STATE_UNAVAILABLE;
        if (upnpConfig) delete upnpConfig;
        upnpConfig = NULL;
    }

    return false;
}

void upnpHandler::printUPnPState() {
    QMutexLocker stack(&upnpMtx);

    if (!upnpConfig || upnpState == UPNP_STATE_UNINITIALISED || upnpState == UPNP_STATE_UNAVAILABLE) return;

    DisplayInfos(&(upnpConfig->urls), &(upnpConfig->data));
    GetConnectionStatus(&(upnpConfig->urls), &(upnpConfig->data));
    ListRedirections(&(upnpConfig->urls), &(upnpConfig->data));
}

bool upnpHandler::start_upnp() {
    QMutexLocker stack(&upnpMtx);

    if (!upnpConfig || upnpState == UPNP_STATE_UNINITIALISED || upnpState == UPNP_STATE_UNAVAILABLE) {
        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "upnpHandler::start_upnp() Not Ready");
        return false;
    }

    if (targetPort == 0) {
        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "Unable to set externalPort");
        return false;
    }

    char internalAddress[256];
    char internalPortTCP[256];
    char internalPortUDP[256];
    char externalPortTCP[256];
    char externalPortUDP[256];

    upnp_internalAddress.sin_port = htons(targetPort);
    struct sockaddr_in localAddress = upnp_internalAddress;
    uint32_t numericIP = ntohl(localAddress.sin_addr.s_addr);

    snprintf(internalPortTCP, 256, "%d", ntohs(localAddress.sin_port));
    snprintf(internalPortUDP, 256, "%d", ntohs(localAddress.sin_port));
    snprintf(internalAddress, 256, "%d.%d.%d.%d",
             ((numericIP >> 24) & 0xff),
             ((numericIP >> 16) & 0xff),
             ((numericIP >> 8) & 0xff),
             ((numericIP >> 0) & 0xff));

    snprintf(externalPortTCP, 256, "%d", targetPort);
    snprintf(externalPortUDP, 256, "%d", targetPort);

    log(LOG_DEBUG_ALERT, UPNPHANDLERZONE,
        "Attempting Redirection: Internal Address: " + QString(internalAddress) +
        " Internal Port: " + QString(internalPortTCP) +
        " External Port: " + QString(externalPortTCP) +
        " External Protocol: TCP");

    if (!SetRedirectAndTest(&(upnpConfig->urls), &(upnpConfig->data), internalAddress, internalPortTCP, externalPortTCP, "TCP")) {
        log(LOG_WARNING, UPNPHANDLERZONE, "Unable to set up Universal Plug and Play redirect on TCP");
        upnpState = UPNP_STATE_FAILED;
    } else if (!SetRedirectAndTest(&(upnpConfig->urls), &(upnpConfig->data), internalAddress, internalPortUDP, externalPortUDP, "UDP")) {
        log(LOG_WARNING, UPNPHANDLERZONE, "Unable to set up Universal Plug and Play redirect on UDP");
        upnpState = UPNP_STATE_FAILED;
    } else {
        upnpState = UPNP_STATE_ACTIVE;
    }

    /* We don't use the external address for anything at the moment.
       UPNP external address is going to be wrong in the case of double NAT.
       Therefore, all of our external address determination happens using STUN, which is always correct.
    char externalIPAddress[32];
    UPNP_GetExternalIPAddress(config->urls.controlURL, config->data.servicetype, externalIPAddress);*/

    return true;
}

bool upnpHandler::shutdown_upnp() {
    QMutexLocker stack(&upnpMtx);

    if (!upnpConfig || upnpState == UPNP_STATE_UNINITIALISED || upnpState == UPNP_STATE_UNAVAILABLE) {
        return false;
    }

    /* always attempt this (unless no port number) */
    if (targetPort > 0) {

        char externalPortTCP[256];
        char externalPortUDP[256];

        snprintf(externalPortTCP, 256, "%d", targetPort);
        snprintf(externalPortUDP, 256, "%d", targetPort);

        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Attempting to remove redirection port: TCP");

        RemoveRedirect(&(upnpConfig->urls), &(upnpConfig->data), externalPortTCP, "TCP");

        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Attempting to remove redirection port: UDP");

        RemoveRedirect(&(upnpConfig->urls), &(upnpConfig->data), externalPortUDP, "UDP");

        upnpState = UPNP_STATE_READY;
    }

    return true;
}

UPnPAsynchronizer* UPnPAsynchronizer::createWorker(upnpHandler* handler) {
    UPnPAsynchronizer *asynchronousWorker = new UPnPAsynchronizer(handler);
    QThread *thread = new QThread();
    asynchronousWorker->moveToThread(thread);
    thread->connect(asynchronousWorker, SIGNAL(workCompleted()), thread, SLOT(quit()));
    thread->connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    thread->start();
    return asynchronousWorker;
}

UPnPAsynchronizer::UPnPAsynchronizer(upnpHandler* handler)
    :handler(handler){
    this->connect(this, SIGNAL(startRequested()), this, SLOT(startUPnP()), Qt::QueuedConnection);
    this->connect(this, SIGNAL(stopRequested()), this, SLOT(stopUPnP()), Qt::QueuedConnection);
}

void UPnPAsynchronizer::requestStart() {
    emit startRequested();
}

void UPnPAsynchronizer::requestRestart() {
    emit stopRequested();
    emit startRequested();
}

void UPnPAsynchronizer::startUPnP() {
    if (handler->initUPnPState()) handler->start_upnp();

    handler->printUPnPState();

    /* The completion of StartUPnP at the moment always means that the thread is finished, since it is only called from requestStart and requestRestart.
       Therefore, for now finishing here means we can destroy the asynchronizer and its thread. */
    emit workCompleted();
    this->deleteLater();
}

void UPnPAsynchronizer::stopUPnP() {
    handler->shutdown_upnp();

    handler->printUPnPState();
}
