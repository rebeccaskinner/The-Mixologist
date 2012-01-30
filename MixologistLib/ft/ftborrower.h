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

#ifndef BORROWER_HEADER
#define BORROWER_HEADER

#include <QMap>
#include <QStringList>
#include <QMutex>
#include <interface/files.h>

class ftBorrowManager;
extern ftBorrowManager *borrowManager;

/* ftBorrowManager is used to track the items a user borrows from friends. */

/* Internal class used only by ftBorrowManager in borrowitems variable */
struct BorrowedItem {
    unsigned int friend_id; //friend from whom this was borrowed
    int source_type; //passed by the friend as an identifier to be returned
    QString source_id; //passed by the friend as an identifier to be returned
    QString title;

    /* Encodes the contained friend_id, source_type, and source_id into an itemKey. */
    QString itemKey() const;

    /* Decodes the key created by generateKey and sets friend_id, source_type, source_id. */
    void readKey(const QString &key);

    /* Encodes the provided friend_id, source_type, and source_id into an itemKey string, which can be used to uniquely identify this item. */
    static QString generateItemKey(unsigned int friend_id, int source_type, const QString &source_id);
};

class ftBorrowManager {
public:
    ftBorrowManager();

    /* Adds the given item to the borrowed database, and also sends a notification to friend with friend_id that
       we have the complete transfer associated with this information so that the friend can delete it on his end.
       If there is an existing borrowed item from that friend, identified by that source_type and source_id, it will
       be overwritten. */
    void addBorrowed(unsigned int friend_id, int source_type, const QString &source_id, const QString &title);

    /* Called from ftTemplist, initiates the return of an item from the borrow database.
       The fully-prepared temporary item must already have been constructed prior to calling this so that the files are available for the lender to request. */
    void returnBorrowed(const QString &itemKey, const QStringList &files, const QStringList &hashes, const QList<qlonglong> &filesizes);

    /* Removes a borrowed item that was being sent back from the borrow database. */
    void returnedBorrowed(unsigned int friend_id, int source_type, const QString &source_id);

    /**********************************************************************************
     * Body of public-facing API functions called through ftServer
     **********************************************************************************/

    /* Called when a user wants to remove something from the borrow database. */
    void deleteBorrowed(const QString &itemKey);

    /* Gets all items borrowed, and populates them into parallel lists for titles, itemKeys that identify them, and the friends they are borrowed from. */
    void getBorrowings(QStringList &titles, QStringList &itemKeys, QList<unsigned int> &friendIds);

    /* Get items borrowed from a specific friend. */
    void getBorrowings(QStringList &titles, QStringList &itemKeys, unsigned int friendId);

private:
    //Master list of all current borrowings, by the itemKeys
    QMap<QString, BorrowedItem> borrowedItems;
    QMutex borrowMutex;
};

#endif //BORROWER_HEADER

