/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-2006, Robert Fernie.
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

#ifndef TYPES_GUI_INTERFACE_H
#define TYPES_GUI_INTERFACE_H

#include <list>
#include <iostream>
#include <string>
#include <stdint.h>
#include <QStringList>
#include <QLinkedList>
#include <QHash>
#include <QDir>
#include <QDomNode>

/**********************************************************************************
 * uploadFileInfo and downloadFileInfo provide info about a single file being transferred.
 * Downloads are grouped into downloadGroupInfo which is the top-level struct the GUI
 * interacts with, while uploads are simply reported on filewise.
 **********************************************************************************/

/* Possible statuses */
/* For downloads, this is when the friend is offline.
   For uploads, this is whenever the transfer is not moving. */
const uint32_t FT_STATE_WAITING = 0x0001;
/* While the download or upload is transferring. */
const uint32_t FT_STATE_TRANSFERRING = 0x0002;
/* When the download is complete. */
const uint32_t FT_STATE_COMPLETE = 0x0003;
/* When the friend is online, but no download is occuring. */
const uint32_t FT_STATE_ONLINE_IDLE = 0x0004;

/* Information about a transfer with a single friend, to be aggregated in FileInfo */
struct TransferInfo {
    /* The friend this is referring to. */
    unsigned int librarymixer_id;
    /* Amount that has been transferred with this friend. */
    uint64_t transferred;
    /* Transfer rate in kilobytes / second. */
    double transferRate;
    /* One of the statuses from above. */
    uint32_t status;
};

struct uploadFileInfo {
    QString path;

    /* Details information for each of the friends that have requested this. */
    QList<TransferInfo> peers;
};

struct downloadFileInfo {
    QString hash;
    uint64_t totalSize; //In bytes

    uint64_t downloadedSize; //Amount received in bytes
    uint32_t downloadStatus;

    double totalTransferRate; //Transfer rate total of all peers in kb
    QList<TransferInfo> peers;
};

struct downloadGroupInfo {
    QString title;
    int groupId;
    QString finalDestination; //If this is empty, it means we are not done
    QList<downloadFileInfo> filesInGroup;
    QStringList filenames;
};

/**********************************************************************************
 * A pending request that has not yet been turned into a download.
 **********************************************************************************/
struct pendingRequest {
    pendingRequest() :status(REPLY_NONE) {}
    unsigned int friend_id; //librarymixer id of the user that is our source
    unsigned int item_id; //librarymixer item id
    QString name; //librarymixer name

    enum requestStatus {
        REPLY_NONE, //When no reply has been received yet
        REPLY_INTERNAL_ERROR, //When response has totally failed for unknown reasons
        REPLY_LENT_OUT, //Response was item is currently lent out
        REPLY_CHAT, //Response was open a chat window
        REPLY_MESSAGE, //Response was a message
        REPLY_NO_SUCH_ITEM, //Response was friend doesn't have such an item
        REPLY_UNMATCHED, //Response was no match has been set yet
        REPLY_BROKEN_MATCH //Response was this had an autoresponse but the file is missing
    };
    requestStatus status;

    uint32_t timeOfLastTry;
};

/**********************************************************************************
 * A suggestion to download files that is saved due to friend being offline.
 **********************************************************************************/
struct pendingSuggest {
    pendingSuggest(){};
    pendingSuggest(unsigned int friend_id, const QString &title, const QStringList &files, const QStringList &hashes, QList<qlonglong> filesizes, unsigned int uniqueSuggestionId)
        :friend_id(friend_id), title(title), files(files), hashes(hashes), filesizes(filesizes), uniqueSuggestionId(uniqueSuggestionId) {}

    /* Friend to send this to. */
    unsigned int friend_id;

    /* What this suggestion will be displayed as. */
    QString title;

    /* Information about the files to be suggested. */
    QStringList files;
    QStringList hashes;
    QList<qlonglong> filesizes;

    /* Used internally to identify suggestions uniquely. */
    unsigned int uniqueSuggestionId;
};

