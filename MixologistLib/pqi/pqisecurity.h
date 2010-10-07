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


#ifndef MRK_PQI_SECURITY_HEADER
#define MRK_PQI_SECURITY_HEADER

#define PQI_INCOMING 2
#define PQI_OUTGOING 5

#include <string>

//structure.
typedef struct sec_policy {
    int searchable; // flags indicate how searchable we are..
} SecurityPolicy;

// functions for checking what is allowed...
//

std::string         secpolicy_print(SecurityPolicy *);
SecurityPolicy     *secpolicy_create();
int         secpolicy_delete(SecurityPolicy *);
int             secpolicy_limit(SecurityPolicy *limiter,
                                SecurityPolicy *alter);
int             secpolicy_check(SecurityPolicy *, int type_transaction,
                                int direction);


#endif

