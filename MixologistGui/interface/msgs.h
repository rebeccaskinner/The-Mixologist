/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-8, Robert Fernie.
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

#ifndef MSG_GUI_INTERFACE_H
#define MSG_GUI_INTERFACE_H

#include <list>
#include <iostream>
#include <string>

#include "interface/types.h"

#include <QString>

//#define CHAT_PUBLIC        0x0001
#define CHAT_PRIVATE     0x0002
//#define CHAT_AVATAR_AVAILABLE 0x0004

class ChatInfo {
public:
    unsigned int librarymixer_id;
    unsigned int chatflags;
    QString name;
    QString msg;
};

class Msgs;
extern Msgs *msgs;

class Msgs {
public:

    Msgs() {}
    virtual ~Msgs() {}

    /*Returns true if there is a new chat service item available.*/
    virtual bool chatAvailable() = 0;

    /*Sends a message to friend with peer_id.*/
    virtual void ChatSend(unsigned int librarymixer_id, const QString &message) = 0;

    /*Processes all items on the incoming chat queue, including avatars and status.
      For each ChatMsgItem in the queue, creates a ChatInfo and adds it to the list to return.*/
    virtual bool getNewChat(QList<ChatInfo> &chats) = 0;

    /*Sends the specified status string status_string to friend with id peer_id.*/
    virtual void sendStatusString(unsigned int librarymixer_id, const QString &status_string) = 0 ;

#ifdef false
    virtual void getAvatarData(std::string pid, unsigned char *& data, int &size) = 0 ;
    virtual void setOwnAvatarData(const unsigned char *data, int size) = 0 ;
    virtual void getOwnAvatarData(unsigned char *& data, int &size) = 0 ;
#endif
    /****************************************/

};


#endif
