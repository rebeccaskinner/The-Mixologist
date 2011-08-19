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

#include "pqi/pqi.h"
#include "pqi/pqiperson.h"
#include "pqi/pqipersongrp.h"

const int pqipersonzone = 82371;
#include "util/debug.h"
#include <sstream>

/****
 * #define PERSON_DEBUG
 ****/

pqiperson::pqiperson(std::string id, int librarymixer_id, pqipersongrp *pg)
    :PQInterface(id, librarymixer_id), active(false), activepqi(NULL),
     inConnectAttempt(false), waittimes(0),
     pqipg(pg) {

    /* must check id! */

    return;
}

pqiperson::~pqiperson() {
    // clean up the children.
    std::map<uint32_t, pqiconnect *>::iterator it;
    for (it = kids.begin(); it != kids.end(); it++) {
        pqiconnect *pc = (it->second);
        delete pc;
    }
    kids.clear();
}


// The PQInterface interface.
int     pqiperson::SendItem(NetItem *i) {
    std::ostringstream out;
    out << "pqiperson::SendItem()";
    if (active) {
        out << " Active: Sending On";
        return activepqi -> SendItem(i);
    } else {
        out << " Not Active: deleting";
        out << std::endl;
        delete i;
    }
    pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str().c_str());
    return 0; // queued.
}

NetItem *pqiperson::GetItem() {
    if (active)
        return activepqi -> GetItem();
    // else not possible.
    return NULL;
}

int     pqiperson::status() {
    if (active)
        return activepqi -> status();
    return -1;
}

// tick......
int pqiperson::tick() {
    int activeTick = 0;

    {
        std::ostringstream out;
        out << "pqiperson::tick() Id: " << PeerId() << " ";

        if (active) out << "***Active***";
        else out << ">>InActive<<";
        out << std::endl;

        out << "Activepqi: " << activepqi << " inConnectAttempt: ";

        if (inConnectAttempt) out << "In Connection Attempt";
        else out << "   Not Connecting    ";
        out << std::endl;


        // tick the children.
        std::map<uint32_t, pqiconnect *>::iterator it;
        for (it = kids.begin(); it != kids.end(); it++) {
            if (0 < (it->second) -> tick()) {
                activeTick = 1;
            }
            out << "\tTicking Child: " << (it->first) << std::endl;
        }

        pqioutput(PQL_DEBUG_ALL, pqipersonzone, out.str().c_str());
    } // end of pqioutput.

    return activeTick;
}

// callback function for the child - notify of a change.
// This is only used for out-of-band info....
// otherwise could get dangerous loops.
int     pqiperson::notifyEvent(NetInterface *ni, int newState) {
    {
        std::ostringstream out;
        out << "pqiperson::notifyEvent() Id: " << LibraryMixerId();
        out << std::endl;
        out << "Message: " << newState << " from: " << ni << std::endl;

        pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str().c_str());
    }

    /* find the pqi, */
    pqiconnect *pqi = NULL;
    uint32_t    type = 0;
    std::map<uint32_t, pqiconnect *>::iterator it;

    /* start again */
    int i = 0;
    for (it = kids.begin(); it != kids.end(); it++) {
        {
            std::ostringstream out;
            out << "pqiperson::notifyEvent() Kid# ";
            out << (i + 1) << " of " << kids.size();
            out << std::endl;
            out << " type: " << (it->first);
            out << " ni: " << (it->second)->ni;
            out << " input ni: " << ni;
            pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str().c_str());
        }
        i++;

        if ((it->second)->thisNetInterface(ni)) {
            pqi = (it->second);
            type = (it->first);
        }
    }

    if (!pqi) {
        pqioutput(LOG_DEBUG_ALERT, pqipersonzone, "pqiperson::notifyEvent() Unknown notifyEvent Source!");
        return -1;
    }


    switch (newState) {
        case CONNECT_RECEIVED:
        case CONNECT_SUCCESS:

            /* notify */
            if (pqipg)
                pqipg->notifyConnect(PeerId(), type, true);

            if ((active) && (activepqi != pqi)) {
                pqioutput(PQL_DEBUG_ALERT, pqipersonzone,
                          "pqiperson::notifyEvent() Connected to friend, but there was an existing connection, resetting");
                activepqi -> reset();
            }

            /* now install a new one. */
            {

                pqioutput(LOG_DEBUG_ALERT, pqipersonzone,
                          "pqiperson::notifyEvent() Successful connection");
                // mark as active.
                active = true;
                activepqi = pqi;
                inConnectAttempt = false;

                /* reset all other children? (clear up long UDP attempt) */

                for (it = kids.begin(); it != kids.end(); it++) {
                    if (it->second != activepqi) {
                        {
                            std::ostringstream out;
                            out << "pqiperson::notifyEvent() about to call reset on: ";
                            out << " type: " << (it->first);
                            out << " ni: " << (it->second)->ni;
                            out << " input ni: " << ni;
                            pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str().c_str());
                        }
                        it->second->reset();
                    }
                }
                return 1;
            }
            break;
        case CONNECT_UNREACHABLE:
        case CONNECT_FIREWALLED:
        case CONNECT_FAILED:
            if (active) {
                if (activepqi == pqi) {
                    pqioutput(PQL_DEBUG_ALERT, pqipersonzone,
                              "pqiperson::notifyEvent() Connection failed");
                    active = false;
                    activepqi = NULL;
                } else {
                    pqioutput(PQL_DEBUG_ALERT, pqipersonzone,
                              "pqiperson::notifyEvent() Connection failed (not activepqi->strange)");
                    // probably UDP connect has failed,
                    // TCP connection has been made since attempt started.
                    return -1;
                }
            } else {
                pqioutput(PQL_DEBUG_ALERT, pqipersonzone,
                          "pqiperson::notifyEvent() Connection failed while not active");
            }

            /* notify up (But not if we are actually active: rtn -1 case above) */
            if (pqipg)
                pqipg->notifyConnect(PeerId(), type, false);

            return 1;

            break;
        default:
            break;
    }
    return -1;
}

