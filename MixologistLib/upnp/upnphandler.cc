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
#include "util/net.h"

#include <QThread>

upnphandler::upnphandler()
    :enabled(false), upnpState(UPNP_S_UNINITIALISED), upnpConfig(NULL), desiredExternalPort(0), currentExternalPort(0){}

/* Interface */
void  upnphandler::enable(bool on) {
    bool start = false;
    bool stop = false;
    {
        QMutexLocker stack(&upnpMtx);

        if (enabled == on) return;

        if (on) start = true;
        else stop = true;

        enabled = on;
    }

    if (start) UPnPAsynchronizer::createWorker(this)->requestStart();
    else if (stop) UPnPAsynchronizer::createWorker(this)->requestStop();
}


void upnphandler::shutdown() {
    shutdown_upnp();
}


void upnphandler::restart() {
    UPnPAsynchronizer::createWorker(this)->requestRestart();;
}

bool upnphandler::getEnabled() {
    //No need to lock for reading a boolean
    return enabled;
}

bool upnphandler::getActive() {
    QMutexLocker stack(&upnpMtx);

    //If we aren't ready yet, we can just return false right off
    if (upnpState != UPNP_S_ACTIVE) return false;

    //However, if we believe we are active, we should recheck to make sure everything is still working
    UPnPConfigData *config = upnpConfig;
    //Sanity check
    if (!config || (upnpState <= UPNP_S_READY));

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

    snprintf(externalPortTCP, 256, "%d", currentExternalPort);
    snprintf(externalPortUDP, 256, "%d", currentExternalPort);

    log(LOG_DEBUG_BASIC, UPNPHANDLERZONE,
        "UPnPHandler::checkUPnPState Checking Redirection: InAddr: " + QString(internalAddress) +
        " InPort: " + QString(internalPortTCP) +
        " ePort: " + QString(externalPortTCP) +
        " eProt: TCP");

    bool tcpOk = TestRedirect(&(config->urls), &(config->data), internalAddress, internalPortTCP, externalPortTCP, "TCP");
    bool udpOk = TestRedirect(&(config->urls), &(config->data), internalAddress, internalPortUDP, externalPortUDP, "UDP");

    if (!tcpOk) {
        upnpState = UPNP_S_TCP_FAILED;
    } else if (!udpOk) {
        upnpState = UPNP_S_UDP_FAILED;
    }

    if ((!tcpOk) || (!udpOk)) {
        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "upnphandler::checkUPnPState() ... Redirect Expired, restarting");

        return false;
    }

    return true;
}

void upnphandler::setInternalPort(unsigned short newPort) {
    upnpMtx.lock();

    log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "UPnPHandler::setInternalPort(" + QString::number(newPort) + ") current port: " + QString::number(internalPort));

    if (internalPort != newPort) {
        internalPort = newPort;
        if (enabled && (upnpState == UPNP_S_ACTIVE)) {
            this->restart();
        }
    }
    upnpMtx.unlock();
}

void upnphandler::setExternalPort(unsigned short newPort) {
    upnpMtx.lock();

    log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "UPnPHandler::setExternalPort(" + QString::number(newPort) + ") current port: " + QString::number(desiredExternalPort));

    /* flag both shutdown/start->for restart */
    if (desiredExternalPort != newPort) {
        desiredExternalPort = newPort;
        if (enabled && (upnpState == UPNP_S_ACTIVE)) {
            this->restart();
        }
    }

    upnpMtx.unlock();
}

/* as determined by uPnP */
bool upnphandler::getInternalAddress(struct sockaddr_in &addr) {
    upnpMtx.lock();

    log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "UPnPHandler::getInternalAddress()");

    addr = upnp_internalAddress;
    bool valid = (upnpState >= UPNP_S_ACTIVE);

    upnpMtx.unlock();

    return valid;
}

bool upnphandler::getExternalAddress(struct sockaddr_in &addr) {
    upnpMtx.lock();

    log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "UPnPHandler::getExternalAddress()");
    addr = upnp_externalAddress;
    bool valid = (upnpState == UPNP_S_ACTIVE);

    upnpMtx.unlock();

    return valid;
}

bool upnphandler::initUPnPState() {
    /* allocate memory */
    UPnPConfigData *newConfigData = new UPnPConfigData;

    newConfigData->devlist = upnpDiscover(2000, NULL, NULL, 0);

    if(newConfigData->devlist) {
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

            /* MODIFY STATE */
            {
                QMutexLocker stack(&upnpMtx);
                /* convert to ipaddress. */
                inet_aton(newConfigData->lanaddr, &(upnp_internalAddress.sin_addr));
                upnp_internalAddress.sin_port = htons(internalPort);

                upnpState = UPNP_S_READY;

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
        upnpState = UPNP_S_UNAVAILABLE;
        if (upnpConfig) delete upnpConfig;
        upnpConfig = NULL;
    }

    return false;
}

void upnphandler::printUPnPState() {
    QMutexLocker stack(&upnpMtx);

    UPnPConfigData *config = upnpConfig;
    if ((upnpState >= UPNP_S_READY) && (config)) {
        DisplayInfos(&(config->urls), &(config->data));
        GetConnectionStatus(&(config->urls), &(config->data));
        ListRedirections(&(config->urls), &(config->data));
    } else log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "UPNP not ready");

    return;
}

