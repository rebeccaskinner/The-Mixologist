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
 * Implements Files interface for external control.
 *
 */

#include <map>
#include <list>
#include <iostream>
#include <QString>

#include "ft/ftdata.h"
#include "interface/files.h"
#include "util/threads.h"
#include "pqi/pqi.h"

class MixologyService;
class LibraryMixerItem;

class p3ConnectMgr;
class AuthMgr;

class NotifyBase; /* needed by FiStore */

class ftController;
class ftItemList;
class ftFileSearch;
class MixologyBorrower;

class ftDataMultiplex;

class ftServer;
extern ftServer *ftserver;

class ftServer: public Files, public ftDataSend {

public:

    /***************************************************************/
    /******************** Setup ************************************/
    /***************************************************************/

    ftServer(AuthMgr *authMgr, p3ConnectMgr *connMgr);

    /* Assign important variables */
    void    setP3Interface(P3Interface *pqi);

    void    setMixologyService(MixologyService *_mixologyservice) {
        mixologyservice = _mixologyservice;
    }

    std::string     OwnId();

    /* Final Setup (once everything is assigned) */
    void    SetupFtServer();

    void    StartupThreads();

    /***************************************************************/
    /*************** Control Interface *****************************/
    /************** (Implements Files) ***************************/
    /***************************************************************/

    /***
     * Control of Downloads
     ***/
    virtual bool LibraryMixerRequest(int librarymixer_id,int item_id, QString name);
    virtual bool LibraryMixerRequestCancel(int item_id);
    virtual void MixologySuggest(unsigned int librarymixer_id, int item_id);
    virtual void LibraryMixerBorrowed(unsigned int librarymixer_id, int item_id);
    virtual void LibraryMixerBorrowReturned(unsigned int librarymixer_id, int item_id);
    virtual bool requestFile(QString librarymixer_name, int orig_item_id, QString fname, std::string hash, uint64_t size,
                             uint32_t flags, QList<int> sourceIds);
    virtual bool cancelFile(int orig_item_id, QString filename, std::string hash);
    virtual bool controlFile(int orig_item_id, std::string hash, uint32_t flags);
    //Removes all completed transfers and all errored pending requests.
    virtual void clearCompletedFiles();
    //Removes all items from the uploads list. (This is a visual only change)
    virtual void clearUploads();

    /***
     * Download/Upload Details
     ***/
    virtual void getPendingRequests(std::list<pendingRequest> &requests);
    virtual bool FileDownloads(QList<FileInfo> &downloads);
    virtual bool FileUploads(QList<FileInfo> &uploads);

    /***
     * Library Management
     ***/
    //Finds the LibraryMixerItem that corresponds to the item id, and returns it.
    //Returns a blank LibraryMixerItem on failure.
    LibraryMixerItem getItem(int id);
    //Finds the LibraryMixerItem that contains the same paths, and returns it.
    //Returns a blank LibraryMixerItem on failure.
    LibraryMixerItem getItem(QStringList paths);
    //Finds the LibraryMixerItem that corresponds to id in either mToHash or mFiles and returns its status.  Returns -1 on not found.
    int getItemStatus(int id);

    /*Finds the unmatched item with item_id, and sets it to ITEM_MATCHED_TO_FILE with paths.
      Arranges for an invitation to be sent to recipient as soon as hashing is complete.
      Returns true on success.
      Returns false if the match is already matched, or item_id is not in the database.*/
    bool matchAndSend(int item_id, QStringList paths, int recipient);
    /*Creates a new temporary LibraryMixerItem with a negative id to hold the item that is not in the library on the website.
      Arranges for an invitation to be sent to recipient as soon as hashing is complete.*/
    void sendTemporary(QString title, QStringList paths, int recipient);

    /***
     * Borrowing Management
     **/

    /*Called by every completed download of a librarymixer transfer. Someday it may do more, but for now
      just checks the borrowing database if action needs to be done.
      Marks an item in the borrow database that was downloading to done downloaded and now borrowed.
      createdDirectory is the path of the directory created to put the files in, or blank is there wasn't one created. */
    void completedDownload(int item_id, QStringList paths, QStringList hashes, QList<unsigned long> filesizes, QString createdDirectory);

