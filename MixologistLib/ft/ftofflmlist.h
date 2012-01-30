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

#ifndef FT_OFF_LM_LIST_HEADER
#define FT_OFF_LM_LIST_HEADER

#include "ft/ftfilemethod.h"
#include "pqi/pqimonitor.h"
#include "interface/files.h"
#include "interface/types.h"
#include <QDomDocument>
#include <QMap>
#include <QMutex>
#include <QTimer>

class ftTransferModule;
class ftOffLMList;
/* This extern is used by statusService to pass information about the Xmls. */
extern ftOffLMList *offLMList;

/**********************************************************************************
 * This class enables sharing of files separate from the LibraryMixer share lists.
 * It maintains the master xml of root paths which are to be shared.
 * It also maintains the OffLMShareItem tree, which is a wrapper to the master xml.
 * The structure of the master xml and wrapper is similar to that used in the
 * QT Simple DOM Model example.
 *
 * ftOffLMList's tick method is called from ftController so it can handle downloading
 * of friend's Xml files (note that files downloaded via the ftOffLMList are handled the
 * standard way, by ftController and not here). This is done because the download of
 * the XMLs is a rather different process than the download of ordinary files, so it
 * makes sense to handle is separately.
 **********************************************************************************/

class OffLMHelper;

class ftOffLMList: public ftFileMethod {
    Q_OBJECT

public:
    ftOffLMList();

    /* Called from ftController, so that ftOffLMList can handle downloading of its Xmls. */
    void tick();

    /* Returns the OffLMShareItem identified by that label, or NULL if not found. */
    OffLMShareItem* getOffLMShareItem(const QString &label);

    /* Fills in the information on the user's own off-LibraryMixer share Xml.
       This is used to send information to friends about the current state of the Xml. */
    void getOwnOffLMXmlInfo(QString *hash, qlonglong *size) const;

    /* Called from StatusService when it receives information on the latest Xml info from a friend. */
    void receiveFriendOffLMXmlInfo(unsigned int friend_id, const QString &hash, qlonglong size);

    /* Called from ftDataDemultiplex through ftController when we receive new data to pass it to the appropriate transferModule. */
    bool handleReceiveData(const std::string &peerId, const QString &hash, uint64_t offset, uint32_t chunksize, void *data);

    /* Sets the given item that is currently set to lend to lent to friend with friend_id
       and deletes all files that it matches. */
    void setLent(unsigned int friend_id, const QString &label);

    /* Called on the return of borrowed items to return the items to their original locations.
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

    /**********************************************************************************
     * Body of public-facing API functions called through ftServer
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

    /* Returns the root item of the OffLMShareItem tree. */
    OffLMShareItem* getOwnOffLMRoot() const;

    /* Queues an asynchronous recheck of all files in ftOffLMList. */
    void recheckFiles();

    /* Returns the root item of a friend's OffLMShareItem tree, as determined by index, or NULL if not present.
       The friends are stored in an arbitrary order, so there is no way to know which friend will be returned by index.
       However, the items are guaranteed to be stored in a consistent order, so by stepping through index until
       a NULL is returned, all friends with OffLMShares are guaranteed to be returned once and only once. */
    OffLMShareItem* getFriendOffLMShares(int index) const;

    /* Returns the number of friends we have off LibraryMixer share information for. */
    int getOffLMShareFriendCount() const;

    /* Called by the ftController (passing from p3ConnectMgr) to inform about changes in friends' online statuses. */
    void statusChange(const std::list<pqipeer> &changeList);

    /**********************************************************************************
     * Implementations for ftFileMethod
     **********************************************************************************/

    /* Returns true if it is able to find a file with matching hash and size.
       If a file is found, populates path. */
    virtual ftFileMethod::searchResult search(const QString &hash, qlonglong size, uint32_t hintflags, QString &path);

signals:
    /* Used by the GUI. */
    void offLMFriendAboutToBeAdded(int row);
    void offLMFriendAdded();
    void offLMFriendAboutToBeRemoved(int row);
    void offLMFriendRemoved();
    void offLMOwnItemAboutToBeAdded(OffLMShareItem* item);
    void offLMOwnItemAdded();
    void offLMOwnItemChanged(OffLMShareItem* item);
    void offLMOwnItemAboutToBeRemoved(OffLMShareItem* item);
    void offLMOwnItemRemoved();

    /* Used internally to connect to OffLMHelper. */
    void offLMOwnXmlUpdated();
    void updateOwnFileHash(QString path, qlonglong size, unsigned int modified, QString newHash);
    void recheckFilesRequested();

private slots:
    /* Connected to ftFileWatcher for when a file has been hashed in order to update with the new information.
       The path should be supplied with native directory separators. */
    void newFileHash(QString path, qlonglong size, unsigned int modified, QString newHash);

private:
    /* Reads in a friend's Xml file from its canonical location (as generated by friendXmlFile()) and adds it to friendXmls and friendRoots. */
    bool readFriendOffLMShares(unsigned int friend_id, QString xmlHash);

    /* Takes a path to the location of a newly downloaded Xml file, and removes any existing information on that friend by internally calling removeFriendOffLMShares, and sets the new information.
       Will remove the existing Xml file, if any, and move the new one into place.
       Finishes by calling readFriendOffLMShares with the supplied informmation, including the xmlHash. */
    bool addFriendOffLMShares(unsigned int friend_id, QString path, QString xmlHash);

