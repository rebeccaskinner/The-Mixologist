/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2008, Robert Fernie
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

#include <ft/fttemplist.h>
#include <ft/ftfilewatcher.h>
#include <ft/ftserver.h>
#include <ft/ftborrower.h>
#include <interface/peers.h>
#include <services/mixologyservice.h>
#include <pqi/pqinotify.h>
#include <util/xml.h>
#include <util/dir.h>
#include <interface/iface.h> //For the libraryconnect and notifyBase
#include <time.h>

#define TEMPITEMFILE "tempItems.xml"

ftTempList::ftTempList() {
    connect(fileWatcher, SIGNAL(newFileHash(QString,qlonglong,uint,QString)), this, SLOT(newFileHash(QString,qlonglong,uint,QString)), Qt::QueuedConnection);
    connect(fileWatcher, SIGNAL(oldHashInvalidated(QString,qlonglong,uint)), this, SLOT(oldHashInvalidated(QString,qlonglong,uint)), Qt::QueuedConnection);
    connect(fileWatcher, SIGNAL(fileRemoved(QString)), this, SLOT(fileRemoved(QString)));

    QDomElement rootNode;
    if (XmlUtil::openXml(TEMPITEMFILE, xml, rootNode, "tempitems", QIODevice::ReadWrite)){
        QDomNode currentNode = rootNode.firstChildElement("item");
        while (!currentNode.isNull()){
            TempShareItem* currentItem = new TempShareItem(currentNode);
            if (currentItem->expiration() < time(NULL)) {
                delete currentItem;
                XmlUtil::writeXml(TEMPITEMFILE, xml);
            } else {
                connect(currentItem, SIGNAL(fileNoLongerAvailable(QString,qulonglong)), this, SIGNAL(fileNoLongerAvailable(QString,qulonglong)));
                tempList.append(currentItem);
                //Check if any items are missing their hashes, continue hashing
                for (int i = 0; i < currentItem->fileCount(); i++) {
                    if (currentItem->hashes()[i].isEmpty()) fileWatcher->addFile(currentItem->paths()[i]);
                    else fileWatcher->addFile(currentItem->paths()[i], currentItem->filesizes()[i], currentItem->modified()[i]);
                }
            }
            currentNode = currentNode.nextSiblingElement("item");
        }
    }
}

void ftTempList::addTempItem(const QString &title, const QStringList &paths, unsigned int friend_id, const QString &itemKey) {
    QMutexLocker stack(&tempItemMutex);

    if (paths.empty() || (friend_id == 0)) return;

    /* If we've already got this in the database, checks if it's unchanged.
       If it is, sends another download invitation, otherwise remove it so we can re-add.
       If we already have something else with this borrowKey that's not a duplicate, just remove it so we can create a new one. */
    foreach (TempShareItem* item, tempList) {
        if (item->label() == title && DirUtil::allPathsMatch(item->paths(), paths) && item->friend_id() == friend_id && item->borrowKey() == itemKey) {
            sendTempItemIfReady(item);
            return;
        } else if (item->borrowKey() == itemKey) {
            removeTempItem(item);
        }
    }

    TempShareItem* newItem = new TempShareItem(xml, title, paths, friend_id);
    if (!itemKey.isEmpty()) newItem->borrowKey(itemKey);
    tempList.append(newItem);
    connect(newItem, SIGNAL(fileNoLongerAvailable(QString,qulonglong)), this, SIGNAL(fileNoLongerAvailable(QString,qulonglong)));
    XmlUtil::writeXml(TEMPITEMFILE, xml);

    notifyBase->notifyUserOptional(friend_id, NOTIFY_USER_SUGGEST_WAITING, title);

    foreach(QString path, paths) fileWatcher->addFile(path);
}

void ftTempList::removeReturnBorrowedTempItem(const QString &itemKey) {
    QMutexLocker stack(&tempItemMutex);
    foreach (TempShareItem* item, tempList) {
        if (item->borrowKey() == itemKey) {
            /* Cache the paths as removeTempItem will destroy the item.
               However, we don't want to try to remove the files before calling removeTempItem as it ensures any file handles are closed. */
            QStringList paths = item->paths();
            removeTempItem(item);
            foreach (QString path, paths) {
                fileWatcher->stopWatching(path);
                if (!QFile::remove(path)) getPqiNotify()->AddSysMessage(0, SYS_WARNING, "File remove error", "Unable to remove borrowed file " + path);
            }
            return;
        }
    }
}

