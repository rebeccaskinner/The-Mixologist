/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
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

#include <time.h>
#include <services/statusservice.h>
#include <pqi/pqinotify.h>
#include <serialiser/statusitems.h>
#include <ft/ftofflmlist.h>
#include <interface/iface.h>
#include <interface/peers.h>
#include <interface/settings.h>
#include <util/debug.h>

/*Time between each wave of status updates
  145 seconds (two and a half minutes)
  Note that this is slightly less than half of the connection timeout period, so we should
  have two opportunities per timeout period to keep alive. */
#define SEND_ALL_PERIOD 145

StatusService::StatusService()
    :p3Service(SERVICE_TYPE_STATUS) {
    addSerialType(new StatusSerialiser());
    timeOfLastSendAll = time(NULL);
}

int StatusService::tick() {
    QMutexLocker stack(&statusMutex);

    /* Send */
    QList<unsigned int> *listToUse;
    QList<unsigned int> friends;
    if (time(NULL) - timeOfLastSendAll > SEND_ALL_PERIOD) {
        peers->getOnlineList(friends);
        listToUse = &friends;
        timeOfLastSendAll = time(NULL);
    } else {
        listToUse = &friendsToSendKeepAlive;
    }
    QString offLMXmlHash = "";
    qlonglong offLMXmlSize = 0;
    if (offLMList) offLMList->getOwnOffLMXmlInfo(&offLMXmlHash, &offLMXmlSize);
    foreach (unsigned int friend_id, *listToUse) {
        BasicStatusItem *item = new BasicStatusItem();
        item->LibraryMixerId(friend_id);

        item->offLMXmlHash = offLMXmlHash;
        item->offLMXmlSize = offLMXmlSize;

        sendItem(item);
    }
    friendsToSendKeepAlive.clear();

    /* Receive */
    NetItem *netitem;
    while ((netitem=recvItem()) != NULL) {
        BasicStatusItem *statusItem = dynamic_cast<BasicStatusItem*>(netitem);
        if (statusItem != NULL) {
            if (offLMList) offLMList->receiveFriendOffLMXmlInfo(statusItem->LibraryMixerId(),
                                                                statusItem->offLMXmlHash,
                                                                statusItem->offLMXmlSize);
        }

        OnConnectStatusItem *onConnectItem = dynamic_cast<OnConnectStatusItem*>(netitem);
        if (onConnectItem != NULL) {
            if (offLMList) offLMList->receiveFriendOffLMXmlInfo(onConnectItem->LibraryMixerId(),
                                                                onConnectItem->offLMXmlHash,
                                                                onConnectItem->offLMXmlSize);
            if (onConnectItem->clientName == control->clientName() &&
                onConnectItem->clientVersion > control->clientVersion() &&
                onConnectItem->clientVersion > control->latestKnownVersion()) {
                log(LOG_WARNING, STATUSSERVICEZONE,
                    "Friend connected with new version " + QString::number(onConnectItem->clientVersion) +
                    ", own version is " + QString::number(control->clientVersion()));
                getPqiNotify()->AddPopupMessage(POPUP_NEW_VERSION_FROM_FRIEND,
                                                peers->getPeerName(onConnectItem->LibraryMixerId()),
                                                QString::number(onConnectItem->clientVersion));
                control->setVersion(control->clientName(), control->clientVersion(), onConnectItem->clientVersion);
            }
        }

        delete netitem;
    }
    return 1;
}

void StatusService::sendStatusUpdateToAll() {
    QMutexLocker stack(&statusMutex);
    timeOfLastSendAll = 0;
}

void StatusService::sendKeepAlive(unsigned int friend_id) {
    QMutexLocker stack(&statusMutex);
    friendsToSendKeepAlive.append(friend_id);
}

void StatusService::statusChange(const std::list<pqipeer> &changedFriends) {
    QMutexLocker stack(&statusMutex);
    QString offLMXmlHash = "";
    qlonglong offLMXmlSize = 0;
    if (offLMList) offLMList->getOwnOffLMXmlInfo(&offLMXmlHash, &offLMXmlSize);

    foreach(pqipeer currentPeer, changedFriends) {
        if (currentPeer.actions & PEER_CONNECTED) {
            OnConnectStatusItem *item = new OnConnectStatusItem(control->clientName(), control->clientVersion());
            item->offLMXmlHash = offLMXmlHash;
            item->offLMXmlSize = offLMXmlSize;
            item->LibraryMixerId(currentPeer.librarymixer_id);
            sendItem(item);
        }
    }
}