    /* Completely removes a friend from ftOffLMList. Removes from:
       files on disk
       friendRoots
       friends
       Note that it does not remove from friendsXmlDownloads for in progress downloads. */
    void removeFriendOffLMShares(unsigned int friend_id);

    /* Used by search to recurse down through the tree. */
    ftFileMethod::searchResult recursiveSearch(const QString &hash, qlonglong size, QString &path, OffLMShareItem* current) const;

    /* Convenience method to return the path to user's own Xml file with native directory separators. */
    QString ownXmlFile() const;

    /* Convenience method to return the path to user's own sanitized Xml file with native directory separators. */
    QString ownSanitizedXmlFile() const;

    /* Convenience method to return the path to a friend's Xml file with native directory separators. */
    QString friendXmlFile(unsigned int friend_id, QString xmlHash) const;

    /* Removes the friend from friendsXmlDownloads, frees the transferModule, and removes any partial downloads if they exist */
    void removeFriendDownload(unsigned int friend_id);

    OffLMHelper* offLMHelper;
    friend class OffLMHelper;

    /* The root of the user's own off-LibraryMixer share cache. */
    OffLMShareItem* rootItem;

    /* There are 4 threads that will call public functions for this class:
       (1) The main GUI thread, performing all sorts of queries and changes on both own and friend item lists.
       (2) The ftFileWatcher thread, returning updated hashes for own items.
       (3) The main server thread when syncing lists with friends, and when updating lent status of own items.
       (4) The captive OffLMHelper thread, when updating own items.
       The mutex protects only the variables below it.
        */
    mutable QMutex offLmMutex;

    /* Represents the user's own share list.
       ownXml must be stored in order for the Xml to stay valid.
       If we didn't store it here, then even if we called rootItem->domNode().ownerDocument(), it only returns an invalid blank document.
       This is saved to disk when changes are made. */
    QDomDocument ownXml;

    /* This meta data is used by the StatusService to keep friends up to date. */
    QString sanitizedXmlHash;
    qlonglong sanitizedXmlSize;

    /* The various roots of the friends' off-LibraryMixer shares.
       We maintain a separate list friends as well that represents the order to display them. */
    QList<int> friends;
    QHash<int, OffLMShareItem*> friendRoots;

    /* This is a list of current downloads of friends Xmls, mapped by the friend_id */
    QMap<unsigned int, ftTransferModule*> friendsXmlDownloads;
};

/* The body of the internal thread that handles the really long-running tasks of ftOffLM such as adding new shares and rechecking existing shares.
   By having it in its own thread, we can have other threads (particularly the GUI) that may call addShare and other methods return immediately. */
class OffLMHelper: public QObject {
    Q_OBJECT

public:
    OffLMHelper(ftOffLMList* list);

    /* Adds a share to the queue to be handled. */
    void addShare(QString path);

signals:
    /* Emitted whenever a request to add a share is added, so that the OffLMHelper thread can wake up and handle a job. */
    void addShareRequested();

public slots:
    /* Sets a timer to save the XML when the timer runs out. If called again before timer runs out, will reset the timer.
       This mechanism prevents the XML from being updated on disk too often when changes are coming in fast.
       Must not be called directly from outside of the OffLMHelper, but instead must be invoked via a queued connection.
       This is because the timer's thread affinity will belong to the calling thread, and the queued connection ensures the calling thread is OffLMHelper's thread. */
    void requestSaveOwnXml();

private slots:
    /* Connected to updateOwnFileHash in ftOffLMList in order to handle updating of hashes of files that are being shared. */
    void updateOwnFileHash(QString path, qlonglong size, unsigned int modified, QString newHash);

    /* Connected to ftFileWatcher for when a file's old hash is no longer valid.
       The path should be supplied with native directory separators. */
    void oldHashInvalidated(QString path, qlonglong size, unsigned int modified);

    /* Connected to ftFileWatcher for when a file has been removed.
       The path should be supplied with native directory separators. */
    void fileRemoved(QString path);

    /* Connected to ftFileWatcher for when a directory's contents are changed, whether by file rename, or addition or removal of file. */
    void directoryChanged(QString path);

    /* Connected to recheckFilesRequested in ftOffLMList so that the GUI can request a recheck of files. */
    void recheckFiles();

    /* Connected to own signal addShareRequested, so that the thread event loop is activated to process the request once for each that is added. */
    void processAddShare();

    /* Connectedo to saveTimer's timeout signal, handles the actual saving of the XML.
       Also creates the sanitized XML and calls for a hash of the new XML. */
    void processSaveOwnXml();

private:
    /* In addition to the user's own XML, the user also has a sanitized XML that is suitable for sending to friends.
       It is sanitized by removing full path information.
       This generates and saves that sanitized XML. */
    bool saveOwnSanitizedXml();

    /* Returns the root share with a path equal to path (whether supplied with native separators or without) if present, otherwise returns NULL. */
    OffLMShareItem* findOffLMShare(const QString &path);

    /* Returns the share items contained anywhere in the DOM tree under startingItem with a path equal to path (whether supplied with native separators or without).
       Will NOT search items that are currently lent out. */
    QList<OffLMShareItem*> findOffLMShareItems(const QString &path, OffLMShareItem* startingItem);

    mutable QMutex offLMHelperMutex;

    /* The queue of paths to add, with additions added via addShare, and removed via processAddShare. */
    QStringList pathsToAdd;

    /* Timer used to ensure that the xml is not updated too often. */
    QTimer* saveTimer;

    ftOffLMList* list;
};

#endif
