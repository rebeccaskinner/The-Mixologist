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

#include <ft/ftitemlist.h>
#include <util/dir.h>
#include <QFile>
#include <QFileInfo>
#include <interface/iface.h>
#include <pqi/pqinotify.h>
#include <ft/ftserver.h>

/******
 * #define DEBUG_ELIST  1
 *****/

void ftItemList::run() {
    bool todo = false;
    time_t cleanup = 0;
    time_t now = 0;

    while (true) {
        now = time(NULL);

        {
            MixStackMutex stack(extMutex);
            todo = (mToHash.size() > 0);
        }

        if (todo) {
            /* Hash a file */
            hashAnItem();
#ifdef WIN32
            Sleep(1); //milliseconds
#else
            usleep(1000); //microseconds
#endif
        } else {
            /* cleanup */
            if (cleanup < now) {
                //FUTODO check old files for changes
                cleanup = now + 600;
            }
            /* sleep */
#ifdef WIN32
            Sleep(1000);
#else
            sleep(1);
#endif
        }
    }
}

void    ftItemList::setItems() {
    std::list<LibraryMixerItem> input = LibraryMixerLibraryManager::getMatched();

    /*Note that this will overwrite values, but will preserve all non-overwritten values.
      This will enable started transfers to continue even after the item has been removed
      and protects temporary items against overwrite.*/
    std::list<LibraryMixerItem>::iterator it;
    for (it = input.begin(); it != input.end(); it++) {
        /* add into system */
        //Not normal to have only some files hashed, so if any are unhashed, rehash all.
        for (int i = 0; i < it->paths.count(); i++) {
            if (it->hashes[i].isEmpty()) {
                MixStackMutex stack(extMutex);
                mToHash[it->id] = *it;
                continue;
            } else {
                MixStackMutex stack(extMutex);
                mItems[it->id] = *it;
            }
        }
    }
}

void ftItemList::hashAnItem() {
    /* extract entry from the queue */
    LibraryMixerItem details;
    {
        MixStackMutex stack(extMutex);
        if (mToHash.size() == 0)
            return;
        details = mToHash.begin().value();
    }

    /* hash it! */
    for(int i = 0; i < details.paths.count(); i++) {
        control->getNotify().notifyHashingInfo(details.paths[i]);
        std::string hash;
        uint64_t size;
        if (DirUtil::getFileHash(details.paths[i], hash, size)) {
            details.hashes[i] = hash.c_str();
            details.filesizes[i] = size;
        }
    }
    control->getNotify().notifyHashingInfo("");
    MixStackMutex stack(extMutex);
    //Check whether mToHash has changed before committing our changes
    if (mToHash.begin().value().id != details.id ||
            mToHash.begin().value().itemState != details.itemState ||
            mToHash.begin().value().paths.count() != details.paths.count()) {
        return;
    }
    for (int i = 0; i < details.paths.count(); i++) {
        if (mToHash.begin().value().paths[i] != details.paths[i]) return;
    }
    mItems[details.id] = details;
    for (int i = 0; i < details.sendToOnHash.count(); i++) {
        ftserver->sendMixologySuggestion(details.sendToOnHash[i], details.id, details.title);
    }
    mItems[details.id].sendToOnHash.clear();
    mToHash.remove(details.id);
    LibraryMixerLibraryManager::updateItemNode(details);
}


void    ftItemList::addItem(LibraryMixerItem item) {
    //First check and remove any entry we may already with those identifiers
    removeItem(item.id);
    MixStackMutex stack(extMutex);

    //Not normal to have only some files hashed, so if any are unhashed, rehash all.
    for (int i = 0; i < item.paths.count(); i++) {
        if (item.hashes[i].isEmpty()) {
            mToHash[item.id] = item;
            continue;
        } else mItems[item.id] = item;
    }
}

void    ftItemList::addTempItem(QString title, QStringList paths, unsigned int librarymixer_id, int requested_item_id) {
    //First check and remove any entry we may already have with those identifiers
    LibraryMixerItem details;
    details.title = title;
    details.itemState = ITEM_MATCHED_TO_FILE;
    details.paths = paths;
    details.sendToOnHash << librarymixer_id;
    details.id = requested_item_id;
    control->getNotify().notifyUserOptional(librarymixer_id, NOTIFY_USER_SUGGEST_WAITING, title);

    {
        MixStackMutex stack(extMutex);
        if(requested_item_id != -1) {
            //Check for and remove any duplicates if we have a requested item id.
            if (mItems.contains(details.id)) mItems.remove(details.id);
            if (mToHash.contains(details.id)) mToHash.remove(details.id);
        } else {
            //Generate a local fake id number to represent the temporary item
            while((mItems.contains(details.id) ||
                    mToHash.contains(details.id))) {
                details.id--;
            }
        }
        //Initialize with empty elements as required by QList
        for (int i = 0; i < paths.count(); i++) {
            details.filesizes << 0;
            details.hashes << QString("");
        }
        mToHash[details.id] = details;
    }
}

