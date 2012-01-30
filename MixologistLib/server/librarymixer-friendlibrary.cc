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

#include <server/librarymixer-friendlibrary.h>
#include <QDomDocument>

void LibraryMixerFriendLibrary::mergeLibrary(const QDomElement &libraryElement) {
    QMutexLocker stack(&friendLibMutex);

    QDomElement currentItemNode = libraryElement.firstChildElement("item");
    QList<unsigned int> preexistingItems = libraryList.keys();

    /* Add or update all items in the new list. */
    while (!currentItemNode.isNull()) {
        unsigned int itemId = currentItemNode.firstChildElement("id").text().toUInt();
        if (libraryList.contains(itemId)) {
            preexistingItems.removeOne(itemId);
            if (libraryList[itemId]->title() != currentItemNode.firstChildElement("title").text()) {
                libraryList[itemId]->title(currentItemNode.firstChildElement("title").text());
                emit friendLibraryStateChanged(libraryList.keys().indexOf(itemId));
            }
        } else {
            FriendLibraryMixerItem* newItem = new FriendLibraryMixerItem(currentItemNode.firstChildElement("id").text().toUInt(),
                                                                         currentItemNode.firstChildElement("userid").text().toUInt(),
                                                                         currentItemNode.firstChildElement("title").text());
            insertIntoList(newItem);
        }
        currentItemNode = currentItemNode.nextSiblingElement("item");
    }

    /* Delete any items removed online. */
    while (!preexistingItems.isEmpty()) {
        unsigned int itemId = preexistingItems.first();
        preexistingItems.removeFirst();
        removeFromList(itemId);
    }
}

QMap<unsigned int, FriendLibraryMixerItem*>*  LibraryMixerFriendLibrary::getFriendLibrary() {
    return &libraryList;
}

//private utility functions
void  LibraryMixerFriendLibrary::insertIntoList(FriendLibraryMixerItem* item) {
    /* We need to find the row where our new item will be inserted so we can report it in our signals.
       To do this, we step through the keys, which are guaranteed to be sorted in ascending order.
       If we find an item id that is less than our new item's id, that's the item we will be inserting before.
       Otherwise, it we reach the end, we stick with our default value of the final spot. */
    QList<unsigned int> keys = libraryList.keys();
    int row = keys.length();
    for (QList<unsigned int>::const_iterator it = keys.begin(); it != keys.end(); it++ ) {
        if (*it < item->item_id()) {
            row = keys.indexOf(*it);
            break;
        }
    }
    emit friendLibraryItemAboutToBeInserted(row);
    libraryList.insert(item->item_id(), item);
    emit friendLibraryItemInserted();
}

void  LibraryMixerFriendLibrary::removeFromList(unsigned int item_id) {
    int row = libraryList.keys().indexOf(item_id);
    if (row == -1) return;
    emit friendLibraryItemAboutToBeRemoved(row);
    delete libraryList[item_id];
    libraryList.remove(item_id);
    emit friendLibraryItemRemoved();
}