/**********************************************************************************
 * LibraryMixerItem: Collectively, the LibraryMixerItems are a wrapper to an underlying XML document that is the master database.
 * The management of the underlying XML is handled by the LibraryMixerLibraryManager.
 * Each LibraryMixerItem represents a single item in a user's LibraryMixer library,
 * which can potentially be matched to a file, a message, or opening a chat box.
 **********************************************************************************/

/* Possible states for a LibraryMixerItem.
   In this documentation the file states will refer to MATCHED_TO_FILE, MATCH_NOT_FOUND, MATCHED_TO_LEND, and MATCHED_TO_LENT.
   When the state is a file state, there will be additional state data stored about the related files.
   When the state is a message state, there will be additional state data stored about the message to be sent.
   When the state is MATCHED_TO_LENT, there will be additional state data stored about who the item was lent to. */

struct LibraryMixerItem {
    /* The name of this item on LibraryMixer. */
    QString title;
    /* The item ID of this item on LibraryMixer. */
    unsigned int item_id;

    /* The current state of this item. */
    enum ItemState {
        UNMATCHED = 0, //if it is unmatched
        MATCHED_TO_CHAT = 1, //open a chat window on request
        MATCHED_TO_FILE = 2, //automatically send a file on request
        MATCH_NOT_FOUND = 3, //previously matched to file, but now contains at least one broken link
        MATCHED_TO_MESSAGE = 4, //automatically send a message on request
        MATCHED_TO_LEND = 5, //automatically lend a file on request
        MATCHED_TO_LENT = 6 //matched to lend, but currently lent out
    };
    ItemState itemState;

    /* The fields below here are provisional, and will not always have real values depending on circumstances. */
    /* If this item has state MATCHED_TO_LENT, the friend who borrowed it. */
    unsigned int lentTo;
    /* If this item has state MATCHED_TO_MESSAGE, the message it responds with. */
    QString message;
    /* If this item has any of the file related states, these will be synchronized lists holding information about the files. */
    QStringList paths;
    QList<qlonglong> filesizes;
    QStringList hashes;
};


/**********************************************************************************
 * FriendLibraryMixerItem: Each LibraryMixerItem represents a single item in a user's
 * friends' LibraryMixer libraries.
 **********************************************************************************/

class FriendLibraryMixerItem {

public:
    FriendLibraryMixerItem(unsigned int item_id, unsigned int friend_id, const QString &title);

    /* General attribute accessors. The set methods will return true on success. */
    QString title() const;
    bool title(QString newTitle);

    unsigned int item_id() const;
    unsigned int friend_id() const;

private:
    unsigned int storedItemId;
    unsigned int storedFriendId;
    QString storedTitle;
};

/**********************************************************************************
 * OffLMShareItem: Collectively, the OffLMShareItems are a wrapper to an underlying XML document that is the master database.
 * The management of the underlying XML file is handled by the ftOffLMList.
 * They are structured in a tree, with a root-level item that is included for ease of navigation and consistency.
 * Below this are root shares, each of which represents either a file or a directory in off-LibraryMixer sharing.
 * Furthermore, root shares that represent directories can contain a number of items representing additional files or directories.
 * Accessors will modify the underlying XML, but will not save the modified XML to disk.
 * Passed to the GUI for display.
 **********************************************************************************/

