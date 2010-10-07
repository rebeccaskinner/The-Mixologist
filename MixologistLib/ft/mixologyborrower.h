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

#ifndef MIXOLOGY_BORROWER_HEADER
#define MIXOLOGY_BORROWER_HEADER

#include <QMap>
#include <QStringList>
#include <QMutex>
#include <interface/files.h>

/* MixologyBorrower is used to track the entire lifecycle of borrowing a librarymixer item from a friend. */

/* Internal class used only by MixologyBorrower in borrowitems variable */
class BorrowItem {
public:
    int item_id; //librarymixer id of item
    int librarymixer_id; //original source friend's id
    QString librarymixer_name;
    borrowStatuses status;

    QList<std::string> hashes;
    QList<int> filesizes;
    QStringList filenames;
};

class MixologyBorrower {
public:
    MixologyBorrower();

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
    //Marks a borrowed item as now in the process of being returned.
    void returnBorrowed(int item_id);
    //Removes a borrowed item that was being sent back from the borrow database.
    void returnedBorrowed(int item_id);
    /*Checks if an item is in the borrow database as being borrowed, and if it was marks it as now borrowed.
      Returns true if the item was found, false if the item wasn't borrowed */
    void completedDownloadBorrowCheck(int item_id);

private:
    //Map by librarymixer item id containing a BorrowItem.
    //Used to track all borrowings.
    QMap<int, BorrowItem> borrowitems;
    QMutex borrowMutex;
};

#endif //MIXOLOGY_BORROWER_HEADER

