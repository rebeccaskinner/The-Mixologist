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
#define RETRY_DELAY 145

StatusService::StatusService()
    :p3Service(SERVICE_TYPE_STATUS) {
    addSerialType(new StatusSerialiser());
    timeOfLastTry = time(NULL);
}

int StatusService::tick() {
    QMutexLocker stack(&statusMutex);
    if (time(NULL) - timeOfLastTry > RETRY_DELAY) {
        std::list<int> friends;
        std::list<int>::iterator it;
        peers->getOnlineList(friends);
        QString offLMXmlHash = "";
        qlonglong offLMXmlSize = 0;
        if (offLMList) offLMList->getOwnOffLMXmlInfo(&offLMXmlHash, &offLMXmlSize);
        for (it = friends.begin(); it != friends.end(); it++) {
            BasicStatusItem *item = new BasicStatusItem();
            item->PeerId(peers->findCertByLibraryMixerId(*it));

            item->offLMXmlHash = offLMXmlHash;
            item->offLMXmlSize = offLMXmlSize;

            sendItem(item);
        }
        timeOfLastTry = time(NULL);
    }
    NetItem *netitem;
    while ((netitem=recvItem()) != NULL) {
        BasicStatusItem *statusItem = dynamic_cast<BasicStatusItem*>(netitem);
        if (statusItem != NULL) {
            if (offLMList) offLMList->receiveFriendOffLMXmlInfo(peers->findLibraryMixerByCertId(statusItem->PeerId()),
                                                                statusItem->offLMXmlHash,
                                                                statusItem->offLMXmlSize);
        }

        OnConnectStatusItem *onConnectItem = dynamic_cast<OnConnectStatusItem*>(netitem);
        if (onConnectItem != NULL) {
            if (offLMList) offLMList->receiveFriendOffLMXmlInfo(peers->findLibraryMixerByCertId(onConnectItem->PeerId()),
                                                                onConnectItem->offLMXmlHash,
                                                                onConnectItem->offLMXmlSize);
            if (onConnectItem->clientName == control->clientName() &&
                onConnectItem->clientVersion > control->clientVersion() &&
                onConnectItem->clientVersion > control->latestKnownVersion()) {
                log(LOG_WARNING, STATUSSERVICEZONE,
                    "Friend connected with new version " + QString::number(onConnectItem->clientVersion) +
                    ", own version is " + QString::number(control->clientVersion()));
                getPqiNotify()->AddPopupMessage(POPUP_NEW_VERSION_FROM_FRIEND,
                                                peers->getPeerName(peers->findLibraryMixerByCertId(onConnectItem->PeerId())),
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
    timeOfLastTry = 0;
}

void StatusService::statusChange(const std::list<pqipeer> &plist) {
    QMutexLocker stack(&statusMutex);
    QString offLMXmlHash = "";
    qlonglong offLMXmlSize = 0;
    if (offLMList) offLMList->getOwnOffLMXmlInfo(&offLMXmlHash, &offLMXmlSize);

    foreach(pqipeer currentPeer, plist) {
        if (currentPeer.actions & PEER_CONNECTED) {
            OnConnectStatusItem *item = new OnConnectStatusItem(control->clientName(), control->clientVersion());
            item->offLMXmlHash = offLMXmlHash;
            item->offLMXmlSize = offLMXmlSize;
            item->PeerId(currentPeer.cert_id);
            sendItem(item);
        }
    }
}
