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

#include <stdlib.h> /* Included because GCC4.4 wants it */
#include <string.h>     /* Included because GCC4.4 wants it */

#include "serialiser/baseserial.h"
#include "util/net.h"

/* UInt16 get/set */

bool getRawUInt16(void *data, uint32_t size, uint32_t *offset, uint16_t *out) {
    /* first check there is space */
    if (size < *offset + 2) {
        return false;
    }
    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* extract the data */
    uint16_t netorder_num;
    memcpy(&netorder_num, buf, sizeof(uint16_t));

    (*out) = ntohs(netorder_num);
    (*offset) += 2;
    return true;
}

bool setRawUInt16(void *data, uint32_t size, uint32_t *offset, uint16_t in) {
    /* first check there is space */
    if (size < *offset + 2) {
        return false;
    }

    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* convert the data to the right format */
    uint16_t netorder_num = htons(in);

    /* pack it in */
    memcpy(buf, &netorder_num, sizeof(uint16_t));

    (*offset) += 2;
    return true;
}

/* UInt32 get/set */

bool getRawUInt32(void *data, uint32_t size, uint32_t *offset, uint32_t *out) {
    /* first check there is space */
    if (size < *offset + 4) {
        return false;
    }
    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* extract the data */
    uint32_t netorder_num;
    memcpy(&netorder_num, buf, sizeof(uint32_t));

    (*out) = ntohl(netorder_num);
    (*offset) += 4;
    return true;
}

bool setRawUInt32(void *data, uint32_t size, uint32_t *offset, uint32_t in) {
    /* first check there is space */
    if (size < *offset + 4) {
        return false;
    }

    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* convert the data to the right format */
    uint32_t netorder_num = htonl(in);

    /* pack it in */
    memcpy(buf, &netorder_num, sizeof(uint32_t));

    (*offset) += 4;
    return true;
}

/* Int32 get/set */

bool getRawInt32(void *data, uint32_t size, uint32_t *offset, int32_t *out) {
    /* first check there is space */
    if (size < *offset + 4) {
        return false;
    }
    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* extract the data */
    uint32_t netorder_num;
    memcpy(&netorder_num, buf, sizeof(int32_t));

    (*out) = ntohl(netorder_num);
    (*offset) += 4;
    return true;
}

bool setRawInt32(void *data, uint32_t size, uint32_t *offset, int32_t in) {
    /* first check there is space */
    if (size < *offset + 4) {
        return false;
    }

    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* convert the data to the right format */
    uint32_t netorder_num = htonl(in);

    /* pack it in */
    memcpy(buf, &netorder_num, sizeof(int32_t));

    (*offset) += 4;
    return true;
}

/* UInt64 get/set */

bool getRawUInt64(void *data, uint32_t size, uint32_t *offset, uint64_t *out) {
    /* first check there is space */
    if (size < *offset + 8) {
        return false;
    }
    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* extract the data */
    uint64_t netorder_num;
    memcpy(&netorder_num, buf, sizeof(uint64_t));

    (*out) = ntohll(netorder_num);
    (*offset) += 8;
    return true;
}

bool setRawUInt64(void *data, uint32_t size, uint32_t *offset, uint64_t in) {
    /* first check there is space */
    if (size < *offset + 8) {
        return false;
    }

    void *buf = (void *) &(((uint8_t *) data)[*offset]);

    /* convert the data to the right format */
    uint64_t netorder_num = htonll(in);

    /* pack it in */
    memcpy(buf, &netorder_num, sizeof(uint64_t));

    (*offset) += 8;
    return true;
}



