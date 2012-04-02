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
#ifndef MRK_PQI_PERSON_HEADER
#define MRK_PQI_PERSON_HEADER


#include "pqi/pqi.h"

#include <QMap>

class ConnectionToFriend;

#include "pqi/pqistreamer.h"

class AggregatedConnectionsToFriends;
class connectionMethod;

/*
 * ConnectionToFriend
 *
 * Each ConnectionToFriend belongs to a specific peer, or more precisely, to a specific certificate.
 * It represents the aggregated various methods of communicating with the peer represented by that certificate.
 *
 * Each method of communication is stored as a connectionMethod.
 * More than one connection method is supported, with one active at any given time.
 * Passes on calls to the PQInterface through to the appropriate actual implementation.
 *
 */

class ConnectionToFriend: public PQInterface {
public:
    ConnectionToFriend(std::string id, unsigned int librarymixer_id);
    virtual ~ConnectionToFriend();

    // control of the connection.
    int reset();
    int listen();
    int stoplistening();
    int connect(ConnectionType type, struct sockaddr_in raddr, uint32_t period, uint32_t timeout);

    //Add in connection method to connectionMethods.
    int addConnectionMethod(ConnectionType type, connectionMethod *pqi);

    // The PQInterface interface.
    virtual int SendItem(NetItem *);
    virtual NetItem *GetItem();

    virtual int tick();

    // Called by connectionMethods to notify the ConnectionToFriend of a connection event.
    int notifyEvent(NetInterface *notifyingInterface, NetNotificationEvent event);

    // PQInterface for rate control overloaded....
    virtual float getRate(bool in);
    virtual void setMaxRate(bool in, float val);

private:

    QMap<ConnectionType, connectionMethod *> connectionMethods;
    bool active;
    connectionMethod *activeConnectionMethod;
    bool inConnectAttempt;
    int waittimes;
};

/*
A method of communication to be used by the ConnectionToFriend.
A combination of a NetBinInterface with a pqistreamer.  Connection control
commands are passed directly to the NetInterface side of it, while PQInterface
events are handled by the pqistreamer half (which uses the BinInterface part
of the NetBinInterface).
*/
class connectionMethod: public pqistreamer, public NetInterface {
public:
    connectionMethod(Serialiser *rss, NetBinInterface *ni_in)
        :pqistreamer(rss, ni_in->PeerId(), ni_in->LibraryMixerId(), ni_in, 0),  // pqistreamer will cleanup NetInterface.
         NetInterface(NULL, ni_in->PeerId(), ni_in->LibraryMixerId()),     // No need for callback.
         ni(ni_in) {
        if (!ni_in) exit(1); //NetInterface cannot be NULL
    }

    virtual ~connectionMethod() {}

    // presents a virtual NetInterface -> passes to ni.
    virtual int connect(struct sockaddr_in raddr) {return ni->connect(raddr);}
    virtual int listen() {return ni -> listen();}
    virtual int stoplistening() {return ni -> stoplistening();}
    virtual void reset() {ni -> reset();}
    virtual bool setConnectionParameter(netParameters type, uint32_t value) {return ni -> setConnectionParameter(type, value);}

    // get the contact from the net side.
    virtual std::string PeerId() {
        if (ni) return ni->PeerId();
        else return PQInterface::PeerId();
    }

    // to check if this is our interface.
    virtual bool thisNetInterface(NetInterface *compare) {return (compare == ni);}

    NetBinInterface *ni;
};

#endif

