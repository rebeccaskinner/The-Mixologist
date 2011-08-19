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

#include <list>
#include <iostream>
#include <string>
#include <QString>

#include "interface/types.h"
#include "interface/librarymixer-library.h"

class Files;
extern Files *files;

/* These are used mainly by ftController at the moment */
const uint32_t FILE_CTRL_PAUSE   = 0x00000100;
const uint32_t FILE_CTRL_START   = 0x00000200;

const uint32_t FILE_RATE_TRICKLE     = 0x00000001;
const uint32_t FILE_RATE_SLOW    = 0x00000002;
const uint32_t FILE_RATE_STANDARD    = 0x00000003;
const uint32_t FILE_RATE_FAST    = 0x00000004;
const uint32_t FILE_RATE_STREAM_AUDIO = 0x00000005;
const uint32_t FILE_RATE_STREAM_VIDEO = 0x00000006;

const uint32_t FILE_PEER_ONLINE      = 0x00001000;
const uint32_t FILE_PEER_OFFLINE     = 0x00002000;

/************************************
 * Used To indicate where to search.
 *
 * The Order of these is very important,
 * it specifies the search order too.
 *
 */

const uint32_t FILE_HINTS_ITEM   = 0x00000002;
const uint32_t FILE_HINTS_DOWNLOAD   = 0x00000010;
const uint32_t FILE_HINTS_UPLOAD     = 0x00000020;
/* Means to only search the specific hints, otherwise they're just hints on priority.  I think. */
const uint32_t FILE_HINTS_SPEC_ONLY  = 0x01000000;
//const uint32_t FILE_HINTS_BACKGROUND   = 0x00002000; // To download slowly.

enum borrowStatuses {
    BORROW_STATUS_NOT, //When something is not in the borrow database (this is not ever set as an actual status)
    BORROW_STATUS_PENDING, //When we have been informed by a MixologyResponse on what hashes to request to borrow something
    BORROW_STATUS_GETTING, //When we are in the process of downloading
    BORROW_STATUS_BORROWED //When we have something borrowed from a friend
};

/* The interface by which MixologistGui can control file transfers */
class  Files {
public:

    Files() {
        return;
    }
    virtual ~Files() {
        return;
    }

    /****************************************/
    /* download */


    /***
     *  Control of Transfers.
     ***/
    /*Adds a request to request list for an item identified by item_id from friend with librarymixer_id
      labeling the request name */
    virtual bool LibraryMixerRequest(int librarymixer_id, int item_id, QString name) = 0;
    /*Removes a request from the pending request list.*/
    virtual bool LibraryMixerRequestCancel(int item_id) = 0;
    /*Sends a download invitation to a friend for the given item.
      If the item is not yet ready, then adds them to the waiting list. */
    virtual void MixologySuggest(unsigned int librarymixer_id, int item_id) = 0;
    /*Downloads a file*/
    virtual bool requestFile(QString librarymixer_name, int orig_item_id, QString fname, std::string hash, uint64_t size,
                             uint32_t flags, QList<int> sourceIds) = 0;
    virtual bool cancelFile(int orig_item_id, QString filename, std::string hash) = 0;
    //Pauses or resumes a transfer
    virtual bool controlFile(int orig_item_id, std::string hash, uint32_t flags) = 0;
    //Removes all completed transfers and all errored pending requests.
    virtual void clearCompletedFiles() = 0;
    //Removes all items from the uploads list. (This is a visual only change)
    virtual void clearUploads() = 0;

    /***
     * Download / Upload Details.
     ***/
    virtual void getPendingRequests(std::list<pendingRequest> &requests) = 0;
    virtual bool FileDownloads(QList<FileInfo> &downloads) = 0;
    virtual bool FileUploads(QList<FileInfo> &uploads) = 0;

    /***
     * Directory Control
     ***/
    /* Set the directories as specified, saving changes to settings file. */
    virtual void setDownloadDirectory(QString path) = 0;
    virtual void setPartialsDirectory(QString path) = 0;
    /* Return the directories currently being used */
    virtual QString getDownloadDirectory() = 0;
    virtual QString getPartialsDirectory() = 0;

    /***
     * Item Database Control
     ***/
    /*Note that there are another set of similarly named functions in librarymixer-library
      Someday, librarymixer-library and the ftserver ftextrralist should be merged, but for now
      these functions search only items that have files associated.*/
    /*Finds the LibraryMixerItem that corresponds to the item id, and returns it.
      Returns a blank LibraryMixerItem on failure.*/
    virtual LibraryMixerItem getItem(int id) = 0;
    /*Finds the LibraryMixerItem that contains the same paths, and returns it.
      Returns a blank LibraryMixerItem on failure.*/
    virtual LibraryMixerItem getItem(QStringList paths) = 0;
    //Finds the LibraryMixerItem that corresponds to id in either mToHash or mFiles and returns its status.  Returns -1 on not found.
    virtual int getItemStatus(int id)  = 0;

    /*Finds the unmatched item with item_id, and sets it to ITEM_MATCHED_TO_FILE with paths.
      Arranges for an invitation to be sent to recipient as soon as hashing is complete.
      Returns true on success.
      Returns false if the match is already matched, or item_id is not in the database.*/
    virtual bool matchAndSend(int item_id, QStringList paths, int recipient) = 0;
    /*Creates a new temporary LibraryMixerItem with a negative id to hold the item that is not in the library on the website.
      Arranges for an invitation to be sent to recipient as soon as hashing is complete.*/
    virtual void sendTemporary(QString title, QStringList paths, int recipient) = 0;

    /***
     * Borrowing Management
     **/

    /* Gets all borrowings associated with that librarymixer_id, and populates them into parallel lists for
       titles and item_ids.
       Returns false if there are none, or true if there is at least one.*/
    virtual bool getBorrowings(int librarymixer_id, QStringList &titles, QList<int> &item_ids) = 0;
    //Fills out parallel lists in item_ids and statuses on all borrowings
    virtual void getBorrowingInfo(QList<int> &item_ids, QList<borrowStatuses> &statuses, QStringList &names) = 0;

    //Called when a user declines to borrow an item or wants to remove something from the borrow database.
    virtual void cancelBorrow(int item_id) = 0;
    //Causes a pending BorrowItem to start downloading.
    virtual void borrowPending(int item_id) = 0;
    /*Marks a borrowed item as now in the process of being returned.
      Adds it to the files database as a temporary item and sends a download invitation to the owner.*/
    virtual void returnBorrowed(int librarymixer_id, int item_id, QString title, QStringList paths) = 0;
};


#endif