bool upnphandler::start_upnp() {
    QMutexLocker stack(&upnpMtx);

    UPnPConfigData *config = upnpConfig;
    if (!config || (upnpState < UPNP_S_READY)) {
        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "upnphandler::start_upnp() Not Ready");
        return false;
    }

    if (desiredExternalPort != 0) {
        currentExternalPort = desiredExternalPort;
    } else if (internalPort != 0) {
        currentExternalPort = internalPort;
        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "Using internalPort for externalPort!");
    } else {
        log(LOG_DEBUG_BASIC, UPNPHANDLERZONE, "Unable to set externalPort");
        return false;
    }

    /* our port */
    char internalAddress[256];
    char internalPortTCP[256];
    char internalPortUDP[256];
    char externalPortTCP[256];
    char externalPortUDP[256];

    upnp_internalAddress.sin_port = htons(internalPort);
    struct sockaddr_in localAddress = upnp_internalAddress;
    uint32_t numericIP = ntohl(localAddress.sin_addr.s_addr);

    snprintf(internalPortTCP, 256, "%d", ntohs(localAddress.sin_port));
    snprintf(internalPortUDP, 256, "%d", ntohs(localAddress.sin_port));
    snprintf(internalAddress, 256, "%d.%d.%d.%d",
             ((numericIP >> 24) & 0xff),
             ((numericIP >> 16) & 0xff),
             ((numericIP >> 8) & 0xff),
             ((numericIP >> 0) & 0xff));

    snprintf(externalPortTCP, 256, "%d", currentExternalPort);
    snprintf(externalPortUDP, 256, "%d", currentExternalPort);

    log(LOG_DEBUG_ALERT, UPNPHANDLERZONE,
        "Attempting Redirection: Internal Address: " + QString(internalAddress) +
        " Internal Port: " + QString(internalPortTCP) +
        " External Port: " + QString(externalPortTCP) +
        " External Protocol: TCP");

    if (!SetRedirectAndTest(&(config->urls), &(config->data), internalAddress, internalPortTCP, externalPortTCP, "TCP")) {
        upnpState = UPNP_S_TCP_FAILED;
    } else if (!SetRedirectAndTest(&(config->urls), &(config->data), internalAddress, internalPortUDP, externalPortUDP, "UDP")) {
        upnpState = UPNP_S_UDP_FAILED;
    } else {
        upnpState = UPNP_S_ACTIVE;
    }

    /* now store the external address */
    char externalIPAddress[32];
    UPNP_GetExternalIPAddress(config->urls.controlURL, config->data.servicetype, externalIPAddress);

    sockaddr_clear(&upnp_externalAddress);

    if(externalIPAddress[0]) {
        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Stored external address: " + QString(externalIPAddress) + ":" + QString::number(currentExternalPort));

        inet_aton(externalIPAddress, &(upnp_externalAddress.sin_addr));
        upnp_externalAddress.sin_family = AF_INET;
        upnp_externalAddress.sin_port = htons(currentExternalPort);
    } else {
        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Failed to get external address");
    }

    return true;
}

bool upnphandler::shutdown_upnp() {
    QMutexLocker stack(&upnpMtx);

    UPnPConfigData *config = upnpConfig;
    if (!config || (upnpState < UPNP_S_READY)) {
        return false;
    }

    /* always attempt this (unless no port number) */
    if (currentExternalPort > 0) {

        char externalPortTCP[256];
        char externalPortUDP[256];

        snprintf(externalPortTCP, 256, "%d", currentExternalPort);
        snprintf(externalPortUDP, 256, "%d", currentExternalPort);

        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Attempting to remove redirection port: TCP");

        RemoveRedirect(&(config->urls), &(config->data), externalPortTCP, "TCP");

        log(LOG_DEBUG_ALERT, UPNPHANDLERZONE, "Attempting to remove redirection port: UDP");

        RemoveRedirect(&(config->urls), &(config->data), externalPortUDP, "UDP");

        upnpState = UPNP_S_READY;
    }

    return true;
}

UPnPAsynchronizer::UPnPAsynchronizer(upnphandler* handler)
    :handler(handler){
    this->connect(this, SIGNAL(startRequested()), this, SLOT(startUPnP()), Qt::QueuedConnection);
    this->connect(this, SIGNAL(stopRequested(bool)), this, SLOT(stopUPnP(bool)), Qt::QueuedConnection);
}

UPnPAsynchronizer* UPnPAsynchronizer::createWorker(upnphandler* handler) {
    UPnPAsynchronizer *asynchronousWorker = new UPnPAsynchronizer(handler);
    QThread *thread = new QThread();
    asynchronousWorker->moveToThread(thread);
    thread->connect(asynchronousWorker, SIGNAL(workCompleted()), thread, SLOT(quit()));
    thread->connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    thread->start();
    return asynchronousWorker;
}

void UPnPAsynchronizer::requestStart() {
    emit startRequested();
}

void UPnPAsynchronizer::requestStop() {
    emit stopRequested(true);
}

void UPnPAsynchronizer::requestRestart() {
    emit stopRequested(false);
    emit startRequested();
}

void UPnPAsynchronizer::startUPnP() {
    handler->initUPnPState();
    handler->start_upnp();

    handler->printUPnPState();

    /* Asynchronous call to start completed, destroy the asynchronizer and its thread. */
    emit workCompleted();
    this->deleteLater();
}

void UPnPAsynchronizer::stopUPnP(bool emitCompletion) {
    handler->shutdown_upnp();

    handler->printUPnPState();

    if (emitCompletion) {
        emit workCompleted();
        this->deleteLater();
    }
}
