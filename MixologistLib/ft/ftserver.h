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

#ifndef FT_SERVER_HEADER
#define FT_SERVER_HEADER

/*
 * ftServer.
 *
 * Top level File Transfer interface.
 *
 * Implements Files interface for control from the GUI.
 *
 * Also responsible for the operation of all of the non-file transfer related services,
 * making the name a bit of a misnomer.
 *
 */

#include "ft/ftdata.h"
#include "interface/files.h"
#include "pqi/pqi.h"

#include <QString>

class MixologyService;
class LibraryMixerItem;

class AuthMgr;

class NotifyBase; /* needed by FiStore */

class ftController;
class ftTempList;
class LibraryMixerLibraryManager;
class ftOffLMList;
class ftDataDemultiplex;

class ftServer;
extern ftServer *ftserver;

class ftServer: public Files, public ftDataSend {

public:    

    /**********************************************************************************
     * Setup
     **********************************************************************************/

    ftServer();

    /* Assign important variables */
    void setP3Interface(P3Interface *pqi);

    /* Sets up links to the mixology service. Cannot be done in the constructor because
       the mixology service is created after the ftserver in in it. */
    void setupMixologyService();

    /* Final Setup (once everything is assigned) */
    void SetupFtServer();

    void StartupThreads();

    void StopThreads();

    /**********************************************************************************
     * Control Interface (Implements Files)
     **********************************************************************************/

    /**********************************************************************************
     * Control of Downloads
     **********************************************************************************/

    /* Adds a request to request list for an item identified by item_id from friend with librarymixer_id
       labeling the request name. */
    virtual bool LibraryMixerRequest(unsigned int librarymixer_id, unsigned int item_id, QString name);

    /* Removes a request from the pending request list. */
    virtual bool LibraryMixerRequestCancel(unsigned int item_id);

    /* Sends a download invitation to a friend for the given item.
       If the item is not yet ready, then adds them to the waiting list. */
    virtual void MixologySuggest(unsigned int librarymixer_id, unsigned int item_id);

    /* Creates a new download of a batch of files. */
    virtual bool downloadFiles(unsigned int friend_id, const QString &title, const QStringList &paths,
                               const QStringList &hashes, const QList<qlonglong> &filesizes);

    /* Creates a new download to borrow a batch of files.
       source_type is one of the file_hint constants defined above to indicate the type of share we are borrowing from.
       source_id's meaning changes depending on the source_type, but always uniquely identifies the item. */
    virtual bool borrowFiles(unsigned int friend_id, const QString &title, const QStringList &paths,
                             const QStringList &hashes, const QList<qlonglong> &filesizes,
                             uint32_t source_type, QString source_id);

    /* Cancels an entire downloadGroup. */
    virtual void cancelDownloadGroup(int groupId);

    /* Cancels an existing file transfer */
    virtual void cancelFile(int groupId, const QString &hash);

    /* Pauses or resumes a transfer. */
    virtual bool controlFile(int orig_item_id, QString hash, uint32_t flags);

    /* Removes all completed transfers and all errored pending requests. */
    virtual void clearCompletedFiles();

    /* Removes all items from the uploads list. (This is a visual only change). */
    virtual void clearUploads();

    /**********************************************************************************
     * Download/Upload Details
     **********************************************************************************/

    virtual void getPendingRequests(std::list<pendingRequest> &requests);
    virtual void FileDownloads(QList<downloadGroupInfo> &downloads);
    virtual void FileUploads(QList<uploadFileInfo> &uploads);

    /**********************************************************************************
     * Directory Handling
     **********************************************************************************/

    /* Set the directories as specified, saving changes to settings file. */
    virtual void setDownloadDirectory(QString path);
    virtual void setPartialsDirectory(QString path);

    /* Return the directories currently being used */
    virtual QString getDownloadDirectory();
    virtual QString getPartialsDirectory();

