/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
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


#ifndef STATUS_ITEMS_H
#define STATUS_ITEMS_H

#include "serialiser/serial.h"
#include "serialiser/serviceids.h"
#include "serialiser/tlvbase.h"

class StatusItem: public NetItem {
public:
    StatusItem()  : NetItem(PKT_VERSION_SERVICE,SERVICE_TYPE_STATUS,0) {}
    StatusItem(void *,uint32_t)  : NetItem(PKT_VERSION_SERVICE,SERVICE_TYPE_STATUS,0) {}
    virtual ~StatusItem() {}

    virtual void clear() {}
    virtual std::ostream &print(std::ostream &out, uint16_t indent = 0);
    virtual bool serialise(void *data,uint32_t &size);
    virtual uint32_t serial_size();
};


class StatusSerialiser: public SerialType {
public:
    StatusSerialiser() :SerialType(PKT_VERSION_SERVICE, SERVICE_TYPE_STATUS) {}

    virtual uint32_t    size (NetItem *item) {
        return static_cast<StatusItem *>(item)->serial_size() ;
    }
    virtual bool serialise(NetItem *item, void *data, uint32_t *size) {
        return static_cast<StatusItem *>(item)->serialise(data,*size) ;
    }
    virtual NetItem *deserialise (void *data, uint32_t *size) ;
};

/**************************************************************************/

#endif /* STATUS_ITEMS_H */