/***************** Not PQInterface Fns ***********************/

int     pqiperson::reset() {
    {
        std::ostringstream out;
        out << "pqiperson::reset() Id: " << PeerId();
        out << std::endl;
        pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str().c_str());
    }

    std::map<uint32_t, pqiconnect *>::iterator it;
    for (it = kids.begin(); it != kids.end(); it++) {
        (it->second) -> reset();
    }

    activepqi = NULL;
    active = false;

    return 1;
}

int pqiperson::addChildInterface(uint32_t type, pqiconnect *pqi) {
    {
        std::ostringstream out;
        out << "pqiperson::addChildInterface() : Id " << PeerId() << " " << type;
        out << std::endl;
        pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str().c_str());
    }

    kids[type] = pqi;
    return 1;
}

/***************** PRIVATE FUNCTIONS ***********************/
// functions to iterate over the connects and change state.


int     pqiperson::listen() {
    {
        std::ostringstream out;
        out << "pqiperson::listen() Id: " << PeerId();
        out << std::endl;
        pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str().c_str());
    }

    if (!active) {
        std::map<uint32_t, pqiconnect *>::iterator it;
        for (it = kids.begin(); it != kids.end(); it++) {
            // set them all listening.
            (it->second) -> listen();
        }
    }
    return 1;
}


int     pqiperson::stoplistening() {
    {
        std::ostringstream out;
        out << "pqiperson::stoplistening() Id: " << PeerId();
        out << std::endl;
        pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str().c_str());
    }

    std::map<uint32_t, pqiconnect *>::iterator it;
    for (it = kids.begin(); it != kids.end(); it++) {
        // set them all listening.
        (it->second) -> stoplistening();
    }
    return 1;
}

int pqiperson::connect(uint32_t type, struct sockaddr_in raddr, uint32_t delay, uint32_t period, uint32_t timeout) {
#ifdef PERSON_DEBUG
    {
        std::ostringstream out;
        out << "pqiperson::connect() Id: " << PeerId();
        out << " type: " << type;
        out << " addr: " << inet_ntoa(raddr.sin_addr);
        out << ":" << ntohs(raddr.sin_port);
        out << " delay: " << delay;
        out << " period: " << period;
        out << " timeout: " << timeout;
        out << std::endl;
        std::cerr << out.str();
        //pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str());
    }
#endif

    std::map<uint32_t, pqiconnect *>::iterator it;

    it = kids.find(type);
    if (it == kids.end()) {
#ifdef PERSON_DEBUG
        std::ostringstream out;
        out << "pqiperson::connect()";
        out << " missing pqiconnect";
        out << std::endl;
        std::cerr << out.str();
        //pqioutput(PQL_DEBUG_BASIC, pqipersonzone, out.str());
#endif
        return 0;
    }

    /* set the parameters */
    (it->second)->reset();

#ifdef PERSON_DEBUG
    std::cerr << "pqiperson::connect() setting connect_parameters" << std::endl;
#endif
    (it->second)->connect_parameter(NET_PARAM_CONNECT_DELAY, delay);
    (it->second)->connect_parameter(NET_PARAM_CONNECT_PERIOD, period);
    (it->second)->connect_parameter(NET_PARAM_CONNECT_TIMEOUT, timeout);

    (it->second)->connect(raddr);

    // flag if we started a new connectionAttempt.
    inConnectAttempt = true;

    return 1;
}


float   pqiperson::getRate(bool in) {
    // get the rate from the active one.
    if ((!active) || (activepqi == NULL))
        return 0;
    return activepqi -> getRate(in);
}

void    pqiperson::setMaxRate(bool in, float val) {
    // set to all of them. (and us)
    PQInterface::setMaxRate(in, val);
    // clean up the children.
    std::map<uint32_t, pqiconnect *>::iterator it;
    for (it = kids.begin(); it != kids.end(); it++) {
        (it->second) -> setMaxRate(in, val);
    }
}