/* XML format example, with:
(1) a file that has been lent to friend 37
(2) a folder containing an unhashed file and an empty folder
(3) a share that is missing
<offLM>
    <share>
        <label>example label</label>
        <rootPath>D:</rootPath>
        <method>1</method>
        <lent>37</lent>
        <updated>123456789</updated>
        <shareItem>
            <path>example.txt</path>
            <size>1024</size>
            <modified>123456789</modified>
            <hash>1a2b3c</hash>
        </shareItem>
    </share>
    <share>
        <label>example label 2</label>
        <rootPath>D:</rootPath>
        <method>0</method>
        <updated>987654321</updated>
        <shareItem>
            <path>sample</path>
            <subitems>
                <shareItem>
                    <path>unhashedFile</path>
                </shareItem>
                <shareItem>
                   <path>testFolder</path>
                   <subitems/>
                </shareItem>
            <subitems>
        </shareItem>
    </share>
    <share>
        <label>example label for missing share</label>
        <rootPath>//disconnectedNetworkServer</rootPath>
        <method>3</method>
        <updated>123456789</updated>
        <shareItem>
            <path>networkShare</path>
            <subitems>
                <shareItem>
                    <path>example.txt</path>
                    <size>1</size>
                    <modified>123456789</modified>
                    <hash>ba83</hash>
                </shareItem>
            <subitems>
        </shareItem>
    </share>
</offLM>

In addition, in practice the friend's XML files have two additional pieces of meta-data stored in their filename:
1. The friend's LibraryMixer ID,
2. The hash of that XML file.
Since the OffLMShareItem class does not deal with the files on disk directly, these must be set manually after the item is constructed.
*/

class OffLMShareItem: public QObject {
    Q_OBJECT

public:
    /* Used to indicate the sharing method set for a share.
       The missing states are used for when a share itself's path is no longer found, for example if it is a network path and network is disconnected.
       Contained files are generally cleared out when they go missing, but if the whole share is missing, we keep it around with files intact.
       This way if it comes back we still have the info, and we leave it up to the user to remove whole shares that have been invalidated. */
    enum shareMethodState {
        STATE_TO_SEND = 0,
        STATE_TO_LEND = 1,
        STATE_TO_SEND_MISSING = 2,
        STATE_TO_LEND_MISSING = 3
    };

    /* Our implementation of OffLMShareItem is actually as a wrapper to a copy of the DomNode from the XML.
       Row is the order in which the item comes relative to its siblings, starting from 0. */
    OffLMShareItem(QDomElement &domElement, int orderNumber, OffLMShareItem* parent = NULL);
    ~OffLMShareItem();

    /* Rechecks all paths associated with this item, as well as all contained subitems.
       Returns true if there are no changes, or if there are changes, updates the XML, and returns false.
       Adds all to ftFileWatcher as watched (checkPathsOrUpdate can be called multiple times without worrying about duplicative watching). */
    bool checkPathsOrUpdate();
    /* True if this is the root element of an XML, i.e. the one named offLM. */
    bool isRootItem() const;
    /* True if this represents a share, i.e. an item directly attached to the root item that can contain a file or folder. */
    bool isShare() const;
    /* True if this represents represents a file (can be as a leaf or as a share that is a file). */
    bool isFile() const;
    /* True if this represents represents a folder (can be as a nested folder or as a share that is a folder). */
    bool isFolder() const;
    /* Returns the number of children this item has. */
    int childCount() const;
    /* Returns the specified contained OffLMShareItem, or NULL if it doesn't exist. */
    OffLMShareItem* child(int order);
    /* Returns all children this item has. */
    QList<OffLMShareItem*> children();
    /* Removes the child indicated by order, returns true on success. */
    bool removeChild(int order);
    /* Returns the OffLMShareItem that contains this (either the root or a folder), or NULL if this is root level. */
    OffLMShareItem* parent();
    /* Accessors for the order number of this among its siblings. */
    int order() const;
    void order(int newOrder);
    /* Returns the path stored in this item.
       dirSeperator will be returned in OS-native format. */
    QString path() const;
    /* Returns the full path to this item, i.e. the path of all items in the item's ancestry + the path stored in this item.
       dirSeperator will be returned in OS-native format.
       Also handles QT fact that the rootPath can sometimes end in an extra dirSeperator at the base of drives. */
    QString fullPath() const;
    /* Accessors for the wrapped QDomElement. */
    QDomElement& domElement();
    void domElement(QDomElement &domElement);
    /* Accessors for whether the item is for copy or loan.
       The setter method may only be called for root shares. */
    shareMethodState shareMethod();
    bool shareMethod(shareMethodState newState);

    /* Root item functions. */
    /* Adds a new item to the XML of base root paths shared off-LibraryMixer.
       The supplied path should be in QT format with respect to directory separators. */
    void addShare(QString path);

