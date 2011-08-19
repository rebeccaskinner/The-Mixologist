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


#include <iostream>
#include <sstream>

#include "util/dir.h"
#include "util/debug.h"
const int p3facemsgzone = 11453;

#include <sys/time.h>
#include <time.h>

#include "interface/types.h"
#include "server/p3msgs.h"

//#include "services/p3msgservice.h"
#include "services/p3chatservice.h"

/****************************************/
/****************************************/
void    p3Msgs::ChatSend(const std::string &peer_id,const QString &message) {
    mChatSrv->sendPrivateChat(peer_id, message);
}

void p3Msgs::sendStatusString(const std::string &peer_id,const QString &status_string) {
    mChatSrv->sendStatusString(peer_id,status_string);
}

bool    p3Msgs::chatAvailable() {
    return mChatSrv->receivedItems();
}

bool    p3Msgs::getNewChat(std::list<ChatInfo> &chats) {
    /* get any messages and push them to iface */

    // get the items from the list.
    std::list<ChatMsgItem *> clist = mChatSrv -> getChatQueue();
    if (clist.size() < 1) {
        return false;
    }

    std::list<ChatMsgItem *>::iterator it;
    for (it = clist.begin(); it != clist.end(); it++) {
        ChatInfo ci;
        initChatInfo((*it), ci);
        chats.push_back(ci);
        delete (*it);
    }
    return true;
}

/**** HELPER FNS For Chat/Msg/Channel Lists ************
 *
 * The iface->Mutex is required to be locked
 * for intAddChannel / intAddChannelMsg.
 */

void p3Msgs::initChatInfo(ChatMsgItem *c, ChatInfo &i) {
    i.rsid = c -> PeerId();
    i.name = connMgr->getFriendName(i.rsid);
    i.chatflags = 0 ;
    i.msg  = c -> message;

    if (c -> chatFlags & CHAT_FLAG_PRIVATE) {
        i.chatflags |= CHAT_PRIVATE;
    }
    if (c->chatFlags & CHAT_FLAG_AVATAR_AVAILABLE) {
        i.chatflags |= CHAT_AVATAR_AVAILABLE;
    }
}

void p3Msgs::getOwnAvatarData(unsigned char *& data,int &size) {
    mChatSrv->getOwnAvatarJpegData(data,size) ;
}

void p3Msgs::setOwnAvatarData(const unsigned char *data,int size) {
    mChatSrv->setOwnAvatarJpegData(data,size) ;
}

void p3Msgs::getAvatarData(std::string pid,unsigned char *& data,int &size) {
    mChatSrv->getAvatarJpegData(pid,data,size) ;
}


