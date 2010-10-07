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

    p3Peers(p3ConnectMgr *cm, AuthMgr *am);
    virtual ~p3Peers() {
        return;
    }

    /* Peer Details (Net & Auth) */
    virtual std::string getOwnCertId();
    virtual int getOwnLibraryMixerId();
    virtual QString getOwnName();

    //List of LibraryMixer ids for all online friends
    virtual bool    getOnlineList(std::list<int> &ids);
    //List of LibraryMixer ids for all friends with encryption keys
    virtual bool    getSignedUpList(std::list<int> &ids);
    //List of LibraryMixer ids for all friends
    virtual bool    getFriendList(std::list<int> &ids);

    virtual bool    isOnline(int librarymixer_id);
    virtual bool    isFriend(int librarymixer_id);
    virtual QString getPeerName(int librarymixer_id);
    virtual bool    getPeerDetails(int librarymixer_id, PeerDetails &d);

    //Returns the associated cert_id or "" if unable to find.
    virtual std::string     findCertByLibraryMixerId(int librarymixer_id);
    //Returns the associated librarymixer_id or -1 if unable to find.
    virtual int     findLibraryMixerByCertId(std::string cert_id);

    /* Add/Remove Friends */
    //Either adds a new friend, or updates the existing friend
    virtual bool addUpdateFriend(int librarymixer_id, QString cert, QString name);

    /* Network Stuff */
    virtual void connectAttempt(int librarymixer_id);
    virtual void connectAll();
    virtual bool setLocalAddress(int librarymixer_id, std::string addr, uint16_t port);
    virtual bool setExtAddress(int librarymixer_id, std::string addr, uint16_t port);
    virtual bool setNetworkMode(int librarymixer_id, uint32_t netMode);
    virtual bool setVisState(int librarymixer_id, uint32_t mode);

private:

    p3ConnectMgr *mConnMgr;
    AuthMgr    *mAuthMgr;
};

#endif
