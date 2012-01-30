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


#ifndef FT_TEMP_LIST_HEADER
#define FT_TEMP_LIST_HEADER

/*
 * This class enables sharing of files on a temporary basis.
 * It maintains the master XML in which temporary files are stored.
 */

#include "ft/ftfilemethod.h"
#include "interface/files.h"
#include "interface/types.h"
#include <QMutex>

class ftTempList;
extern ftTempList *tempList;

class ftTempList: public ftFileMethod {
    Q_OBJECT

public:
    ftTempList();

    /* Called when the transfer is complete for a temp item that is created for the return of a borrowed item.
       Removes the item from the temp list, and deletes the files from disk. */
    void removeReturnBorrowedTempItem(const QString &itemKey);

    /**********************************************************************************
     * Body of public-facing API functions called through ftServer
     **********************************************************************************/

    /* Creates a new temporary item.
       A download suggestion will be sent to recipient as soon as hashing is complete.
       If an itemKey from the borrow database is given, then this will be sent as a return of that item. */
    void addTempItem(const QString &title, const QStringList &paths, unsigned int friend_id, const QString &itemKey = "");

    /**********************************************************************************
     * Implementations for ftFileMethod
     **********************************************************************************/

    /* Returns true if it is able to find a file with matching hash and size.
       If a file is found, populates path. */
    virtual ftFileMethod::searchResult search(const QString &hash, qlonglong size, uint32_t hintflags, QString &path);

private slots:
    /* Connected to ftFileWatcher for when a file's old hash is no longer valid.
       The path should be supplied with native directory separators. */
    void oldHashInvalidated(QString path, qlonglong size, unsigned int modified);

    /* Connected to ftFileWatcher for when a file has been hashed in order to update with the new information.
       The path should be supplied with native directory separators. */
    void newFileHash(QString path, qlonglong size, unsigned int modified, QString newHash);

    /* Connected to ftFileWatcher for when a file has been removed.
       The path should be supplied with native directory separators. */
    void fileRemoved(QString path);

private:
    /* Removes the given file specified by index from item, and removes the whole item if there are not files left. */
    void removeFile(TempShareItem* item, int index);

    /* Removes the given item and updates the Xml. */
    void removeTempItem(TempShareItem* item);

    /* Causes an invitation to download the temp item to be sent to its intended recipient.
       If not all files are fully hashed, does nothing. */
    void sendTempItemIfReady(TempShareItem* item);

    mutable QMutex tempItemMutex;
    QDomDocument xml; //The master XML for a user's temp list
    QList<TempShareItem*> tempList;
};

#endif
