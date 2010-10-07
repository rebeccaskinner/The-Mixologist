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

#include <ft/mixologyborrower.h>
#include <ft/ftserver.h>
#include <interface/settings.h>
#include <QSettings>
#include <QMutexLocker>

MixologyBorrower::MixologyBorrower() {
    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Borrowed");
    QStringList borrowings = saved.childGroups();
    for (int i = 0; i < borrowings.size(); i++) {
        BorrowItem item;
        saved.beginGroup(borrowings[i]);
        item.item_id = borrowings[i].toInt();
        item.librarymixer_id = saved.value("friend").toInt();
        item.librarymixer_name = saved.value("name").toString();
        item.status = (borrowStatuses) saved.value("status").toInt();
        borrowitems[item.item_id] = item;
        saved.endGroup();
    }
}

bool MixologyBorrower::getBorrowings(int librarymixer_id, QStringList &titles, QList<int> &item_ids) {
    bool found = false;
    foreach(BorrowItem item, borrowitems) {
        if(item.librarymixer_id == librarymixer_id) {
            found = true;
            titles.append(item.librarymixer_name);
            item_ids.append(item.item_id);
        }
    }
    return found;
}

void MixologyBorrower::getBorrowingInfo(QList<int> &item_ids, QList<borrowStatuses> &statuses, QStringList &names) {
    foreach(BorrowItem item, borrowitems) {
        item_ids.append(item.item_id);
        statuses.append(item.status);
        names.append(item.librarymixer_name);
    }
}

void MixologyBorrower::addPendingBorrow(int item_id, QString librarymixer_name, int librarymixer_id, QStringList filenames, QStringList hashes, QStringList filesizes) {
    BorrowItem item;
    item.status = BORROW_STATUS_PENDING;
    item.item_id = item_id;
    item.librarymixer_name = librarymixer_name;
    item.librarymixer_id = librarymixer_id;
    for(int i = 0; i < filenames.count(); i++) {
        if(!filenames[i].isEmpty() && !hashes[i].isEmpty() && !filesizes[i].isEmpty()) {
            item.filenames.append(filenames[i]);
            item.filesizes.append(filesizes[i].toInt());
            item.hashes.append(hashes[i].toStdString());;
        }
    }
    QMutexLocker locker(&borrowMutex);
    borrowitems[item_id] = item;
}

void MixologyBorrower::cancelBorrow(int item_id) {
    QMutexLocker locker(&borrowMutex);
    borrowitems.remove(item_id);

    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Borrowed");
    saved.remove(QString::number(item_id));
    saved.sync();
}

void MixologyBorrower::borrowPending(int item_id) {
    QMutexLocker locker(&borrowMutex);
    QMap<int,BorrowItem>::iterator it = borrowitems.find(item_id);
    if (it != borrowitems.end()) {
        QList<int> sourceIds;
        sourceIds.push_back(it.value().librarymixer_id);
        for (int i = 0; i < it.value().filenames.size(); i++) {
            files->requestFile(it.value().librarymixer_name, it.value().item_id, it.value().filenames[i],
                               it.value().hashes[i], it.value().filesizes[i], 0, sourceIds);
        }
        it.value().status = BORROW_STATUS_GETTING;

        //Now that we're actually borrowing it, we should save the record to disk
        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.beginGroup("Borrowed");
        saved.beginGroup(QString::number(item_id));
        saved.setValue("friend", it.value().librarymixer_id);
        saved.setValue("name", it.value().librarymixer_name);
        saved.setValue("status", it.value().status);
        saved.sync();
    }
}

void MixologyBorrower::completedDownloadBorrowCheck(int item_id) {
    if (borrowitems.contains(item_id) && borrowitems[item_id].status == BORROW_STATUS_GETTING) {
        QMutexLocker locker(&borrowMutex);
        borrowitems[item_id].status = BORROW_STATUS_BORROWED;

        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.beginGroup("Borrowed");
        saved.beginGroup(QString::number(item_id));
        saved.setValue("status", borrowitems[item_id].status);
        saved.sync();

        ftserver->LibraryMixerBorrowed(borrowitems[item_id].librarymixer_id, item_id);
    }
}

void MixologyBorrower::returnedBorrowed(int item_id) {
    if (borrowitems.contains(item_id) && borrowitems[item_id].status == BORROW_STATUS_BORROWED) {
        ftserver->deleteRemoveItem(item_id);
        borrowitems.remove(item_id);

        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.beginGroup("Borrowed");
        saved.remove(QString::number(item_id));
        saved.sync();
    }
}