void    ftItemList::removeItem(int id) {
    std::list<LibraryMixerItem>::iterator it;
    MixStackMutex stack(extMutex);
    if (mToHash.contains(id)) mToHash.remove(id);
    if (mItems.contains(id)) mItems.remove(id);
}

void ftItemList::deleteRemoveItem(int id) {
    std::list<LibraryMixerItem>::iterator it;
    MixStackMutex stack(extMutex);
    for(int i = 0; i < mItems[id].paths.count(); i++) {
        if (!QFile::remove(mItems[id].paths[i]))
            getPqiNotify()->AddSysMessage(0, SYS_WARNING, "File remove error", "Unable to remove returned file " + mItems[id].paths[i]);
    }
}

LibraryMixerItem ftItemList::getItem(int id) {
    std::list<LibraryMixerItem>::iterator it;
    MixStackMutex stack(extMutex);
    if(mItems.contains(id)) return mItems[id];
    if(mToHash.contains(id)) return mToHash[id];
    LibraryMixerItem item;
    return item;
}

LibraryMixerItem ftItemList::getItem(QStringList paths) {
    bool all_paths_match;
    QMap<int, LibraryMixerItem>::const_iterator it;
    MixStackMutex stack(extMutex);
    for (it = mItems.begin(); it != mItems.end(); it++) {
        if(it.value().paths.count() == paths.count()) {
            all_paths_match = true;
            for(int i = 0; i < paths.count(); i++) {
                if (!it.value().paths.contains(paths[i])) {
                    all_paths_match = false;
                    break;
                }
            }
            if (all_paths_match) return it.value();
        }
    }
    LibraryMixerItem item;
    return item;
}

int ftItemList::getItemStatus(int id) {
    MixStackMutex stack(extMutex);
    if (mItems.contains(id)) return mItems[id].itemState;
    else return -1;
}

LibraryMixerItem *ftItemList::recheckItem(int id, bool *changed) {
    *changed = false;
    std::list<LibraryMixerItem>::iterator it;
    MixStackMutex stack(extMutex);
    if (!mItems.contains(id)) return NULL;
    QFileInfo file;
    for(int i = 0; i < mItems[id].paths.count(); i++) {
        file.setFile(mItems[id].paths[i]);
        if (file.size() != mItems[id].filesizes[i]) {
            std::string hash;
            uint64_t filesize;
            control->getNotify().notifyHashingInfo(mItems[id].paths[i]);
            if (DirUtil::getFileHash(mItems[id].paths[i], hash, filesize)) {
                control->getNotify().notifyHashingInfo("");
                if(mItems[id].hashes[i] != hash.c_str() ||
                        mItems[id].filesizes[i] != filesize) {
                    *changed = true;
                    mItems[id].hashes[i] = hash.c_str();
                    mItems[id].filesizes[i] = filesize;
                } else continue;
            } else { //If unable to hash a file, we mark it as a missing file
                control->getNotify().notifyHashingInfo("");
                *changed = true;
                mItems[id].hashes[i] = "";
                mItems[id].filesizes[i] = 0;
                mItems[id].itemState = ITEM_MATCH_NOT_FOUND;
                break;
            }
        }
    }
    return &mItems[id];
}

void ftItemList::MixologySuggest(unsigned int librarymixer_id, int item_id) {
    MixStackMutex stack(extMutex);
    if (mToHash.contains(item_id)) {
        if (!mToHash[item_id].sendToOnHash.contains(librarymixer_id)) {
            mToHash[item_id].sendToOnHash << librarymixer_id;
            control->getNotify().notifyUserOptional(librarymixer_id, NOTIFY_USER_SUGGEST_WAITING, mToHash[item_id].title);
        }
    } else {
        if (mItems.contains(item_id)) {
            ftserver->sendMixologySuggestion(librarymixer_id, item_id, mItems[item_id].title);
        }
    }
}

bool    ftItemList::search(std::string hash, uint64_t size, uint32_t hintflags, FileInfo &info) const {
    (void) size;
    (void) hintflags;
    /* find hash */
    QMap<int, LibraryMixerItem>::const_iterator it;
    for(it = mItems.begin(); it != mItems.end(); it++) {
        for (int i = 0; i < it.value().hashes.count(); i++) {
            if (hash.c_str() == it.value().hashes[i]) {
                info.paths.append(it.value().paths[i]);
                info.hash = it->hashes[i].toStdString();
                info.size = it->filesizes[i];
                return true;
            }
        }
    }
    return false;
}

