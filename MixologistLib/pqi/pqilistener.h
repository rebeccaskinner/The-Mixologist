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

#ifndef MRK_PQI_GENERIC_LISTEN_HEADER
#define MRK_PQI_GENERIC_LISTEN_HEADER

// operating system specific network header.
#include "pqi/pqinetwork.h"

/*
Interface to be implemented by the master listening socket for
the pqipersongrp.

Implementations include pqissllistener.
*/

class pqilistener {
public:

    pqilistener()       {
        return;
    }
    virtual ~pqilistener()      {
        return;
    }

    virtual int     tick()                  {
        return 1;
    }
    virtual int     status()                {
        return 1;
    }
    virtual int     setListenAddr(struct sockaddr_in addr)  {
        (void) addr;
        return 1;
    }
    virtual int setuplisten()               {
        return 1;
    }
    virtual int     resetlisten()               {
        return 1;
    }

};


#endif // MRK_PQI_GENERIC_LISTEN_HEADER
