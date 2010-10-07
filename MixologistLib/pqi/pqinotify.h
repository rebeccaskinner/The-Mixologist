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

#ifndef PQI_NOTIFY_INTERFACE_H
#define PQI_NOTIFY_INTERFACE_H

#include "interface/notify.h"  /* for ids */
#include <QString>

/* This class is the input interface for notification events. */

class pqiNotify {
public:

    pqiNotify() {
        return;
    }
    virtual ~pqiNotify() {
        return;
    }

    /* Input from MixologistLib */
    virtual bool AddPopupMessage(uint32_t ptype, QString name, QString msg) = 0;
    virtual bool AddSysMessage(uint32_t sysid, uint32_t type, QString title, QString msg) = 0;

};

/* Global Access -> so we don't need everyone to have a pointer to this! */
extern pqiNotify *getPqiNotify();

#endif
