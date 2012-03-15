/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-8, Robert Fernie.
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

#ifndef PEER_GUI_INTERFACE_H
#define PEER_GUI_INTERFACE_H

#include <inttypes.h>
#include <string>
#include <list>
#include <QString>

/* The Main Interface Class - for information about your Peers */
class Peers;
extern Peers *peers;

/* State */
enum PeerDetailsState {
    PEER_STATE_CONNECTED,
    PEER_STATE_TRYING,
    PEER_STATE_WAITING_FOR_RETRY,
    PEER_STATE_OFFLINE,
    PEER_STATE_NO_CERT
};

/* Details class */
class PeerDetails {
public:

    PeerDetails();

    /* Identifying details */
    std::string id;
    unsigned int librarymixer_id;
    QString name;

    /* Network details (only valid if friend) */
    PeerDetailsState state; //Current connection status

    std::string localAddr;
    uint16_t localPort;
    std::string extAddr;
    uint16_t extPort;

    /* basic stats */
    uint32_t lastConnect; /* how long ago */
};

class Peers {
public:
    /* Returns the logged in Mixologist user's own encryption certificate id. */
    virtual std::string getOwnCertId() = 0;

    /* Returns the logged in Mixologist user's own LibraryMixer id. */
    virtual unsigned int getOwnLibraryMixerId() = 0;

    /* Returns the logged in user's own name. */
    virtual QString getOwnName() = 0;

    /* List of LibraryMixer ids for all online friends. */
    virtual void getOnlineList(std::list<int> &ids) = 0;

    /* List of LibraryMixer ids for all friends with encryption keys. */
    virtual void getSignedUpList(std::list<int> &ids) = 0;

    /* List of LibraryMixer ids for all friends. */
    virtual void getFriendList(std::list<int> &ids) = 0;

    /* Returns true if that id belongs to a friend. */
    virtual bool isFriend(unsigned int librarymixer_id) = 0;

    /* Returns true if that id belongs to a friend, and that friend is online. */
    virtual bool isOnline(unsigned int librarymixer_id) = 0;

    /* Returns the name of the friend with that LibraryMixer id.
       Returns an empty string if no such friend. */
    virtual QString getPeerName(unsigned int librarymixer_id) = 0;

    /* Fills in PeerDetails for user with librarymixer_id
       Can be used for friends or self.
       Returns true, or false if unable to find user with librarymixer_id*/
    virtual bool getPeerDetails(unsigned int librarymixer_id, PeerDetails &d) = 0;

    /* Returns the user's encryption certificacte id or "" if unable to find. */
    virtual std::string findCertByLibraryMixerId(unsigned int librarymixer_id) = 0;

    /* Returns the user's LibraryMixer id or -1 if unable to find. */
    virtual unsigned int findLibraryMixerByCertId(std::string cert_id) = 0;

    /* Either adds a new friend, or updates the existing friend. */
    virtual bool addUpdateFriend(unsigned int librarymixer_id, QString cert, QString name) = 0;

    /* Immediate retry to connect to that friend. */
    virtual void connectAttempt(unsigned int librarymixer_id) = 0;

    /* Immediate retry to connect to all offline friends. */
    virtual void connectAll() = 0;

    /* The maximum and minimum values for the Mixologist's ports. */
    static const int MIN_PORT = 1024;
    static const int MAX_PORT = 50000;

    virtual bool setLocalAddress(unsigned int librarymixer_id, std::string addr, uint16_t port) = 0;
    virtual bool setExtAddress(unsigned int librarymixer_id, std::string addr, uint16_t port) = 0;
};

#endif
