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

#ifndef BASE_PACKING_H
#define BASE_PACKING_H

#include <string>
#include <stdlib.h>
#include <stdint.h>

/*******************************************************************
 * This is at the lowlevel packing routines. They are usually
 * created in pairs - one to pack the data, the other to unpack.
 *
 * getRawXXX(void *data, uint32_t size, uint32_t *offset, XXX *out);
 * setRawXXX(void *data, uint32_t size, uint32_t *offset, XXX *in);
 *
 *
 * data - the base pointer to the serialised data.
 * size - size of the memory pointed to by data.
 * *offset - where we want to (un)pack the data.
 *      This is incremented by the datasize.
 *
 * *in / *out - the data to (un)pack.
 *
 ******************************************************************/


bool getRawUInt16(void *data, uint32_t size, uint32_t *offset, uint16_t *out);
bool setRawUInt16(void *data, uint32_t size, uint32_t *offset, uint16_t in);

bool getRawUInt32(void *data, uint32_t size, uint32_t *offset, uint32_t *out);
bool setRawUInt32(void *data, uint32_t size, uint32_t *offset, uint32_t in);

bool getRawInt32(void *data, uint32_t size, uint32_t *offset, int32_t *out);
bool setRawInt32(void *data, uint32_t size, uint32_t *offset, int32_t in);

bool getRawUInt64(void *data, uint32_t size, uint32_t *offset, uint64_t *out);
bool setRawUInt64(void *data, uint32_t size, uint32_t *offset, uint64_t in);

#endif

