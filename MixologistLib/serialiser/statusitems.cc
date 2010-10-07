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

#include <serialiser/statusitems.h>
#include <iostream>
#include <serialiser/baseserial.h>


std::ostream &StatusItem::print(std::ostream &out, uint16_t indent) {
    printNetItemBase(out, "StatusItem", indent);
    printNetItemEnd(out, "StatusItem", indent);
    return out;
}

bool StatusItem::serialise(void *data, uint32_t &pktsize) {
    uint32_t tlvsize = serial_size() ;
    if (pktsize < tlvsize) return false; /* not enough space */
    pktsize = tlvsize;

    bool ok = true;

    ok &= setNetItemHeader(data, tlvsize, PacketId(), tlvsize);

    return ok;
}

uint32_t StatusItem::serial_size() {
    uint32_t size = 8; /* header */

    return size;
}

/****************************Serialiser*********************************/
NetItem *StatusSerialiser::deserialise(void *data, uint32_t *pktsize) {
    uint32_t rstype = getNetItemId(data);
    uint32_t rssize = getNetItemSize(data);

    // look what we have...
    if (*pktsize < rssize) {  /* check size */
        return NULL; /* not enough data */
    }

    /* set the packet length */
    *pktsize = rssize;

    /* ready to load */

    if ((PKT_VERSION_SERVICE != getNetItemVersion(rstype)) || (SERVICE_TYPE_STATUS != getNetItemService(rstype))) {
        return NULL; /* wrong type */
    }

    return new StatusItem(data,*pktsize) ;
}
