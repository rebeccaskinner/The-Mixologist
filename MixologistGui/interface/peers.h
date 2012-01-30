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

/* Net Mode */
const uint32_t NETMODE_UDP = 0x0001;
const uint32_t NETMODE_UPNP = 0x0002;
const uint32_t NETMODE_EXT = 0x0003;
const uint32_t NETMODE_UNREACHABLE = 0x0004;

/* Visibility */
const uint32_t VS_DHT_ON = 0x0001;
//const uint32_t VS_DISC_ON = 0x0002;

/* State */
enum peerStates {
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

    /* Auth details */
    std::string id;
    unsigned int librarymixer_id;
    QString name;

    std::string fpr; /* fingerprint */

    /* Network details (only valid if friend) */
    peerStates state; //Current connection status

    std::string localAddr;
    uint16_t localPort;
    std::string extAddr;
    uint16_t extPort;

    uint32_t netMode;
    uint32_t tryNetMode; /* only for ownState */
    uint32_t visState;

    /* basic stats */
    uint32_t lastConnect; /* how long ago */
    uint32_t connectPeriod;
};

std::ostream &operator<<(std::ostream &out, const PeerDetails &detail);

class Peers {
public:

    /* Peer Details (Net & Auth) */
    virtual std::string getOwnCertId() = 0;
    virtual unsigned int getOwnLibraryMixerId() = 0;
    virtual QString getOwnName() = 0;

    //List of LibraryMixer ids for all online friends
    virtual bool getOnlineList(std::list<int> &ids) = 0;
    //List of LibraryMixer ids for all friends with encryption keys
    virtual bool getSignedUpList(std::list<int> &ids) = 0;
    //List of LibraryMixer ids for all friends
    virtual bool getFriendList(std::list<int> &ids) = 0;

    virtual bool isOnline(unsigned int librarymixer_id) = 0;
    virtual bool isFriend(unsigned int librarymixer_id) = 0;
    virtual QString getPeerName(unsigned int librarymixer_id) = 0;
    /*Fills in PeerDetails for user with librarymixer_id
      Can be used for friends or self.
      Returns true, or false if unable to find user with librarymixer_id*/
    virtual bool getPeerDetails(unsigned int librarymixer_id, PeerDetails &d) = 0;

    //Returns the associated cert_id or "" if unable to find.
    virtual std::string findCertByLibraryMixerId(unsigned int librarymixer_id) = 0;
    //Returns the associated librarymixer_id or -1 if unable to find.
    virtual unsigned int findLibraryMixerByCertId(std::string cert_id) = 0;

    /* Add/Remove Friends */
    //Either adds a new friend, or updates the existing friend
    virtual bool addUpdateFriend(unsigned int librarymixer_id, QString cert, QString name) = 0;

    /* Network Stuff */
    virtual void connectAttempt(unsigned int librarymixer_id) = 0;
    virtual void connectAll() = 0;
    virtual bool setLocalAddress(unsigned int librarymixer_id, std::string addr, uint16_t port) = 0;
    virtual bool setExtAddress(unsigned int librarymixer_id, std::string addr, uint16_t port) = 0;
    virtual bool setNetworkMode(unsigned int librarymixer_id, uint32_t netMode) = 0;
    virtual bool setVisState(unsigned int librarymixer_id, uint32_t vis) = 0;

};

#endif
