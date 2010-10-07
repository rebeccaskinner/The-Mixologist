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

#ifndef SERVICE_IDS_H
#define SERVICE_IDS_H

#include <inttypes.h>

/* Single place for Cache/Service Ids (uint16_t)
 * to 64K of them....
 */

const uint16_t SERVICE_TYPE_STATUS      = 0x0001;
const uint16_t SERVICE_TYPE_CHAT        = 0x0012;
const uint16_t SERVICE_TYPE_MIX         = 0x0100;
const uint16_t SERVICE_TYPE_FILE        = 0x0101;

#endif /* SERVICE_IDS_H */


