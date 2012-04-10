/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2008, Robert Fernie.
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

#ifndef FILES_GUI_INTERFACE_H
#define FILES_GUI_INTERFACE_H

#include "interface/types.h"

#include <QString>

class Files;

extern Files *files;

/**********************************************************************************
 * These are used mainly by ftController at the moment
 **********************************************************************************/

const uint32_t FILE_CTRL_PAUSE = 0x00000100;
const uint32_t FILE_CTRL_START = 0x00000200;

const uint32_t FILE_RATE_TRICKLE = 0x00000001;
const uint32_t FILE_RATE_SLOW = 0x00000002;
const uint32_t FILE_RATE_STANDARD = 0x00000003;
const uint32_t FILE_RATE_FAST = 0x00000004;
const uint32_t FILE_RATE_STREAM_AUDIO = 0x00000005;
const uint32_t FILE_RATE_STREAM_VIDEO = 0x00000006;

const uint32_t FILE_PEER_ONLINE = 0x00001000;
const uint32_t FILE_PEER_OFFLINE = 0x00002000;

/**********************************************************************************
 * Used to refer to the various file sources.
 * Used both by file methods and also in borrowing.
 **********************************************************************************/

/* Indicates the temporary item list. */
const uint32_t FILE_HINTS_TEMP = 0x00000001;
/* Indicates the LibraryMixer item list */
const uint32_t FILE_HINTS_ITEM = 0x00000002;
/* Indicates the off-LibraryMixer share list */
const uint32_t FILE_HINTS_OFF_LM = 0x00000004;

/* The interface by which MixologistGui can control file transfers */
class Files: public QObject {
    Q_OBJECT

public:

    Files() {
        return;
    }
    virtual ~Files() {
        return;
    }

    /**********************************************************************************
     * Download / Upload Details.
     **********************************************************************************/

    virtual void getPendingRequests(std::list<pendingRequest> &requests) = 0;
    virtual void FileDownloads(QList<downloadGroupInfo> &downloads) = 0;
    virtual void FileUploads(QList<uploadFileInfo> &uploads) = 0;

    /**********************************************************************************
     * Directory Control
     **********************************************************************************/

    /* Set the directories as specified, saving changes to settings file. */
    virtual void setDownloadDirectory(QString path) = 0;
    virtual void setPartialsDirectory(QString path) = 0;

    /* Return the directories currently being used. */
    virtual QString getDownloadDirectory() = 0;
    virtual QString getPartialsDirectory() = 0;

    /**********************************************************************************
     *  Control of File Transfers and Requests for LibraryMixer Items
     **********************************************************************************/

    /* Creates a new download of a batch of files. */
    virtual bool downloadFiles(unsigned int friend_id, const QString &title, const QStringList &paths,
                               const QStringList &hashes, const QList<qlonglong> &filesizes) = 0;

    /* Creates a new download to borrow a batch of files.
       source_type is one of the file_hint constants defined above to indicate the type of share we are borrowing from.
       source_id's meaning changes depending on the source_type, but always uniquely identifies the item. */
    virtual bool borrowFiles(unsigned int friend_id, const QString &title, const QStringList &paths,
                             const QStringList &hashes, const QList<qlonglong> &filesizes,
                             uint32_t source_type, QString source_id) = 0;

    /* Cancels an entire downloadGroup. */
    virtual void cancelDownloadGroup(int groupId) = 0;

    /* Cancels an existing file transfer from a single downloadGroup. */
    virtual void cancelFile(int groupId, const QString &hash) = 0;

    /* Pauses or resumes a transfer. */
    virtual bool controlFile(int orig_item_id, QString hash, uint32_t flags) = 0;

    /* Removes all completed transfers and all errored pending requests. */
    virtual void clearCompletedFiles() = 0;

    /* Removes all items from the uploads list. (This is a visual only change). */
    virtual void clearUploads() = 0;

    /* Adds a request to request list for an item identified by item_id from friend with librarymixer_id
       labeling the request name. */
    virtual bool LibraryMixerRequest(unsigned int librarymixer_id, unsigned int item_id, QString name) = 0;

    /* Removes a request from the pending request list. */
    virtual bool LibraryMixerRequestCancel(unsigned int item_id) = 0;

    /**********************************************************************************
     * LibraryMixer Item Control
     **********************************************************************************/

    /* Returns a map of all LibraryMixerItems. The map is by LibraryMixer item IDs. */
    virtual QMap<unsigned int, LibraryMixerItem*>* getLibrary() = 0;

    /* Finds the LibraryMixerItem that corresponds to the item id, and returns it.
       Returns a blank LibraryMixerItem on failure. */
    virtual LibraryMixerItem* getLibraryMixerItem(unsigned int item_id) = 0;

    /* Finds the LibraryMixerItem that contains the same paths, and returns it.
       Returns a blank LibraryMixerItem on failure. */
    virtual LibraryMixerItem* getLibraryMixerItem(QStringList paths) = 0;

    /* Finds the LibraryMixerItem that corresponds to the item id, and returns its status.  Returns -2 on error, -1 on not found.
       Retry if true will update the item if the item is not immediately found and try again once. */
    virtual int getLibraryMixerItemStatus(unsigned int item_id, bool retry=true) = 0;

    /* Sets the given item to MATCHED_TO_CHAT. */
    virtual bool setMatchChat(unsigned int item_id) = 0;

