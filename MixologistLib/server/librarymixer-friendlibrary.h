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

#ifndef LibraryMixerFriendLibrary_H
#define LibraryMixerFriendLibrary_H

#include "interface/files.h"
#include "interface/types.h"
#include <QMap>
#include <QMutex>

/* Keeps track of the list of items in friends' libraries made available on LibraryMixer. */

class LibraryMixerFriendLibrary;

extern LibraryMixerFriendLibrary *libraryMixerFriendLibrary;

class LibraryMixerFriendLibrary: public QObject {
    Q_OBJECT

    /**********************************************************************************
     * Interface for MixologistLib
     **********************************************************************************/

public:
    /* Called whenever the library list is downloaded from LibraryMixer.
       Merges an xml list of items to the existing lists.  Assumes that they are in the same order,
       with newest first. */
    void mergeLibrary(const QDomElement &libraryElement);

    /**********************************************************************************
     * Body of public-facing API functions called through ftServer
     **********************************************************************************/

    /* Returns a map of all items that have been matched to someting by a user, other than those MATCH_NOT_FOUND.
       The map is by LibraryMixer item IDs. */
    QMap<unsigned int, FriendLibraryMixerItem*>* getFriendLibrary();

signals:
    void friendLibraryItemAboutToBeInserted(int row);
    void friendLibraryItemInserted();
    void friendLibraryItemAboutToBeRemoved(int row);
    void friendLibraryItemRemoved();
    void friendLibraryStateChanged(int row);

private:
    /* Instead of directly altering the lists, changes should be made through these functions,
       which emit signals to ensure the GUI is informed of pending changes.
       These are not mutex protected, so only call from inside the mutex. */
    void insertIntoList(FriendLibraryMixerItem* item);
    void removeFromList(unsigned int item_id);

    mutable QMutex friendLibMutex;

    QMap<unsigned int, FriendLibraryMixerItem*> libraryList;

};

#endif