    /**********************************************************************************
     * LibraryMixer Item Control
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

    /**********************************************************************************
     * Friend LibraryMixer Item Control
     **********************************************************************************/

    /* Returns a map of all FriendLibraryMixerItems. The map is by LibraryMixer item IDs. */
    QMap<unsigned int, FriendLibraryMixerItem*>* getFriendLibrary();

    /**********************************************************************************
     * Temporary Share Control
     **********************************************************************************/

    /* Creates a new temporary item.
       A download suggestion will be sent to recipient as soon as hashing is complete. */
    void sendTemporary(QString title, QStringList paths, unsigned int friend_id);

    /* Creates a new temporary item to return a batch of files, with a return suggestion sent as soon as hashing is complete.
       itemKey is the identifier of the borrowed item that is used by the borrow database. */
    void returnFiles(const QString &title, const QStringList &paths, unsigned int friend_id, const QString &itemKey);

    /**********************************************************************************
     * Saved Suggestions Control
     **********************************************************************************/
    /* Sets a list of all pendingSuggests into suggestions. */
    void getPendingSuggestions(QList<pendingSuggest> &suggestions);

    /* Removes a saved pendingSuggest. */
    void removeSavedSuggestion(unsigned int uniqueSuggestionId);

    /**********************************************************************************
     * Off LibraryMixer Sharing Control
     **********************************************************************************/
    /* Adds a new share to the list of base root paths shared off-LibraryMixer.
       The supplied path should be in QT format with respect to directory separators. */
    void addOffLMShare(QString path);

    /* Removes a share identified by its root OffLMShareItem from the list of base root paths shared off-LibraryMixer.
       Does nothing if given a non-root item.
       Returns true on success, false if toRemove was not removed. */
    bool removeOffLMShare(OffLMShareItem* toRemove);

    /* Sets the shareMethod to the specified value. Returns true on success. */
    bool setOffLMShareMethod(OffLMShareItem* toModify, OffLMShareItem::shareMethodState state);

    /* Sets the label to the specified value. Returns true on success. */
    void setOffLMShareLabel(OffLMShareItem* toModify, QString newLabel);

    /* Returns the root item of the OffLMShareItem tree */
    OffLMShareItem* getOwnOffLMRoot() const;

    /* Queues an asynchronous recheck of all files in ftOffLMList. */
    void recheckOffLMFiles() const;

    /* Reads all existing saved friend off-LibraryMixer shares from disk, keyed by their ID.
       The caller will be responsible for calling delete on each OffLMShareItem it no longer needs. */
    void readExistingFriendsOffLMShares(QHash<unsigned int, OffLMShareItem*> &friendRoots) const;

    /* Reads a specific friend's off-LibraryMixer shares from disk.
       Useful for after a signal informs a listener that a new friend has been added. */
    OffLMShareItem* readFriendOffLMShares(unsigned int friend_id) const;

    /**********************************************************************************
     * Borrowing Management
     **********************************************************************************/

    /* Gets all items borrowed, and populates them into parallel lists for titles, itemKeys that identify them, and the friends they are borrowed from. */
    void getBorrowings(QStringList &titles, QStringList &itemKeys, QList<unsigned int> &friendIds);

    /* Get items borrowed from a specific friend. */
    void getBorrowings(QStringList &titles, QStringList &itemKeys, unsigned int friendId);

    /* Called when a user wants to remove something from the borrow database. */
    void deleteBorrowed(const QString &itemKey);

    /**********************************************************************************
     * Data Transfer Interface (Implements ftDataSend)
     **********************************************************************************/

    /* Client Send */
    virtual bool sendDataRequest(unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize);

    /* Server Send */
    virtual bool sendData(unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t baseOffset, uint32_t chunkSize, void *data);

    /* This tick is called from the main server */
    virtual int tick();

    /* Configuration */
    bool ResumeTransfers();

private:
    bool handleFileData();

    P3Interface *persongrp;

    ftDataDemultiplex *mFtDataplex;
};

#endif
