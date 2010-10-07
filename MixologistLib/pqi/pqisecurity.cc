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

#include "pqi/pqisecurity.h"
#include <stdlib.h>  // malloc


// Can keep the structure hidden....
// but won't at the moment.

// functions for checking what is allowed...
// currently these are all dummies.


std::string         secpolicy_print(SecurityPolicy *) {
    return std::string("secpolicy_print() Implement Me Please!");
}

SecurityPolicy     *secpolicy_create() {
    return (SecurityPolicy *) malloc(sizeof(SecurityPolicy));
}

int secpolicy_delete(SecurityPolicy *p) {
    free(p);
    return 1;
}


int             secpolicy_limit(SecurityPolicy *,
                                SecurityPolicy *) {
    return 1;
}

int             secpolicy_check(SecurityPolicy *, int,
                                int) {
    return 1;
}