    /* Gets all borrowings associated with that librarymixer_id, and populates them into parallel lists for
       titles and item_ids.
       Returns false if there are none, or true if there is at least one.*/
    bool getBorrowings(int librarymixer_id, QStringList &titles, QList<int> &item_ids);
    //Fills out parallel lists on all borrowings
    void getBorrowingInfo(QList<int> &item_ids, QList<borrowStatuses> &statuses, QStringList &names);

    //Called upon receiving an offer to borrow a set of files so we can wait for user response on whether to proceed.
    void addPendingBorrow(int item_id, QString librarymixer_name, int librarymixer_id, QStringList filenames, QStringList hashes, QStringList filesizes);
    //Called when a user declines to borrow an item or wants to remove something from the borrow database.
    void cancelBorrow(int item_id);
    //Causes a pending BorrowItem to start downloading.
    void borrowPending(int item_id);
    /*Marks a borrowed item as now in the process of being returned.
      Addsd it to the files database as a temporary item and sends a download invitation to the owner.*/
    void returnBorrowed(int librarymixer_id, int item_id, QString title, QStringList paths);
    //Removes an item from the borrow database that was in the process of being returned.
    void returnedBorrowed(int item_id);

    /***
         * Item List Access
     ***/
    void setItems();
    //Adds the item identified by the provided information to the list of items to be hashed.
    void addItem(LibraryMixerItem item);
    //Removes the item identified by the provided information from both mFiles and mToHash.  Does nothing if files does not already exist.
    void removeItem(int id);
    //Deletes the files and removes the item identified by the provided information from mFiles only.
    void deleteRemoveItem(int id);
    //Blocking function that immediately rechecks the identified item.
    //If unable to find the item, changed is false and returns NULL.
    //If the filesize has changed, changed is true, file is rehashed, changes are saved, and returns the modified LibraryMixerItem with itemState ITEM_MATCHED_TO_FILE.
    //If the filesize has not changed, changes is false, and returns the LibraryMixerItem with itemState ITEM_MATCHED_TO_FILE.
    //If the file could not be found, changed is true, hash is set to "" and filesize 0, and returns modified LibraryMixerItem with itemState ITEM_MATCH_NOT_FOUND.
    LibraryMixerItem *recheckItem(int id, bool *changed);

    /***
     * Directory Handling
     ***/
    virtual void    setDownloadDirectory(QString path, bool trySavedSettings = false);
    virtual void    setPartialsDirectory(QString path, bool trySavedSettings = false);
    virtual QString getDownloadDirectory();
    virtual QString getPartialsDirectory();

    /***************************************************************/
    /*************** Control Interface *****************************/
    /***************************************************************/

    /***************************************************************/
    /*************** Data Transfer Interface ***********************/
    /***************************************************************/
public:
    virtual bool    sendData(std::string peerId, std::string hash, uint64_t size,
                             uint64_t offset, uint32_t chunksize, void *data);
    virtual bool    sendDataRequest(std::string peerId,
                                    std::string hash, uint64_t size,
                                    uint64_t offset, uint32_t chunksize);
    void sendMixologySuggestion(unsigned int librarymixer_id, int item_id, QString name);

    /*************** Internal Transfer Fns *************************/
    virtual int tick();

    /* Configuration */
    bool    ResumeTransfers();

private:
    bool    handleFileData();

private:

    /* no need for Mutex protection -
     * as each component is protected independently.
     */

    MixologyService *mixologyservice;

    P3Interface *mP3iface;     /* XXX THIS NEEDS PROTECTION */
    AuthMgr    *mAuthMgr;
    p3ConnectMgr *mConnMgr;

    ftController  *mFtController;
    ftItemList   *mFtItems;
    MixologyBorrower  *mixologyborrower;

    ftDataMultiplex *mFtDataplex;

    ftFileSearch   *mFtSearch;

    MixMutex srvMutex;
    QString mDownloadPath;
    QString mPartialsPath;

};



#endif
