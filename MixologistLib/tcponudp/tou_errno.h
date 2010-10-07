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

#ifndef TOU_ERRNO_HEADER
#define TOU_ERRNO_HEADER

/* C Interface */
#ifdef  __cplusplus
extern "C" {
#endif

    /*******
     * This defines the unix errno's for windows, these are
     * needed to determine error types, these are defined
     * to be the same as the unix ones.
     */

    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/
#ifdef WINDOWS_SYS

#define EAGAIN      11
#define EWOULDBLOCK     EAGAIN

#define EUSERS      87
#define ENOTSOCK    88

#define EOPNOTSUPP  95

#define EADDRINUSE  98
#define EADDRNOTAVAIL   99
#define ENETDOWN    100
#define ENETUNREACH     101

#define ECONNRESET  104

#define ETIMEDOUT   110
#define ECONNREFUSED    111
#define EHOSTDOWN   112
#define EHOSTUNREACH    113
#define EALREADY    114
#define EINPROGRESS     115


#endif
    /********************************** WINDOWS/UNIX SPECIFIC PART ******************/

#ifdef  __cplusplus
} /* C Interface */
#endif

#endif /* TOU_ERRNO_HEADER */
