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

#ifndef BIO_TCPONUDP_H
#define BIO_TCPONUDP_H

#include <openssl/bio.h>

#ifdef  __cplusplus
extern "C" {
#endif


    int BIO_tou_socket_should_retry(int s, int e);
    int BIO_tou_socket_non_fatal_error(int error);

#define BIO_TYPE_TOU_SOCKET     (30|0x0400|0x0100)      /* NEW rmfern type */

    BIO_METHOD *BIO_s_tou_socket(void);

#ifdef  __cplusplus
}
#endif
#endif
