/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-8, Robert Fernie
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

#ifndef PQI_MONITOR_H
#define PQI_MONITOR_H

#include <inttypes.h>
#include <string>
#include <list>

#include <QString>

/************** Define Type/Mode/Source ***************/

/* ACTIONS */
//Signal to AggregatedConnectionsToFriends that a new peer has been added, so a new ConnectionToFriend must be created
const uint32_t PEER_NEW = 0x0001;
//Signal that a connection attempt succeeded
const uint32_t PEER_CONNECTED = 0x0002;
//Signal to AggregatedConnectionsToFriends that it should try the connection again
const uint32_t PEER_CONNECT_REQ = 0x0004;
//Signal to AggregatedConnectionsToFriends that an existing peer has been timed out, and their connection must be reset
const uint32_t PEER_TIMEOUT = 0x0008;
//Signal to AggregatedConnectionsToFriends that we have updated info from LibraryMixer on this friend
const uint32_t PEER_CERT_AND_ADDRESS_UPDATED = 0x0010;

/* Used in a list to describe changed friends for monitors. */
class pqipeer {
public:
    unsigned int librarymixer_id;
    std::string cert_id;
    QString name;
    uint32_t state;
    uint32_t actions;
};

/* pqiMonitors are registered with the FriendsConnectivityManager to receive updates on changes in friends' connectivity. */
class pqiMonitor {
public:
    pqiMonitor() {}
    virtual ~pqiMonitor() {}

    /* Called by the FriendsConnectivityManager's tick function with a list of pqipeers whose statuses have changed. */
    virtual void statusChange(const std::list<pqipeer> &changedFriends) = 0;
};

#endif
