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
#include "pqi/p3connmgr.h"
#include "pqi/authmgr.h"
#include <QString>

class p3Peers: public Peers {
public:

    p3Peers() {return;}
    virtual ~p3Peers() {return;}

    /* Peer Details (Net & Auth) */
    virtual std::string getOwnCertId();
    virtual unsigned int getOwnLibraryMixerId();
    virtual QString getOwnName();

    //List of LibraryMixer ids for all online friends
    virtual bool getOnlineList(std::list<int> &ids);
    //List of LibraryMixer ids for all friends with encryption keys
    virtual bool getSignedUpList(std::list<int> &ids);
    //List of LibraryMixer ids for all friends
    virtual bool getFriendList(std::list<int> &ids);

    virtual bool isOnline(unsigned int librarymixer_id);
    virtual bool isFriend(unsigned int librarymixer_id);
    virtual QString getPeerName(unsigned int librarymixer_id);
    /*Fills in PeerDetails for user with librarymixer_id
      Can be used for friends or self.
      Returns true, or false if unable to find user with librarymixer_id*/
    virtual bool getPeerDetails(unsigned int librarymixer_id, PeerDetails &d);

    //Returns the associated cert_id or "" if unable to find.
    virtual std::string findCertByLibraryMixerId(unsigned int librarymixer_id);
    //Returns the associated librarymixer_id or -1 if unable to find.
    virtual unsigned int findLibraryMixerByCertId(std::string cert_id);

    /* Add/Remove Friends */
    //Either adds a new friend, or updates the existing friend
    virtual bool addUpdateFriend(unsigned int librarymixer_id, QString cert, QString name);

    /* Network Stuff */
    virtual void connectAttempt(unsigned int librarymixer_id);
    virtual void connectAll();
    virtual bool setLocalAddress(unsigned int librarymixer_id, std::string addr, uint16_t port);
    virtual bool setExtAddress(unsigned int librarymixer_id, std::string addr, uint16_t port);
    virtual bool setNetworkMode(unsigned int librarymixer_id, uint32_t netMode);
    virtual bool setVisState(unsigned int librarymixer_id, uint32_t mode);
};

#endif
