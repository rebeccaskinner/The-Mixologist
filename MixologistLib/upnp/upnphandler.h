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

/* These indicate the possible states of the UPNP.
   They are ordered in that unready states are <2 while final states are >2 */
#define UPNP_S_UNINITIALISED  0
#define UPNP_S_UNAVAILABLE    1
#define UPNP_S_READY          2
#define UPNP_S_TCP_FAILED     3
#define UPNP_S_UDP_FAILED     4
#define UPNP_S_ACTIVE         5

class UPnPConfigData;
class UPnPAsynchronizer;

class upnphandler {
public:

    upnphandler();
    ~upnphandler(){};

    /* Asynchronous call to enable or disable UPNP. */
    void enable(bool on);
    /* Blocking call to disable UPNP and remove all forwardings. */
    void shutdown();
    /* Asynchronous call to disable UPNP, removing all forwardings, and restart it. */
    void restart();

    /* True if UPNP has been set to enabled. */
    bool getEnabled();
    /* True if UPNP has successfully started and is working properly. */
    bool getActive();

    /* The address that the listening port is on. */
    void setInternalPort(unsigned short newPort);
    void setExternalPort(unsigned short newPort);

    /* Addresses as determined by uPnP. */
    bool getInternalAddress(struct sockaddr_in &addr);
    bool getExternalAddress(struct sockaddr_in &addr);

private:

    /* Searches the network for an Internet Gateway Device and if found, initializes the upnpConfig and returns true.
       Otherwise clears out the upnpConfig info and returns false. */
    bool initUPnPState();
    void printUPnPState();
    /* Starts UPnP. Must be called after initUPnPState. */
    bool start_upnp();
    /* Removes any UPnP forwardings. */
    bool shutdown_upnp();

    /* Mutex for data below */
    mutable QMutex upnpMtx;

    /* Whether we have been called to enable UPnP. */
    bool enabled;

    /* Possible values for state are defined by constants above. */
    unsigned int upnpState;
    /* UPnP configuration data created by initUPnPState. */
    UPnPConfigData *upnpConfig;

    unsigned short internalPort;
    unsigned short desiredExternalPort;
    unsigned short currentExternalPort;

    struct sockaddr_in upnp_internalAddress;
    struct sockaddr_in upnp_externalAddress;

    friend class UPnPAsynchronizer;
};

/* This class can be moved into a QThread, such that calls to its request functions will execute the tasks asynchronously.
   All of the underlying code remains in the upnphandler, with calls made via UPnPAsynchronizer's friend status. */
class UPnPAsynchronizer: public QObject {
Q_OBJECT

public:
    UPnPAsynchronizer(upnphandler* handler);

    /* Returns a QThread that contains a new UPnPAsynchronizer.
       There is no need to handle memory management of this QThread, it is all handled internally and will free itself upon completion of its task. */
    static UPnPAsynchronizer* createWorker(upnphandler* handler);

    /* These functions are asynchronous.
       Upon completion they will emit the workCompleted signal and then schedule the UPnPAsynchronizer for deletion. */
    void requestStart();
    void requestStop();
    void requestRestart();

private slots:
    void startUPnP();
    void stopUPnP(bool emitCompletion = true);

signals:
    void startRequested();
    void stopRequested(bool emitCompletion);
    void workCompleted();

private:
    upnphandler* handler;
};

class UPnPConfigData {
public:
    struct UPNPDev *devlist;
    struct UPNPUrls urls;
    struct IGDdatas data;
    char lanaddr[16]; /* Own ip address on the LAN. */
};

#endif /* UPNP_IFACE_H */
