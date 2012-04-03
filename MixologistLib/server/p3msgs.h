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

#ifndef P3MSG_INTERFACE_H
#define P3MSG_INTERFACE_H

#include "interface/msgs.h"

class p3ChatService;

class ChatMsgItem;

class p3Msgs: public Msgs {
public:

    p3Msgs(p3ChatService *p3c)
        :mChatSrv(p3c) {
        return;
    }
    virtual ~p3Msgs() {
        return;
    }

    /*Returns true if there is a new chat service item available.*/
    virtual bool chatAvailable();

    /*Sends a message to friend with peer_id.*/
    virtual void ChatSend(unsigned int librarymixer_id, const QString &message);

    /*Processes all items on the incoming chat queue, including avatars and status.
      For each ChatMsgItem in the queue, creates a ChatInfo and adds it to the list to return.*/
    virtual bool getNewChat(QList<ChatInfo> &chats);

    /*Sends the specified status string status_string to friend with id peer_id.*/
    virtual void sendStatusString(unsigned int librarymixer_id, const QString &status_string);

#ifdef false
    virtual void getAvatarData(std::string pid, unsigned char *& data, int &size);
    virtual void setOwnAvatarData(const unsigned char *data, int size);
    virtual void getOwnAvatarData(unsigned char *& data, int &size);
#endif

private:
    /*Takes an ChatMsgItem c and uses its information to populate a ChatInfo i.*/
    void initChatInfo(ChatMsgItem *c, ChatInfo &i);

    p3ChatService *mChatSrv;
};


#endif
