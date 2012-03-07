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
#include "interface/init.h"

// for blocking signals
#include <signal.h>
#include "util/debug.h"
#include "pqi/pqisslpersongrp.h"
#include "pqi/pqiloopback.h"
#include "ft/ftcontroller.h"
#include "pqi/connectivitymanager.h"
/* Implemented Rs Interfaces */
#include "server/p3peers.h"
#include "server/p3msgs.h"
#include "ft/ftserver.h"
#include "services/p3chatservice.h"

#include "tcponudp/tou.h"

#include <sys/time.h>

/****
#define DEBUG_TICK 1
****/

bool Server::StartupMixologist() {
    ftserver->StartupThreads();
    ftserver->ResumeTransfers();

    this->start();

    return true;
}

bool Server::ShutdownMixologist() {
    //Should also disconnect all peers here
    connMgr->shutdown();
    return true;
}

void Server::ReloadTransferRates() {
    pqih->load_transfer_rates();
}

void Server::setVersion(const QString &clientName, qulonglong clientVersion, qulonglong latestKnownVersion) {
    storedClientName = clientName;
    storedClientVersion = clientVersion;
    storedLatestKnownVersion = latestKnownVersion;
}

QString Server::clientName() {return storedClientName;}
qulonglong Server::clientVersion() {return storedClientVersion;}
qulonglong Server::latestKnownVersion() {return storedLatestKnownVersion;}

/* Thread Fn: Run the Core */
void Server::run() {
    double timeDelta = 0.25;
    double minTimeDelta = 0.1; // 25;
    double maxTimeDelta = 0.5;
    double kickLimit = 0.15;

    double avgTickRate = timeDelta;

    double lastts, ts;
    lastts = ts = getCurrentTS();

    long   lastSec = 0; /* for the slower ticked stuff */

    while (true) {
#ifndef WINDOWS_SYS
        usleep((int) (timeDelta * 1000000));
#else
        Sleep((int) (timeDelta * 1000));
#endif

        ts = getCurrentTS();
        double delta = ts - lastts;

        /* for the fast ticked stuff */
        if (delta > timeDelta) {
            lastts = ts;

            /******************************** RUN SERVER *****************/
            int moreToTick;

            {
                QMutexLocker stack(&coreMutex);
                moreToTick = ftserver -> tick();
            }

            /* tick the connection Manager */
            connMgr->tick();
            /******************************** RUN SERVER *****************/

            /* adjust tick rate depending on whether there is more.
             */

            avgTickRate = 0.2 * timeDelta + 0.8 * avgTickRate;

            if (1 == moreToTick) {
                timeDelta = 0.9 * avgTickRate;
                if (timeDelta > kickLimit) {
                    /* force next tick in one sec
                     * if we are reading data.
                     */
                    timeDelta = kickLimit;
                    avgTickRate = kickLimit;
                }
            } else {
                timeDelta = 1.1 * avgTickRate;
            }

            /* limiter */
            if (timeDelta < minTimeDelta) {
                timeDelta = minTimeDelta;
            } else if (timeDelta > maxTimeDelta) {
                timeDelta = maxTimeDelta;
            }

            /* Fast Updates */

            /* now we have the slow ticking stuff */
            /* stuff ticked once a second (but can be slowed down) */
            if ((int) ts > lastSec) {
                lastSec = (int) ts;

                // Every second! (UDP keepalive).
                TCP_over_UDP_tick_stunkeepalive();

            } // end of slow tick.

        } // end of only once a second.
    }
    return;
}


/* General Internal Helper Functions
  ----> MUST BE LOCKED!
 */

#ifdef WINDOWS_SYS
#include <time.h>
#include <sys/timeb.h>
#endif

double Server::getCurrentTS() {

#ifndef WINDOWS_SYS
    struct timeval cts_tmp;
    gettimeofday(&cts_tmp, NULL);
    double cts =  (cts_tmp.tv_sec) + ((double) cts_tmp.tv_usec) / 1000000.0;
#else
    struct _timeb timebuf;
    _ftime( &timebuf);
    double cts =  (timebuf.time) + ((double) timebuf.millitm) / 1000.0;
#endif
    return cts;
}


