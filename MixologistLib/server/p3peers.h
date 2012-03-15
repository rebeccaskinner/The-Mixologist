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

#ifndef P3_PEER_INTERFACE_H
#define P3_PEER_INTERFACE_H

#include "interface/peers.h"
#include "pqi/connectivitymanager.h"
#include "pqi/authmgr.h"
#include <QString>

/*
 * p3Peers.
 *
 * Top level friend interface.
 *
 * Implements Peers interface for control from the GUI.
 *
 */

class p3Peers: public Peers {
public:

    p3Peers(QString &ownName);
    virtual ~p3Peers() {}

    /* Returns the logged in Mixologist user's own encryption certificate id. */
    virtual std::string getOwnCertId();

    /* Returns the logged in Mixologist user's own LibraryMixer id. */
    virtual unsigned int getOwnLibraryMixerId();

    /* Returns the logged in user's own name. */
    virtual QString getOwnName();

    /* List of LibraryMixer ids for all online friends. */
    virtual void getOnlineList(std::list<int> &ids);

    /* List of LibraryMixer ids for all friends with encryption keys. */
    virtual void getSignedUpList(std::list<int> &ids);

    /* List of LibraryMixer ids for all friends. */
    virtual void getFriendList(std::list<int> &ids);

    /* Returns true if that id belongs to a friend. */
    virtual bool isFriend(unsigned int librarymixer_id);

    /* Returns true if that id belongs to a friend, and that friend is online. */
    virtual bool isOnline(unsigned int librarymixer_id);

    /* Returns the name of the friend with that LibraryMixer id.
       Returns an empty string if no such friend. */
    virtual QString getPeerName(unsigned int librarymixer_id);

    /* Fills in PeerDetails for user with librarymixer_id
       Can be used for friends or self.
       Returns true, or false if unable to find user with librarymixer_id*/
    virtual bool getPeerDetails(unsigned int librarymixer_id, PeerDetails &d);

    /* Returns the user's encryption certificacte id or "" if unable to find. */
    virtual std::string findCertByLibraryMixerId(unsigned int librarymixer_id);

    /* Returns the user's LibraryMixer id or -1 if unable to find. */
    virtual unsigned int findLibraryMixerByCertId(std::string cert_id);

    /* Either adds a new friend, or updates the existing friend. */
    virtual bool addUpdateFriend(unsigned int librarymixer_id, QString cert, QString name);

    /* Immediate retry to connect to that friend. */
    virtual void connectAttempt(unsigned int librarymixer_id);

    /* Immediate retry to connect to all offline friends. */
    virtual void connectAll();

    virtual bool setLocalAddress(unsigned int librarymixer_id, std::string addr, uint16_t port);
    virtual bool setExtAddress(unsigned int librarymixer_id, std::string addr, uint16_t port);

private:
    /* Master storage in the Mixologist for a user's own name. */
    QString ownName;
};

#endif
