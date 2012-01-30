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

#ifndef MRK_PQI_SSL_PERSON_HANDLER_HEADER
#define MRK_PQI_SSL_PERSON_HANDLER_HEADER

#include "pqi/pqipersongrp.h"

/*
Specific implementation of a pqipersongrp to use SSL.
*/

class pqisslpersongrp: public pqipersongrp {
public:
    pqisslpersongrp(unsigned long flags)
        :pqipersongrp(flags) {
        return;
    }

protected:

    /********* FUNCTIONS to OVERLOAD for specialisation ********/
    virtual pqilistener *createListener(struct sockaddr_in laddr);
    //Creates a new pqiperson while calling its addChildInterface method
    //with both pqissl and pqissludp
    virtual pqiperson   *createPerson(std::string id, unsigned int librarymixer_id, pqilistener *listener);
};


#endif // MRK_PQI_SSL_PERSON_HANDLER_HEADER
