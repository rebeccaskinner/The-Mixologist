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

#include "pqi/p3notify.h"
#include <stdint.h>

/* external reference point */
Notify *notify = NULL;

pqiNotify *getPqiNotify() {
    return ((p3Notify *) notify);
}

bool p3Notify::NotifySysMessage(uint32_t &sysid, uint32_t &type,
                                QString &title, QString &msg) {
    MixStackMutex stack(noteMtx); /************* LOCK MUTEX ************/
    if (pendingSysMsgs.size() > 0) {
        p3NotifySysMsg smsg = pendingSysMsgs.front();
        pendingSysMsgs.pop_front();

        sysid = smsg.sysid;
        type = smsg.type;
        title = smsg.title;
        msg = smsg.msg;

        return true;
    }

    return false;
}

bool p3Notify::NotifyPopupMessage(uint32_t &ptype, QString &name, QString &msg) {
    MixStackMutex stack(noteMtx); /************* LOCK MUTEX ************/
    if (pendingPopupMsgs.size() > 0) {
        p3NotifyPopupMsg pmsg = pendingPopupMsgs.front();
        pendingPopupMsgs.pop_front();

        ptype = pmsg.type;
        name = pmsg.name;
        msg = pmsg.msg;

        return true;
    }

    return false;
}

/* Input from MixologistLib */
bool p3Notify::AddPopupMessage(uint32_t ptype, QString name, QString msg) {
    MixStackMutex stack(noteMtx); /************* LOCK MUTEX ************/

    p3NotifyPopupMsg pmsg;

    pmsg.type = ptype;
    pmsg.name = name;
    pmsg.msg = msg;

    pendingPopupMsgs.push_back(pmsg);

    return true;
}


bool p3Notify::AddSysMessage(uint32_t sysid, uint32_t type,
                             QString title, QString msg) {
    MixStackMutex stack(noteMtx); /************* LOCK MUTEX ************/

    p3NotifySysMsg smsg;

    smsg.sysid = sysid;
    smsg.type = type;
    smsg.title = title;
    smsg.msg = msg;

    pendingSysMsgs.push_back(smsg);

    return true;
}