    /* Share functions. Return empty results if not a root share. */
    /* Accessor for the label of this root share. */
    QString label() const;
    void label(QString newLabel);
    /* Accessor for the time_t of when this share was updated or created. */
    unsigned int updated() const;
    /* Accessors for who the share has currently been lent out to, or 0 if not lent out.
       When lent is set to a non-0 friend_id, its files will be deleted from disk. */
    unsigned int lent() const;
    void lent(unsigned int friend_id);

    /* File functions. Return empty results if not a file. */
    /* Accessor for the hash of the file. If not yet available, will return an empty string. */
    QString hash() const;
    /* Accessor for size in bytes of the file. If not yet available, will return 0. */
    qlonglong size() const;
    /* Accessor for the time_t of the last modified date of the file. If not yet available, will return 0. */
    unsigned int modified() const;
    /* Sets the file info for the file. Can accept either native or QT directory separators. */
    void setFileInfo(qlonglong newSize, unsigned int newModified, const QString &newHash);

    /* Own functions, that can only be used for items that are a user's own (as opposed to a friend's). */
    /* Sanitizes the item (and its underlying XML) in preparation for sending to friends by removing:
       -rootPath
       -if the item was lent out, will replace the friend id of the borrower with 1
       -if the item is currently missing, removes its contents
       Returns true on success.
       Furthermore, it will return false on items that represent files that don't have hash information yet. */
    void makeShareableWithFriends();
    /* Returns true if the item and all contained subitems have hash info set. */
    bool allFilesHashed();

    /* Friend functions, that can only be used for items that are friend's (as opposed to a user's own). */
    /* Accessors for the LibraryMixer ID of the friend this is associated with.
       Must be manuallly set externally for the root item, and all appended children will automatically be sets. */
    int friendId() const;
    void friendId(int newFriendId);
    /* Populates the information on either the current file, or if this is a folder, on all contained files.
       The lists are synchronized, in that element 1 of each refers to a given file, element 2 another, etc. */
    void getRecursiveFileInfo(QStringList &paths, QStringList &hashes, QList<qlonglong> &filesizes);

signals:
    void fileNoLongerAvailable(QString hash, qulonglong size);
    /* Emitted when a child is to be appended to item (note this is the item and not the child). */
    void itemAboutToBeAdded(OffLMShareItem* item);
    void itemAdded();
    /* Emitted when item has changed in a way potentially relevant to GUI. */
    void itemChanged(OffLMShareItem* item);
    /* Emitted when item is to be removed. */
    void itemAboutToBeRemoved(OffLMShareItem* item);
    void itemRemoved();

private:
    /* Takes a path, and rootPath from which it is relative, and returns the QDomElement containing the xml representing the offLMShareItem.
       This returned element will represent only that single path, and will not include any subitems. */
    QDomElement createOffLMShareItemXml(QDomDocument &xml, const QString &rootPath, const QString &path);

    /* Calls checkPathsOrUpdate on each of the contained subitems, returns true if unchanged, false if there were any changes on any. */
    bool checkSubitemsOrUpdate();

    /* May only be called upon the root item. All labels must be unique, so steps through and ensures proposed label is unique.
       If it is not, then it returns a new label with a number appended to the end that is unique. */
    QString findUniqueLabel(const QString &proposedLabel);

    /* Called whenever a file is modified, it causes the share's updated field to be updated as well. */
    void shareUpdated(unsigned int newModified);

    /* Steps through starting from item, and deletes all files (but not folders) starting from item. */
    void recursiveDeleteFromDisk();

    /* Returns the element to which subitems are appended for this element.
       For the rootItem, this is the linkedDomElement,
       for shares this is linkedDomElement=>shareItem=>subitems, and
       for non-share folders this is linkedDomElement=>subitems. */
    QDomElement subitemHoldingElement() const;

