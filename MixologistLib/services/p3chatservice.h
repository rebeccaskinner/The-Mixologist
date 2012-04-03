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

#ifndef SERVICE_CHAT_HEADER
#define SERVICE_CHAT_HEADER

/*
 * The basic Chat service.
 *
 */

#include <QMap>

#include "serialiser/msgitems.h"
#include "services/p3service.h"

class p3ChatService: public p3Service {
public:
    p3ChatService();

    /* overloaded */
    virtual int   tick();
    virtual int   status();

    /* Fills out all appropriate header information, and adds to the send queue a chat message with body msg to id. */
    int sendPrivateChat(unsigned int librarymixer_id, const QString &message);

    /* Adds to the send queue a status string update status_str */
    void sendStatusString(unsigned int librarymixer_id, const QString &status_str);

    /// gets the peer's avatar in jpeg format, if available. Null otherwise. Also asks the peer to send
    /// its avatar, if not already available. Creates a new unsigned char array. It's the caller's
    /// responsibility to delete this once used.
    ///
    void getAvatarJpegData(const std::string &peer_id,unsigned char *& data,int &size);

    /// Sets the avatar data and size. Data is copied, so should be destroyed by the caller.
    void setOwnAvatarJpegData(const unsigned char *data,int size);
    void getOwnAvatarJpegData(unsigned char *& data,int &size);

    /* Pulls all items off of the chat input queue.
       For each item, if the item is
       An ChatStatusItem, informs the Notify of the chat status.
       An ChatAvatarItem, calls receiveAvatarJpegData.
       An ChatMsgItem, does the following:
       1) If it is flagged CHAT_FLAG_CONTAINS_AVATAR, then just deletes it (??)
       2) If it is flagged CHAT_FLAG_REQUESTS_AVATAR, then calls sendAvatarJpegData.
       3) Otherwise, adds it to a list of ChatMsgItems to return.
          If it is flagged CHAT_FLAG_AVATAR_AVAILABLE, calls sendAvatarRequest, and clears the flag unless it is marked as a new avatar. (?)*/
    QList<ChatMsgItem *> getChatQueue();

private:
    mutable QMutex mChatMtx;

    class AvatarInfo;

    /// Send avatar info to peer in jpeg format.
    void sendAvatarJpegData(unsigned int librarymixer_id);

    void receiveAvatarJpegData(ChatAvatarItem *ci);    // new method

    /// Sends a request for an avatar to the peer of given id
    void sendAvatarRequest(unsigned int librarymixer_id);

    ChatAvatarItem *makeOwnAvatarItem();

    AvatarInfo *_own_avatar;
    QMap<unsigned int, AvatarInfo *> _avatars;

};

#endif // SERVICE_CHAT_HEADER