    /* Sets the the given item to be matched for file, either for send or lend.
       If paths is not empty, clears out all pre-existing path info and requests that they be hashed by Files.
       If paths is empty, then this can be used to toggle the ItemState.
       Sets ItemState MATCHED_TO_FILE or MATCHED_TO_LEND based on ItemState.
       If recipient is added with librarymixer id, on completion of hash, a download invitation will be sent. */
    virtual bool setMatchFile(unsigned int item_id, QStringList paths, LibraryMixerItem::ItemState itemState, unsigned int recipient = 0) = 0;

    /* Sets the given item with the autorespond message and to MATCHED_TO_MESSAGE. */
    virtual bool setMatchMessage(unsigned int item_id, QString message) = 0;

    /* Sends a download invitation to a friend for the given item.
       If the item is not yet ready, then adds them to the waiting list. */
    virtual void MixologySuggest(unsigned int librarymixer_id, unsigned int item_id) = 0;

signals:
    /* Signals for responses received from requests for LibraryMixerItems. */
    /* When a response is received from a request, and it is an offer to lend a set of files. */
    void responseLendOfferReceived(unsigned int friend_id, unsigned int item_id, QString title, QStringList paths, QStringList hashes, QList<qlonglong> filesizes);

    /* Signals for the LibraryMixer library. */
    void libraryItemAboutToBeInserted(int row);
    void libraryItemInserted();
    void libraryItemAboutToBeRemoved(int row);
    void libraryItemRemoved();
    void libraryStateChanged(int row);

    /**********************************************************************************
     * Friend LibraryMixer Item Control
     **********************************************************************************/

public:
    /* Returns a map of all FriendLibraryMixerItems. The map is by LibraryMixer item IDs. */
    virtual QMap<unsigned int, FriendLibraryMixerItem*>* getFriendLibrary() = 0;

signals:
    /* Signals for the LibraryMixer friends' library. */
    void friendLibraryItemAboutToBeInserted(int row);
    void friendLibraryItemInserted();
    void friendLibraryItemAboutToBeRemoved(int row);
    void friendLibraryItemRemoved();
    void friendLibraryStateChanged(int row);

    /**********************************************************************************
     * Off LibraryMixer Sharing Control
     **********************************************************************************/
public:
    /* Adds a new share to the list of base root paths shared off-LibraryMixer.
       The supplied path should be in QT format with respect to directory separators.
       Returns false if duplicative and ignored. */
    virtual void addOffLMShare(QString path) = 0;

    /* Removes a share identified by its root OffLMShareItem from the list of base root paths shared off-LibraryMixer.
       Does nothing if given a non-root item.
       Returns true on success, false if toRemove was not removed. */
    virtual bool removeOffLMShare(OffLMShareItem* toRemove) = 0;

    /* Sets the shareMethod to the specified value. Returns true on success. */
    virtual bool setOffLMShareMethod(OffLMShareItem* toModify, OffLMShareItem::shareMethodState state) = 0;

    /* Sets the label to the specified value. Returns true on success. */
    virtual void setOffLMShareLabel(OffLMShareItem* toModify, QString newLabel) = 0;

    /* Returns the root item of the OffLMShareItem tree */
    virtual OffLMShareItem* getOwnOffLMRoot() const = 0;

    /* Queues an asynchronous recheck of all files in ftOffLMList. */
    virtual void recheckOffLMFiles() const = 0;

    /* Reads all existing saved friend off-LibraryMixer shares from disk, keyed by their ID.
       The caller will be responsible for calling delete on each OffLMShareItem it no longer needs. */
    virtual void readExistingFriendsOffLMShares(QHash<unsigned int, OffLMShareItem*> &friendRoots) const = 0;

    /* Reads a specific friend's off-LibraryMixer shares from disk.
       Useful for after a signal informs a listener that a new friend has been added. */
    virtual OffLMShareItem* readFriendOffLMShares(unsigned int friend_id) const = 0;

signals:
    /* Signals for the Off-LibraryMixer sharing. */
    void offLMFriendAdded(unsigned int friend_id);
    void offLMFriendRemoved(unsigned int friend_id);
    void offLMOwnItemAboutToBeAdded(OffLMShareItem* item);
    void offLMOwnItemAdded();
    void offLMOwnItemChanged(OffLMShareItem* item);
    void offLMOwnItemAboutToBeRemoved(OffLMShareItem* item);
    void offLMOwnItemRemoved();

    /**********************************************************************************
     * Temporary Share Control
     **********************************************************************************/

public:
    /* Creates a new temporary item.
       A download suggestion will be sent to recipient as soon as hashing is complete. */
    virtual void sendTemporary(QString title, QStringList paths, unsigned int friend_id) = 0;

    /* Creates a new temporary item to return a batch of files, with a return suggestion sent as soon as hashing is complete.
       itemKey is the identifier of the borrowed item that is used by the borrow database. */
    virtual void returnFiles(const QString &title, const QStringList &paths, unsigned int friend_id, const QString &itemKey) = 0;

    /**********************************************************************************
     * Borrowing Management
     **********************************************************************************/

    /* Gets all items borrowed, and populates them into parallel lists for titles, itemKeys that identify them, and the friends they are borrowed from. */
    virtual void getBorrowings(QStringList &titles, QStringList &itemKeys, QList<unsigned int> &friendIds) = 0;

    /* Get items borrowed from a specific friend. */
    virtual void getBorrowings(QStringList &titles, QStringList &itemKeys, unsigned int friendId) = 0;

    /* Called when a user declines to borrow an item or wants to remove something from the borrow database. */
    virtual void deleteBorrowed(const QString &itemKey) = 0;
};

#endif
