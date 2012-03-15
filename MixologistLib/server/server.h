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

#ifndef MRK_P3_INTERFACE_H
#define MRK_P3_INTERFACE_H

#include "interface/iface.h"
#include <QThread>
#include <QMutex>

/*
The main thread that does most of the work, and implements the Control interface in iface.h that provides high-level control over MixologistLib.
*/

class pqipersongrp;

class Server: public QObject, public Control {
    Q_OBJECT

public:
    Server(){}
    virtual ~Server(){};

    /* Starts all of MixologistLib's threads. */
    virtual bool StartupMixologist();
    /* Shuts down connections. */
    virtual bool ShutdownMixologist();

    /* Reloads transfer limits set in settings.ini by the user. */
    virtual void ReloadTransferRates();

    /* Used so that the GUI can inform the library of what version the client is, which is shared with friends on connect.
       setVersion is not thread-safe but is only being called once on startup before any reading could happen, and then by the status service.
       If any other thread is going to access these variables, make sure to update and make this thread-safe.
       The clientName is the name of that client, and can be anything, but for the Mixologist is simply Mixologist.
       The clientVersion is the version number with that client.
       The latestKnownVersion indicates that the user has indicated he should not be informed about new versions equal to or lower than that version. */
    virtual void setVersion(const QString &clientName, qulonglong clientVersion, qulonglong latestKnownVersion);
    virtual QString clientName();
    virtual qulonglong clientVersion();
    virtual qulonglong latestKnownVersion();

signals:
    /* When signaled, asynchronously calls beginTimers to start the tick loop. */
    void timersStarting();

private slots:
    /* Starts the tick loop. */
    void beginTimers();

    /* Ticks once a second. */
    void oneSecondTick();

    /* A variable rate tick for incoming and outgoing data that sets its own rate. */
    void variableTick();

private:

    /* This is the main MixologistLib loop, handles the ticking */
    //virtual void run();

    mutable QMutex versionMutex;
    QString storedClientName;
    qulonglong storedClientVersion;
    qulonglong storedLatestKnownVersion;

public:

    //These important public variables hold pointers to the single instances of each of the below
    pqipersongrp *pqih;

    friend class Init;
};

#endif