    QDomElement linkedDomElement; //The QDomElement that contains all of the substantive contents
    int orderNumber; //Its order number among its siblings
    OffLMShareItem* parentItem; //The OffLMShareItem containing this item, or NULL if root
    QHash<int, OffLMShareItem*> childItems; //Cache of contained OffLMShareItems, hashed by order

    /* Tagged onto friend's root nodes so we can keep track of who the node belongs to. */
    int friend_id;
};

/**********************************************************************************
 * TempShareItem: Represents a single temporary share.
 * Once an item is fullyHashed(), as in all files within it are hashed, it will be sent out
 * to the listed friend. At this point, the item is frozen, and should never be updated
 * again because there is no way to make the updated file info available to the friend.
 * Instead, if we find out any files have been updated, we should remove that file from
 * the item.
 **********************************************************************************/

/* XML format example
<tempitems>
 <item>
  <label>Example Temp Item</label>
  <files>
   <file>
    <path>D:/example.txt</path>
    <hash>c6f6207a1a7685f37e089565ac8cf4a0</hash>
    <size>60</size>
    <modified>39872983423</modified>
   </file>
   <file>
    <path>D:/NotYetHashedFile.txt</path>
   </file>
  </files>
  <expiration>32094235</expiration>
  <friend>42</friend>
  <borrowKey></borrowKey>
 </item>
</tempitems>
*/

class TempShareItem: public QObject {
    Q_OBJECT

public:
    /* Creates a TempShareItem wrapper around an existing domNode that it represents. */
    TempShareItem(QDomNode &domNode);
    /* Creates a TempShareItem wrapped while simultaneously creating a new domNode, and appending it to the root element. */
    TempShareItem(QDomDocument &xml, const QString &label, const QStringList &paths, unsigned int authorized_friend_id);
    /* Removes the wrapped domNode from the root element. */
    ~TempShareItem();

    QString label() const;

    /* The files this TempShareItem is sharing.
       These lists are synchronized, in that element X of each refers to the same file.
       Will include an empty string or 0 in the lists for files that haven't had their file data set yet.
       Paths are returned with native directory separators. */
    QStringList paths() const;
    QList<qlonglong> filesizes() const;
    QStringList hashes() const;
    QList<unsigned int> modified() const;

    /* Returns the number of files that are contained in this item. */
    int fileCount() const;

    /* Removes the file contained in this item specified by fileIndex, and emits fileNoLongerAvailable for it. */
    void removeFile(int fileIndex);

    /* Sets the file info for the file identified by path. Can accept either native or QT directory separators.
       Returns false is no such path exists. */
    bool setFileInfo(const QString &path, qlonglong size, unsigned int modified, const QString &hash);

    /* Returns true if this item's all have a hash inserted. */
    bool fullyHashed() const;

    /* Accessors for when this item expires. 0 when expiration is disabled. */
    int expiration() const;
    void expirationSetToTwoWeeks();
    void expirationDisable();

    /* Return the friend id of the friend this temporary item is shared with. */
    unsigned int friend_id() const;

    /* Accessors for the borrowKey, which is only set on temporary items that represent the return of a borrowed item.
       The borrowKey is the key that is used by the borrow database to identify a given borrowed item. */
    QString borrowKey() const;
    void borrowKey(const QString &newKey);

signals:
    void fileNoLongerAvailable(QString hash, qulonglong size);

private:
    QDomNode linkedDomNode; //The QDomNode that contains all of the substantive contents
};

/**********************************************************************************
 * Used to keep track of both own and friend net states
 **********************************************************************************/

struct NetConfig {
    std::string localAddr;
    int localPort;
    std::string extAddr;
    int extPort;
    std::string extName;

    /* older data types */
    bool DHTActive;
    bool uPnPActive;

    /* Flags for Network Status */
    bool netOk;     /* That we've talked to someone! */
    bool netUpnpOk; /* upnp is enabled and active */
    bool netDhtOk;  /* response from dht */
    bool netExtOk;  /* know our external address */
    bool netUdpOk;  /* recvd stun / udp packets */
    bool netTcpOk;  /* recvd incoming tcp */
    bool netResetReq;
};

#endif
