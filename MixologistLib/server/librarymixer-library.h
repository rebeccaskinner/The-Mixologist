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

#ifndef LibraryMixerLibraryManager_H
#define LibraryMixerLibraryManager_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QIODevice>
#include <QDomElement>
#include <QMutex>
#include <server/librarymixer-libraryitem.h>
#include <ft/ftfilemethod.h>
#include <interface/files.h>
#include <interface/types.h>

class LibraryMixerLibraryManager;

extern LibraryMixerLibraryManager *librarymixermanager;

/* Keeps track of the list of items in a user's library and made available to friends on LibraryMixer.
   In addition, keeps track of how to respond for each item when a friend requests it.
   Stores all of this in a file library.xml. */

class LibraryMixerLibraryManager: public ftFileMethod {
    Q_OBJECT

    /**********************************************************************************
     * Interface for MixologistLib
     **********************************************************************************/

public:

    LibraryMixerLibraryManager();

    /* Called whenever the library list is downloaded from LibraryMixer.
       Merges an xml list of items to the existing library.xml.  Assumes that they are in the same order,
       with newest first. */
    void mergeLibrary(const QDomElement &libraryElement);

    /* Sets the given item that is currently MATCHED_TO_LEND to MATCHED_TO_LENT
       and deletes all files that it matches. */
    bool setLent(unsigned int friend_id, const QString &item_id);

    /* Called on the return of borrowed items to return the items to their original locations.
       Returns:
       1 on success and populates finalDestination with the directory where they were placed,
       0 if at least one file could not be returned,
       -1 if file count has changed,
       -2 if file contents have changed,
       -3 if there is no item with that id,
       -4 if that item is not lent out,
       -5 if the item is lent, but not to that friend.
       For each file, attempts to move the file back to its original locations if moveFile is true, otherwise uses copy.
       Will notify GUI via AddSysMessage of any files could not be either moved or copied. */
    int returnBorrowedFiles(unsigned int friend_id, const QString &item_id, const QStringList &paths, const QStringList &hashes,
                            const QList<qlonglong> &filesizes, const QList<bool> &moveFile, QString &finalDestination);

    /* Checks if the item's files are still fully valid.
       If the item couldn't be found with that item_id, returns -1;
       If the files are not fully available, means the item is not currently ready and returns 0. Can be called repeatedly.
       Otherwise, return 1.
       If any files are missing, changes item state to MATCH_NOT_FOUND but other returns 1 as normal. If returning 1, populates item. */
    int getLibraryMixerItemWithCheck(unsigned int item_id, LibraryMixerItem &item);

    /**********************************************************************************
     * Body of public-facing API functions called through ftServer
     **********************************************************************************/

    /* Fills the hash with all of a user's LibraryMixerItems keyed by LibraryMixer item IDs. */
    void getLibrary(QMap<unsigned int, LibraryMixerItem> &library) const;

    /* Finds the LibraryMixerItem that corresponds to the item id, and returns it.
       Returns false on failure. */
    bool getLibraryMixerItem(unsigned int item_id, LibraryMixerItem &itemToFill) const;

    /* Finds the LibraryMixerItem that contains the same paths, and returns it.
       Returns false on failure. */
    bool getLibraryMixerItem(QStringList paths, LibraryMixerItem &itemToFill) const;

    /* Sets the given item to MATCHED_TO_CHAT. */
    bool setMatchChat(unsigned int item_id);

    /* Sets the the given item to be matched for file, either for send or lend.
       If paths is not empty, clears out all pre-existing path info and requests that they be hashed by Files.
       If paths is empty, then this can be used to toggle the ItemState.
       Sets ItemState MATCHED_TO_FILE or MATCHED_TO_LEND based on ItemState.
       If recipient is added with librarymixer id, on completion of hash, a download invitation will be sent. */
    bool setMatchFile(unsigned int item_id, QStringList paths, LibraryMixerItem::ItemState itemState, unsigned int recipient = 0);

    /* Sets the given item with the autorespond message and to MATCHED_TO_MESSAGE. */
    bool setMatchMessage(unsigned int item_id, QString message);

    /* Sends a download invitation to a friend for the given item.
       If the item is not yet ready, then adds them to the waiting list. */
    void MixologySuggest(unsigned int friend_id, int item_id);

    /**********************************************************************************
     * Implementations for ftFileMethod
     **********************************************************************************/

    /* Returns true if it is able to find a file with matching hash and size.
       If a file is found, populates path. */
    virtual ftFileMethod::searchResult search(const QString &hash, qlonglong size, uint32_t hintflags, unsigned int librarymixer_id, QString &path);

signals:
    void libraryItemInserted(unsigned int item_id);
    void libraryItemRemoved(unsigned int item_id);
    void libraryStateChanged(unsigned int item_id);

private slots:
    /* Connected to ftFileWatcher for when a file's old hash is no longer valid.
       The path should be supplied with native directory separators. */
    void oldHashInvalidated(QString path, qlonglong size, unsigned int modified);

    /* Connected to ftFileWatcher for when a file has been hashed in order to update with the new information.
       The path should be supplied with native directory separators. */
    void newFileHash(QString path, qlonglong size, unsigned int modified, QString newHash);

    /* Connected to ftFileWatcher for when a file has been removed.
       The path should be supplied with native directory separators. */
    void fileRemoved(QString path);

private:
    /* Returns a LibraryMixer item with the information filled out from the inputItem. */
    LibraryMixerItem internalConvertItem(LibraryMixerLibraryItem* inputItem) const;

    /* Called whenever we change our item state in a way that would invalidate all of our file share information.
       Can be called even if the old item state didn't have any files. */
    void oldFilesNoLongerAvailable(const LibraryMixerLibraryItem& item);
    void oldFilesNoLongerAvailable(const QStringList &hashes, const QList<qlonglong> &filesizes);

    /* Returns true if both a and b have the same non-null item_id. */
    bool equalNode(const QDomNode a, const QDomNode b) const;

    mutable QMutex libMutex;
    QDomDocument xml; //The master XML for a user's library
    /* These are how we store the LibraryMixerLibraryItems that wrap the xml.
       The key is the item's id on LibraryMixer. */
    QMap<unsigned int, LibraryMixerLibraryItem*> libraryList;

};

#endif
