/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-8, Robert Fernie
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

#include "pqi/pqinotify.h"
#include <stdint.h>
#include <interface/iface.h>

pqiNotify *getPqiNotify() {
    return (pqiNotify*)notify;
}

pqiNotify::pqiNotify() {
    connect(this, SIGNAL(notifyPopupMessage(int,QString,QString)), notifyBase, SLOT(displayPopupMessage(int,QString,QString)));
    connect(this, SIGNAL(notifySysMessage(int,QString,QString)), notifyBase, SLOT(displaySysMessage(int,QString,QString)));
}

/* Input from MixologistLib */
bool pqiNotify::AddPopupMessage(int type, QString name, QString msg) {
    emit notifyPopupMessage(type, name, msg);
    return true;
}

bool pqiNotify::AddSysMessage(int type, QString title, QString msg) {
    emit notifySysMessage(type, title, msg);
    return true;
}
