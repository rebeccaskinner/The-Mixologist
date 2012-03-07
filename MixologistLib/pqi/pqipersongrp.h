/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-8, Robert Fernie
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

#ifndef MRK_PQI_PERSON_HANDLER_HEADER
#define MRK_PQI_PERSON_HANDLER_HEADER

#include "pqi/pqihandler.h"
#include "pqi/pqiperson.h"
#include "pqi/pqilistener.h"
#include "pqi/pqiservice.h"
#include "pqi/pqimonitor.h"

/*
This is the master authentication connection object interface, which must be
implemented by an authentication method (such as SSL).  A single instance
is held by the Server as an internal variable.

It is designed to have one listening socket - pqilistener (for
listening to incoming connections) and a series of outgoing sockets -
PQInterfaces (which are found in pqihandler, which this implements,
each of which holds a PQInterface which corresponds to a person,
friends/self as pqiperson/pqiloopback).

This is also a pqitunnelserver, to which services can be attached.

Types include pqisslpersongrp.
*/

const unsigned long PQIPERSON_NO_LISTENER = 0x0001;

class pqipersongrp: public pqihandler, public pqiMonitor, public p3ServiceServer {
public:
    pqipersongrp(unsigned long flags);

    /*************************** Setup *************************/
    /* pqilistener */
    int init_listener();
    int restart_listener();

    //Loads the transfer rates from the settings files and sets them
    void load_transfer_rates();

    /*************** pqiMonitor callback **********************
    Called by the ConnectivityManager's tick function with a list of pqipeers
    whose statuses have changed. For each friend in the list, if their
    action is PEER_NEW, calls addPeer on them, if their action is
    PEER_CONNECT_REQ, calls connectPeer on them.
    */
    virtual void statusChange(const std::list<pqipeer> &plist);

    /*** callback from children ****/
    bool notifyConnect(std::string id, uint32_t type, bool success);

    /* This tick is called from ftserver, which perhaps should be revisited. */
    virtual int tick();

protected:

    /********* FUNCTIONS to OVERLOAD for specialisation ********/
    virtual pqilistener *createListener(struct sockaddr_in laddr) = 0;
    virtual pqiperson *createPerson(std::string id, unsigned int librarymixer_id, pqilistener *listener) = 0;
    /********* FUNCTIONS to OVERLOAD for specialisation ********/

    /* Overloaded NetItem Check
     * checks item->cid vs Person
     */
    virtual int checkOutgoingNetItem(NetItem *item, int global) {
        (void) item;
        (void) global;
        return 1;
    }

private:
    /******************* Peer Control **************************/
    //These functions are called by statusChange
    //Sets up all the connection classes for a new peer
    int addPeer(std::string id, unsigned int librarymixer_id);
    /*Calls connMgr->connectAttempt to get address information and other information
      about how to connect, and then calls the appropriate pqiperson's connect method*/
    int connectPeer(std::string cert_id, unsigned int librarymixer_id);
    void timeoutPeer(std::string cert_id);
    //int removePeer(std::string id);


    // The tunnelserver operation.
    int tickServiceRecv();
    int tickServiceSend();

    pqilistener *pqil; //The incoming connection listening socket for all
    unsigned long initFlags;
};
#endif // MRK_PQI_PERSON_HANDLER_HEADER
