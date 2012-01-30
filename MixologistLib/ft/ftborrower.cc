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

#include <ft/ftborrower.h>
#include <ft/fttemplist.h>
#include <ft/ftserver.h>
#include <services/mixologyservice.h>
#include <interface/settings.h>
#include <QSettings>
#include <QMutexLocker>

QString BorrowedItem::itemKey() const {
    return generateItemKey(friend_id, source_type, source_id);
}

void BorrowedItem::readKey(const QString &key) {
    int firstIndex = key.indexOf("|");
    friend_id = key.left(firstIndex).toInt();
    int secondIndex = key.indexOf("|", firstIndex + 1);
    source_type = key.mid(firstIndex + 1, (secondIndex - 1) - firstIndex).toInt(); //We use secondIndex - 1 to avoid including the "|"
    source_id = key.mid(secondIndex + 1);
}

QString BorrowedItem::generateItemKey(unsigned int friend_id, int source_type, const QString &source_id) {
    /* We can safely use an arbitrary delimiter between them because of these 3 fields, only the last one is a string,
       which means that there is no possibility for a stray "|" to creep in there and screw things up. */
    return QString::number(friend_id) + "|" + QString::number(source_type) + "|" + source_id;
}

ftBorrowManager::ftBorrowManager() {
    QMutexLocker locker(&borrowMutex);
    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Borrowed");
    foreach (QString itemKey, saved.childKeys()) {
        BorrowedItem item;
        item.readKey(itemKey);
        item.title = saved.value(itemKey).toString();

        borrowedItems[itemKey] = item;
    }
}

void ftBorrowManager::addBorrowed(unsigned int friend_id, int source_type, const QString &source_id, const QString &title) {
    //Basic validity check
    if (friend_id == 0 || source_type == 0 || source_id.isEmpty() || title.isEmpty()) return;

    BorrowedItem item;
    item.friend_id = friend_id;
    item.source_type = source_type;
    item.source_id = source_id;
    item.title = title;
    QString itemKey = item.itemKey();

    deleteBorrowed(itemKey);

    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Borrowed");
    saved.setValue(itemKey, title);
    saved.sync();

    mixologyService->sendBorrowed(friend_id, source_type, source_id);

    QMutexLocker locker(&borrowMutex);
    borrowedItems[itemKey] = item;
}

void ftBorrowManager::deleteBorrowed(const QString &itemKey) {
    QMutexLocker locker(&borrowMutex);

    borrowedItems.remove(itemKey);

    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Borrowed");
    saved.remove(itemKey);
    saved.sync();
}

void ftBorrowManager::getBorrowings(QStringList &titles, QStringList &itemKeys, QList<unsigned int> &friendIds) {
    QMutexLocker locker(&borrowMutex);
    foreach (QString currentKey, borrowedItems.keys()) {
        titles.append(borrowedItems[currentKey].title);
        itemKeys.append(borrowedItems[currentKey].itemKey());
        friendIds.append(borrowedItems[currentKey].friend_id);
    }
}

void ftBorrowManager::getBorrowings(QStringList &titles, QStringList &itemKeys, unsigned int friend_id) {
    QMutexLocker locker(&borrowMutex);
    foreach (QString currentKey, borrowedItems.keys()) {
        if (friend_id == borrowedItems[currentKey].friend_id) {
            titles.append(borrowedItems[currentKey].title);
            itemKeys.append(borrowedItems[currentKey].itemKey());
        }
    }
}

void ftBorrowManager::returnBorrowed(const QString &itemKey, const QStringList &files, const QStringList &hashes, const QList<qlonglong> &filesizes) {
    QMutexLocker locker(&borrowMutex);
    if (!borrowedItems.contains(itemKey)) return;
    mixologyService->sendReturn(borrowedItems[itemKey].friend_id,
                                borrowedItems[itemKey].source_type,
                                borrowedItems[itemKey].source_id,
                                "Return of " + borrowedItems[itemKey].title,
                                files,
                                hashes,
                                filesizes);
}

void ftBorrowManager::returnedBorrowed(unsigned int friend_id, int source_type, const QString &source_id) {
    QString itemKey = BorrowedItem::generateItemKey(friend_id, source_type, source_id);
    {
        QMutexLocker locker(&borrowMutex);
        /* No need to check for identity match explicitly, since the itemKey is a check for all 3 arguments. */
        if (!borrowedItems.contains(itemKey)) return;
    }

    tempList->removeReturnBorrowedTempItem(itemKey);

    deleteBorrowed(itemKey);
}
