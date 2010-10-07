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


#include "server/server.h"
#include "util/dir.h"
#include "services/p3chatservice.h"

#include <iostream>
#include <sstream>

#include "util/debug.h"
const int p3facemsgzone = 11453;

#include <sys/time.h>
#include <time.h>


/* Flagging Persons / Channels / Files in or out of a set (CheckLists) */
int     Server::ClearInChat() {
    lockCore(); /* LOCK */

    mInChatList.clear();

    unlockCore();   /* UNLOCK */

    return 1;
}


/* Flagging Persons / Channels / Files in or out of a set (CheckLists) */
int     Server::SetInChat(std::string id, bool in) {           /* friend : chat msgs */
    /* so we send this.... */
    lockCore();     /* LOCK */

    //std::cerr << "Set InChat(" << id << ") to " << (in ? "True" : "False") << std::endl;
    std::list<std::string>::iterator it;
    it = std::find(mInChatList.begin(), mInChatList.end(), id);
    if (it == mInChatList.end()) {
        if (in) {
            mInChatList.push_back(id);
        }
    } else {
        if (!in) {
            mInChatList.erase(it);
        }
    }

    unlockCore();   /* UNLOCK */

    return 1;
}


bool    Server::IsInChat(std::string id) { /* friend : chat msgs */
    /* so we send this.... */
    lockCore();     /* LOCK */

    std::list<std::string>::iterator it;
    it = std::find(mInChatList.begin(), mInChatList.end(), id);
    bool inChat = (it != mInChatList.end());

    unlockCore();   /* UNLOCK */

    return inChat;
}
