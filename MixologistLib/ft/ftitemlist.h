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


#ifndef FT_ITEM_LIST_HEADER
#define FT_ITEM_LIST_HEADER

/*
 * This class enables sharing of "items" each of which is a LibraryMixerItem each
 * of which can potentially hold many files.
 * In essence, this class acts as a memory cache and interface for ftserver to the LibraryMixerLibraryManager.
 * This class should really be merged into LibraryMixerLibraryManager because they duplicate so much functionality,
 * yet differ in key ways that introduce many gotcha bugs.
 */

#include <string>
#include <QMap>
#include <QString>
#include "ft/ftsearch.h"
#include "interface/librarymixer-library.h"
#include "util/threads.h"
#include "interface/files.h"

class ftItemList: public MixThread, public ftSearch {

public:
    ftItemList() {}
    virtual void run();
    //Called by LibraryMixerLibraryManager::mergeLibrary when the library list is updated
    //Calls LibraryMixerLibraryManager::getMatched() to get the list of matched files which are loaded into memory
    //and added to a list to be hashed if they contain no hash
    void setItems();
    //Adds the item identified by the provided information to the list of items to be hashed.
    void addItem(LibraryMixerItem item);
    /*Creates a new temporary LibraryMixerItem with a negative id to hold the item that is not in the library on the website.
      Arranges for an invitation to be sent to recipient as soon as hashing is complete.
      If a requested_item_id is provided, then will attempt to set that as the id, unless it is already taken in which case:
      if the requested id is positive (indicating a LibaryMixer item), will overwrite (be careful!)
      if the requested id is negative (indicating a local item), will keep decrementing until it finds an open one.*/
    void addTempItem(QString title, QStringList paths, unsigned int librarymixer_id, int requested_item_id = -1);
    //Removes the item identified by the provided information from both mFiles and mToHash.
    void removeItem(int id);
    //Deletes the files and removes the item identified by the provided information from mFiles only.
    void deleteRemoveItem(int id);
    //Finds the LibraryMixerItem that corresponds to the item id, and returns it.
    //Returns a blank LibraryMixerItem on failure.
    LibraryMixerItem getItem(int id);
    //Finds the LibraryMixerItem that contains the same paths, and returns it.
    //Returns a blank LibraryMixerItem on failure.
    LibraryMixerItem getItem(QStringList paths);
    //Finds the LibraryMixerItem that corresponds to id in either mToHash or mFiles and returns its status.  Returns -1 on not found.
    int getItemStatus(int id);
    //Blocking function that immediately rechecks the identified item.
    //If unable to find the item, changed is false and returns NULL.
    //If the filesize has changed, changed is true, file is rehashed, changes are saved, and returns the modified LibraryMixerItem with itemState ITEM_MATCHED_TO_FILE.
    //If the filesize has not changed, changes is false, and returns the LibraryMixerItem with itemState ITEM_MATCHED_TO_FILE.
    //If the file could not be found, changed is true, hash is set to "" and filesize 0, and returns modified LibraryMixerItem with itemState ITEM_MATCH_NOT_FOUND.
    LibraryMixerItem *recheckItem(int id, bool *changed);
    /*Sends a download invitation to a friend for the given item.
      If the item is not yet ready, then adds them to the waiting list. */
    void MixologySuggest(unsigned int librarymixer_id, int item_id);
    //Search Function - used by File Transfer implementation of ftSearch.  Searches come in, and this returns the path of where to get the file.
    virtual bool search(std::string hash, uint64_t size, uint32_t hintflags, FileInfo &info) const;

private:
    /* Called by the thread loop to hash a single file off of mToHash, and
       calls LibraryMixerItemManager::updateItemNode to update the authoritative xml */
    void    hashAnItem();

    mutable MixMutex extMutex;

    QMap<int, LibraryMixerItem> mItems; //list of LibraryMixerItems, by their librarymixer item id
    QMap<int, LibraryMixerItem> mToHash; //embryonic mItems, that have to have their files hashed still
};
#endif