ftFileMethod::searchResult ftTempList::search(const QString &hash, qlonglong size, uint32_t hintflags, QString &path) {
    if (hintflags | FILE_HINTS_TEMP){
        QMutexLocker stack(&tempItemMutex);
        foreach(TempShareItem* item, tempList) {
            for (int i = 0; i < item->fileCount(); i++) {
                if (hash == item->hashes()[i] && size == item->filesizes()[i]) {
                    path = item->paths()[i];

                    /* Everytime a file in this tempShareItem is accessed, we will renew the expiration of the tempShareItem to 2 weeks
                       from now.
                       Note that this access via search only occurs once per file per time the Mioxlogist is opened,
                       because after the initial search, and ftFileProvider is created by ftDataMultiplex, and the file won't need to be
                       searched again to be provided.
                       In the future we might want a better method, in case users keep their Mixologist open for weeks without restarting. */
                    item->expirationSetToTwoWeeks();

                    return ftFileMethod::SEARCH_RESULT_FOUND_SHARED_FILE;
                }
            }
        }
    }
    return ftFileMethod::SEARCH_RESULT_NOT_FOUND;
}

void ftTempList::oldHashInvalidated(QString path, qlonglong size, unsigned int modified) {
    newFileHash(path, size, modified, "");
}

void ftTempList::newFileHash(QString path, qlonglong size, unsigned int modified, QString newHash) {
    QMutexLocker stack(&tempItemMutex);

    foreach(TempShareItem* item, tempList) {
        /* Once a TempShareItem is fully hashed, it is in its final state, and never modified again except to remove individual bad files,
           because at that point, it has already begun to be sent out to its recipient, and updating it would not be useful.
           If we are receiving a path contained in a fullyHashed item, and the hash has changed, that means our old info has been invalidated. */
        if (item->fullyHashed()) {
            for (int i = 0; i < item->fileCount(); i++) {
                if (path == item->paths()[i] &&
                    newHash != item->hashes()[i]) {
                    getPqiNotify()->AddPopupMessage(POPUP_MISC,
                                                    "File changed, sharing cancelled",
                                                    path + "\nthat was shared with\n" + peers->getPeerName(item->friend_id()) + "\nhas changed on disk and is no longer shared.");
                    removeFile(item, i);
                    break;
                }
            }
        } else {
            for (int i = 0; i < item->fileCount(); i++) {
                if (path == item->paths()[i]) {
                    item->setFileInfo(path, size, modified, newHash);
                    XmlUtil::writeXml(TEMPITEMFILE, xml);
                    sendTempItemIfReady(item);
                }
            }
        }
    }
}

void ftTempList::fileRemoved(QString path) {
    QMutexLocker stack(&tempItemMutex);

    foreach(TempShareItem* item, tempList) {
        /* Once a TempShareItem is fully hashed, it is in its final state, and never modified again except to remove individual bad files,
           because at that point, it has already begun to be sent out to its recipient, and updating it would not be useful.
           If we are receiving a path contained in a fullyHashed item, and the hash has changed, that means our old info has been invalidated. */
        if (item->fullyHashed()) {
            for (int i = 0; i < item->fileCount(); i++) {
                if (path == item->paths()[i]) {
                    getPqiNotify()->AddPopupMessage(POPUP_MISC,
                                                    "File missing",
                                                    path + "\nthat was shared with\n" + peers->getPeerName(item->friend_id()) + "\nwas removed on disk.");
                    removeFile(item, i);
                    break;
                }
            }
        } else {
            for (int i = 0; i < item->fileCount(); i++) {
                if (path == item->paths()[i]) {
                    getPqiNotify()->AddPopupMessage(POPUP_MISC,
                                                    "File missing",
                                                    item->label() + "\nthat was to be sent to\n" + peers->getPeerName(item->friend_id()) + "\nis missing at least one file, send cancelled.");
                    removeTempItem(item);
                    break;
                }
            }
        }
    }
}

void ftTempList::removeFile(TempShareItem *item, int index) {
    item->removeFile(index);
    if (item->fileCount() > 0) XmlUtil::writeXml(TEMPITEMFILE, xml);
    else removeTempItem(item);
}

void ftTempList::removeTempItem(TempShareItem *item) {
    tempList.removeOne(item);
    for (int i = 0; i < item->fileCount(); i++) item->removeFile(i);
    delete item;
    XmlUtil::writeXml(TEMPITEMFILE, xml);
}

void ftTempList::sendTempItemIfReady(TempShareItem *item) {
    if (!item->fullyHashed()) return;
    if (item->borrowKey().isEmpty()) {
        mixologyService->sendSuggestion(item->friend_id(),
                                        item->label(),
                                        DirUtil::getRelativePaths(item->paths()),
                                        item->hashes(),
                                        item->filesizes());
    } else {
        borrowManager->returnBorrowed(item->borrowKey(),
                                      DirUtil::getRelativePaths(item->paths()),
                                      item->hashes(),
                                      item->filesizes());
    }
}
