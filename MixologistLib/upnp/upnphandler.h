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

/* platform independent networking... */
#include "pqi/pqinetwork.h"
#include "upnp/upnputil.h"

#include <QObject>
#include <QMutex>

class UPnPConfigData;
class UPnPAsynchronizer;

class upnpHandler {
public:

    upnpHandler();
    ~upnpHandler(){};

    /* These indicate the possible states of the UPNP configuration. */
    enum upnpStates {
        //Initial state before calling startup
        UPNP_STATE_UNINITIALISED,
        //There are no Internet Gateway Devices available on this LAN
        UPNP_STATE_UNAVAILABLE,
        //There is an Internet Gateway Device available, but no mapping set
        UPNP_STATE_READY,
        //We either failed in setting the mapping, or we set a mapping and it has been discovered not working
        UPNP_STATE_FAILED,
        //We have set a mapping, and it is working
        UPNP_STATE_ACTIVE
    };

    /* Asynchronous call to start UPNP. */
    void startup();

    /* Blocking call to disable UPNP and remove all forwardings. */
    void shutdown();

    /* Asynchronous call to disable UPNP, removing all forwardings, and restart it. */
    void restart();

    /* Returns the state of UPNP.
       If a mapping has already been set and confirmed working, calling this will double-check it still remains working. */
    upnpHandler::upnpStates getUpnpState();

    /* The address that the listening port is on, this port will be used for both internal and external address. */
    void setTargetPort(unsigned short newPort);

private:

    /* Searches the network for an Internet Gateway Device and if found, initializes the upnpConfig and returns true.
       Otherwise clears out the upnpConfig info and returns false. */
    bool initUPnPState();

    /* Prints the current state of UPNP setup. */
    void printUPnPState();

    /* Starts UPnP. Must be called after initUPnPState. */
    bool start_upnp();

    /* Removes any UPnP forwardings. */
    bool shutdown_upnp();

    /* Mutex for data below */
    mutable QMutex upnpMtx;

    /* Possible values for state are defined by constants above. */
    upnpStates upnpState;

    /* UPnP configuration data created by initUPnPState. */
    UPnPConfigData *upnpConfig;

    /* Both the internal and external port number to be mapped. */
    unsigned short targetPort;

    struct sockaddr_in upnp_internalAddress;

    friend class UPnPAsynchronizer;
};

/* This class can be moved into a QThread, such that calls to its request functions will execute the tasks asynchronously.
   All of the underlying code remains in the upnpHandler, with calls made via UPnPAsynchronizer's friend status. */
class UPnPAsynchronizer: public QObject {
Q_OBJECT

public:
    UPnPAsynchronizer(upnpHandler* handler);

    /* Returns a QThread that contains a new UPnPAsynchronizer.
       There is no need to handle memory management of this QThread, it is all handled internally and will free itself upon completion of its task. */
    static UPnPAsynchronizer* createWorker(upnpHandler* handler);

    /* These functions are asynchronous.
       Upon completion they will emit the workCompleted signal and then schedule the UPnPAsynchronizer for deletion. */
    void requestStart();
    void requestRestart();

private slots:
    void startUPnP();
    void stopUPnP();

signals:
    void startRequested();
    void stopRequested();
    void workCompleted();

private:
    upnpHandler* handler;
};

class UPnPConfigData {
public:
    struct UPNPDev *devlist;
    struct UPNPUrls urls;
    struct IGDdatas data;
    char lanaddr[16]; /* Own ip address on the LAN. */
};

#endif /* UPNP_IFACE_H */
