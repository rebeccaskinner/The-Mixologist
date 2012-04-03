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
#include "interface/peers.h"
#include "server/p3msgs.h"

//#include "services/p3msgservice.h"
#include "services/p3chatservice.h"

/****************************************/
/****************************************/
void p3Msgs::ChatSend(unsigned int librarymixer_id, const QString &message) {
    mChatSrv->sendPrivateChat(librarymixer_id, message);
}

void p3Msgs::sendStatusString(unsigned int librarymixer_id, const QString &status_string) {
    mChatSrv->sendStatusString(librarymixer_id, status_string);
}

bool p3Msgs::chatAvailable() {
    return mChatSrv->receivedItems();
}

bool p3Msgs::getNewChat(QList<ChatInfo> &chats) {
    /* get any messages and push them to iface */
    QList<ChatMsgItem *> chatList = mChatSrv->getChatQueue();
    if (chatList.size() < 1) return false;

    foreach (ChatMsgItem *currentItem, chatList) {
        ChatInfo info;
        initChatInfo(currentItem, info);
        chats.append(info);
        delete currentItem;
    }

    return true;
}

void p3Msgs::initChatInfo(ChatMsgItem *chatMsgItem, ChatInfo &chatInfo) {
    chatInfo.librarymixer_id = chatMsgItem->LibraryMixerId();
    chatInfo.name = peers->getPeerName(chatMsgItem->LibraryMixerId());
    chatInfo.chatflags = 0;
    chatInfo.msg = chatMsgItem->message;

    if (chatMsgItem->chatFlags & CHAT_FLAG_PRIVATE) {
        chatInfo.chatflags |= CHAT_PRIVATE;
    }
#ifdef false
    if (chatMsgItem->chatFlags & CHAT_FLAG_AVATAR_AVAILABLE) {
        chatInfo.chatflags |= CHAT_AVATAR_AVAILABLE;
    }
#endif
}

#ifdef false
void p3Msgs::getOwnAvatarData(unsigned char *& data,int &size) {
    mChatSrv->getOwnAvatarJpegData(data,size);
}

void p3Msgs::setOwnAvatarData(const unsigned char *data,int size) {
    mChatSrv->setOwnAvatarJpegData(data,size);
}

void p3Msgs::getAvatarData(std::string pid,unsigned char *& data,int &size) {
    mChatSrv->getAvatarJpegData(pid,data,size);
}
#endif
