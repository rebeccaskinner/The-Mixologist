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

#ifndef SERVICE_STATUS_HEADER
#define SERVICE_STATUS_HEADER

/*
 * Right now it just sends occasional keep alive messages, but these
 * should be expanded in the future to become full on status messages.
 */

#include <services/p3service.h>
#include <pqi/pqimonitor.h>

class StatusService;

/* This extern is to provide access to sendStatusUpdateToAll. */
extern StatusService *statusService;

class StatusService: public p3Service, public pqiMonitor {

public:
    StatusService();
    virtual int tick();

    /* Requests that a status update be sent out to all friends on the next tick. */
    void sendStatusUpdateToAll();

    /* Sends an OnConnectStatusItem to friend. */
    void sendOnConnect(unsigned int friend_id);

    /**********************************************************************************
     * Implementations for pqiMonitor
     **********************************************************************************/
    /* Called by the ConnectivityManager's tick function with a list of pqipeers whose statuses have changed.
       If action is PEER_CONNECTED, sends an OnConnectStatusItem. */
    virtual void statusChange(const std::list<pqipeer> &plist);

private:
    time_t timeOfLastTry; //The time of the last status update sent out

    mutable QMutex statusMutex;
};

#endif

