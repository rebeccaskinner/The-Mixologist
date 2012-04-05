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


#include "server/p3peers.h"
#include "server/server.h"
#include "pqi/friendsConnectivityManager.h"
#include "pqi/ownConnectivityManager.h"
#include "pqi/authmgr.h"
#include <interface/init.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include "pqi/authmgr.h"

p3Peers::p3Peers(QString &ownName)
    :ownName(ownName) {
    connect(ownConnectivityManager, SIGNAL(connectionStateChanged(int, bool)), this, SIGNAL(connectionStateChanged(int, bool)));
    connect(ownConnectivityManager, SIGNAL(ownConnectionReadinessChanged(bool)), this, SIGNAL(ownConnectionReadinessChanged(bool)));
}

/* Peer Details (Net & Auth) */
std::string p3Peers::getOwnCertId() {
    return authMgr->OwnCertId();
}

unsigned int p3Peers::getOwnLibraryMixerId() {
    return authMgr->OwnLibraryMixerId();
}

QString p3Peers::getOwnName(){
    return ownName;
}

bool p3Peers::getConnectionReadiness() {
    return ownConnectivityManager->getConnectionReadiness();
}

bool p3Peers::getConnectionAutoConfigEnabled() {
    return ownConnectivityManager->getConnectionAutoConfigEnabled();
}

ConnectionStatus p3Peers::getConnectionStatus() {
    return ownConnectivityManager->getConnectionStatus();
}

void p3Peers::getOnlineList(QList<unsigned int> &friend_ids) {
    friendsConnectivityManager->getOnlineList(friend_ids);
}

void p3Peers::getSignedUpList(QList<unsigned int> &friend_ids) {
    friendsConnectivityManager->getSignedUpList(friend_ids);
}

void p3Peers::getFriendList(QList<unsigned int> &friend_ids) {
    friendsConnectivityManager->getFriendList(friend_ids);
}

bool p3Peers::isOnline(unsigned int librarymixer_id) {
    return friendsConnectivityManager->isOnline(librarymixer_id);
}

bool p3Peers::isFriend(unsigned int librarymixer_id) {
    return friendsConnectivityManager->isFriend(librarymixer_id);
}

bool p3Peers::getPeerDetails(unsigned int librarymixer_id, PeerDetails &detailsToFill) {
    if (librarymixer_id == getOwnLibraryMixerId()) {
        detailsToFill.localAddr = inet_ntoa(ownConnectivityManager->getOwnLocalAddress()->sin_addr);
        detailsToFill.localPort = ntohs(ownConnectivityManager->getOwnLocalAddress()->sin_port);
        detailsToFill.extAddr = inet_ntoa(ownConnectivityManager->getOwnExternalAddress()->sin_addr);
        detailsToFill.extPort = ntohs(ownConnectivityManager->getOwnExternalAddress()->sin_port);
        detailsToFill.librarymixer_id = librarymixer_id;
    } else {
        friendListing *requestedListing = friendsConnectivityManager->getFriendListing(librarymixer_id);

        if (!requestedListing) return false;

        detailsToFill.librarymixer_id = requestedListing->librarymixer_id;
        detailsToFill.name = requestedListing->name;
        detailsToFill.localAddr = inet_ntoa(requestedListing->localaddr.sin_addr);
        detailsToFill.localPort = ntohs(requestedListing->localaddr.sin_port);
        detailsToFill.extAddr = inet_ntoa(requestedListing->serveraddr.sin_addr);
        detailsToFill.extPort = ntohs(requestedListing->serveraddr.sin_port);
        detailsToFill.lastConnect = requestedListing->lastcontact;
        detailsToFill.state = requestedListing->state;
        detailsToFill.waitingForAnAction = (requestedListing->nextTryDelayedUntil != 0);
    }

    return true;
}

QString p3Peers::getPeerName(unsigned int librarymixer_id) {
    return friendsConnectivityManager->getPeerName(librarymixer_id);
}

/* Add/Remove Friends */
bool p3Peers::addUpdateFriend(unsigned int librarymixer_id, const QString &cert, const QString &name,
                              const QString &localIP, ushort localPort, const QString &externalIP, ushort externalPort) {
    return friendsConnectivityManager->addUpdateFriend(librarymixer_id, cert, name, localIP, localPort, externalIP, externalPort);
}

/* Network Stuff */
void p3Peers::connectAll() {
    friendsConnectivityManager->tryConnectToAll();
}

PeerDetails::PeerDetails()
    :state(FCS_NOT_CONNECTED), lastConnect(0) {
    return;
}
