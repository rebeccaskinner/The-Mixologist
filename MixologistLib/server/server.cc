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

#include "server/server.h"
#include "pqi/connectivitymanager.h"
#include "pqi/aggregatedConnections.h"
#include "ft/ftserver.h"
#include "tcponudp/tou.h"

#include <QTimer>

bool Server::StartupMixologist() {
    ftserver->StartupThreads();
    ftserver->ResumeTransfers();

    /* Rather than starting the timers ourselves (that would set the thread affinity to the main thread)
       we emit a queued signal to start the timers so they are created in this thread. */
    connect(this, SIGNAL(timersStarting()), this, SLOT(beginTimers()), Qt::QueuedConnection);
    emit timersStarting();

    return true;
}

void Server::beginTimers() {
    QTimer *oneSecondTimer = new QTimer(this);
    connect(oneSecondTimer, SIGNAL(timeout()), this, SLOT(oneSecondTick()));
    oneSecondTimer->start(1000);

    QTimer::singleShot(0, this, SLOT(variableTick()));
}

bool Server::ShutdownMixologist() {
    //Should also disconnect all peers here
    connMgr->shutdown();
    return true;
}

void Server::ReloadTransferRates() {
    aggregatedConnectionsToFriends->load_transfer_rates();
}

void Server::setVersion(const QString &clientName, qulonglong clientVersion, qulonglong latestKnownVersion) {
    QMutexLocker stack(&versionMutex);
    storedClientName = clientName;
    storedClientVersion = clientVersion;
    storedLatestKnownVersion = latestKnownVersion;
}

QString Server::clientName() {
    QMutexLocker stack(&versionMutex);
    return storedClientName;
}

qulonglong Server::clientVersion() {
    QMutexLocker stack(&versionMutex);
    return storedClientVersion;
}

qulonglong Server::latestKnownVersion() {
    QMutexLocker stack(&versionMutex);
    return storedLatestKnownVersion;
}

void Server::oneSecondTick() {
    connMgr->tick();
}

#define MAX_SECONDS_TO_SLEEP 1
#define MIN_SECONDS_TO_SLEEP_DURING_FILE_TRANSFER 0.15

void Server::variableTick() {
    static double secondsToSleep, averageSecondsToSleep = 0.25;
    averageSecondsToSleep = 0.2 * secondsToSleep + 0.8 * averageSecondsToSleep;

    /* The fact that this tick is in the ftserver is actually deceptive.
       The ftserver tick also ticks our main AggregatedConnectionsToFriends (this should probably be changed in the future into a more sensible hierarchy).
       This means that almost all of our inbound and outbound data are handled by this tick. */
    int moreDataExists = ftserver->tick();

    /* Adjust tick rate depending on whether there is more file transfer data backing up. */
    if (moreDataExists == 1) {
        secondsToSleep = 0.9 * averageSecondsToSleep;
        if (secondsToSleep > MIN_SECONDS_TO_SLEEP_DURING_FILE_TRANSFER) {
            secondsToSleep = MIN_SECONDS_TO_SLEEP_DURING_FILE_TRANSFER;
            averageSecondsToSleep = MIN_SECONDS_TO_SLEEP_DURING_FILE_TRANSFER;
        }
    } else {
        secondsToSleep = 1.1 * averageSecondsToSleep;
    }

    if (secondsToSleep > MAX_SECONDS_TO_SLEEP) {
        secondsToSleep = MAX_SECONDS_TO_SLEEP;
    }

    QTimer::singleShot(secondsToSleep * 1000, this, SLOT(variableTick()));
}
