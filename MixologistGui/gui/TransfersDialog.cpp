/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006-2007, crypton
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

#include "gui/TransfersDialog.h"
#include <gui/MainWindow.h>
#include <gui/PeersDialog.h>
#include "gui/Statusbar/ratesstatus.h"
#include "interface/peers.h"
#include "interface/files.h"
#include "interface/types.h"
#include <interface/settings.h>
#include "interface/librarymixer-connect.h"
#include "time.h"

#include <QUrl>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QTreeWidgetItem>
#include <QDesktopServices>

/* Images for context menu icons */
#define IMAGE_CANCEL               ":/Images/Cancel.png"
#define IMAGE_CLEARCOMPLETED       ":/Images/ClearCompleted.png"
#define IMAGE_CHAT                 ":/Images/Chat.png"
#define IMAGE_OPEN                 ":/Images/Play.png"
#define IMAGE_OPENCONTAINING       ":/Images/Folder.png"

#define UPLOAD_FILE_COLUMN         0
#define UPLOAD_FRIEND_COLUMN       1
#define UPLOAD_SPEED_COLUMN        2
#define UPLOAD_TRANSFERRED_COLUMN  3
#define UPLOAD_TOTAL_SIZE_COLUMN   4
#define UPLOAD_STATUS_COLUMN       5
#define UPLOAD_LIBRARYMIXER_ID     6

#define DOWNLOAD_NAME_COLUMN       0
#define DOWNLOAD_FRIEND_COLUMN     1
#define DOWNLOAD_SPEED_COLUMN      2
#define DOWNLOAD_REMAINING_COLUMN  3
#define DOWNLOAD_TOTAL_SIZE_COLUMN 4
#define DOWNLOAD_PERCENT_COLUMN    5
#define DOWNLOAD_STATUS_COLUMN     6
//Hidden information columns
#define DOWNLOAD_HASH_COLUMN 7
#define DOWNLOAD_LIBRARYMIXER_ID  8
#define DOWNLOAD_ITEM_ID     9

/** Constructor */
TransfersDialog::TransfersDialog(QWidget *parent)
: QWidget(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    connect(ui.downloadButton, SIGNAL(clicked()), this, SLOT(download()));
    connect(ui.downloadsList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(downloadsListContextMenu(QPoint)));
    connect(ui.uploadsList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(uploadsListContextMenu(QPoint)));

    QHeaderView *header = ui.downloadsList->header() ;
    header->hideSection(DOWNLOAD_HASH_COLUMN);
    header->hideSection(DOWNLOAD_LIBRARYMIXER_ID);
    header->hideSection(DOWNLOAD_ITEM_ID);

    header = ui.uploadsList->header() ;
    header->hideSection(UPLOAD_LIBRARYMIXER_ID);

    //Without this preliminary resizing, empty columns will visibly resize after view already displayed
    ui.downloadsList->resizeColumnToContents(DOWNLOAD_NAME_COLUMN);
    ui.downloadsList->resizeColumnToContents(DOWNLOAD_FRIEND_COLUMN);
    ui.downloadsList->resizeColumnToContents(DOWNLOAD_SPEED_COLUMN);
    ui.downloadsList->resizeColumnToContents(DOWNLOAD_REMAINING_COLUMN);
    ui.downloadsList->resizeColumnToContents(DOWNLOAD_TOTAL_SIZE_COLUMN);
    ui.downloadsList->resizeColumnToContents(DOWNLOAD_PERCENT_COLUMN);
    ui.downloadsList->resizeColumnToContents(DOWNLOAD_STATUS_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_FILE_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_FRIEND_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_SPEED_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_TRANSFERRED_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_TOTAL_SIZE_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_STATUS_COLUMN);

    ui.downloadsList->sortItems(0, Qt::AscendingOrder);
    ui.uploadsList->sortItems(0, Qt::AscendingOrder);
}

void TransfersDialog::download(const QString &link) {
    //Some args have default values of 0 to suppress spurious compiler warnings that they might be used uninitialized.
    bool ok;
    QString error;
    int librarymixer_id = 0;
    int item_id = 0;
    QString rawname;
    QString name;
    QStringList argList;

    mainwindow->show();
    mainwindow->raise();
    mainwindow->activateWindow();

    //Pop up a dialog box to allow the user to enter a link. If a link was provided in arguments, starts from there.
    //Some browsers percent encode urls, so undo this before processing the link from the argument.
    QString text = QInputDialog::getText(this, tr("Enter link"), tr("Enter a mixology link:"), QLineEdit::Normal, QUrl::fromPercentEncoding(link.toUtf8()), &ok);
    if (!ok) {
        error = "Unable to read the entered text";
        goto parseError;
    }

    //In case the user pasted in a percent encoded url, undo it again.
    text = QUrl::fromPercentEncoding(text.toUtf8());

    if (!text.startsWith("mixology:", Qt::CaseInsensitive)) {
        error = "Links should begin with mixology:";
        goto parseError;
    }
    //Remove the first 'mixology:' bit
    text = text.remove(0, 9);

    //mixology links are case insensitive and will take the form of:
    //mixology:userid==INTEGER¦name==STRING¦itemid==INTEGER¦
    //If the user pastes in something with a carriage return, don't let it miss up parsing.
    text = text.remove("\n");

    //Some web browsers seem to like to stick an extraneous "/" on the end of all links.
    if (text.endsWith("/")) text.chop(1);

    //On the server side, spaces are encoded into underscores, so convert them back.
    text = text.replace("_", " ");

    argList = text.split("¦", QString::SkipEmptyParts);

    for(int i = 0; i < argList.count(); i++) {
        QStringList keyValue = argList[i].split("==");
        if (keyValue.count() != 2) {
            error = "Encountered a part of the link that is missing its value";
            goto parseError;
        }
        if (QString::compare(keyValue[0], "userid", Qt::CaseInsensitive) == 0) {
            librarymixer_id = keyValue[1].toInt(&ok);
            if (!ok) {
                error = "Unable to read value of userid.";
                goto parseError;
            }
        } else if (QString::compare(keyValue[0], "itemid", Qt::CaseInsensitive) == 0) {
            item_id = keyValue[1].toInt(&ok);
            if (!ok) {
                error = "Unable to read value of itemid.";
                goto parseError;
            }
        } else if (QString::compare(keyValue[0], "name", Qt::CaseInsensitive) == 0) {
            rawname =  keyValue[1];
            name = rawname;
        }
    }
    if (librarymixer_id == 0) {
        error = "That link seemed a little bit messed up.";
        goto parseError;
    }
    if (librarymixer_id == peers->getOwnLibraryMixerId()) {
        QMessageBox::warning (this, "The Mixologist", "FYI, you just tried to connect to yourself", QMessageBox::Ok);
        return;
    } else if (!peers->isFriend(librarymixer_id)) {
        //If not in friends list, download friends list and try again
        librarymixerconnect->downloadFriends(true);
        if (!peers->isFriend(librarymixer_id)) {
            QMessageBox::warning (this,  "The Mixologist", "You can't connect to someone that's not your friend on LibraryMixer", QMessageBox::Ok);
            return;
        }
    }
    if (item_id != 0 && !name.isEmpty()) {
        files->LibraryMixerRequest(librarymixer_id, item_id, name);
        //We must construct our own QMessageBox rather than use a static function in order to avoid making a system sound on window popup
        QMessageBox confirmBox(this);
        //confirmBox.setIcon(QMessageBox::NoIcon);
        confirmBox.setIconPixmap(QPixmap(":/Images/Download.png"));
        confirmBox.setWindowTitle(name);
        confirmBox.setText("A request will be sent to " + peers->getPeerName(librarymixer_id));
        confirmBox.addButton("OK", QMessageBox::RejectRole);
        confirmBox.exec();
    }
    else mainwindow->peersDialog->getOrCreateChat(librarymixer_id, true);
    return;

parseError:
    QMessageBox::warning (this, "Unable to read link", error, QMessageBox::Ok);
    return;
}

void TransfersDialog::suggestionReceived(int librarymixer_id, int item_id, const QString &name) {
    QString friend_name = peers->getPeerName(librarymixer_id);
    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    if (settings.value("Transfers/IncomingAsk", DEFAULT_INCOMING_ASK).toBool()){
        mainwindow->trayOpenTo = mainwindow->transfersDialog;
        mainwindow->trayIcon->showMessage("Incoming File",
                                          "Received an invitation to download '" + name + "' from " + friend_name + ".",
                                          QSystemTrayIcon::Information);
        mainwindow->show();
        mainwindow->raise();
        mainwindow->activateWindow();
        if (QMessageBox::question(this, "Incoming File",
                                  "Received an invitation to download '" + name +
                                  "' from " + friend_name +
                                  ".  Download?", QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)
                    == QMessageBox::Yes) {
            files->LibraryMixerRequest(librarymixer_id, item_id, name);
        }
    } else {
        files->LibraryMixerRequest(librarymixer_id, item_id, name);
        mainwindow->trayOpenTo = mainwindow->transfersDialog;
        mainwindow->trayIcon->showMessage("Incoming File",
                                          "Receiving '" + name + "' from " + friend_name + ".",
                                          QSystemTrayIcon::Information);
    }
}

void TransfersDialog::insertTransfers() {

    /* get the download and upload lists */

    ui.downloadsList->clear();
    ui.uploadsList->clear();

    QList<QTreeWidgetItem *> transfers;

    float downTotalRate = 0;
    float upTotalRate = 0;

    //Stick downloads in the downloadsList box
    QList<FileInfo> downloads;
    files->FileDownloads(downloads);
    //For each file being downloaded
    for (int current_dl = 0; current_dl < downloads.size(); current_dl++) {
        //For each transfer that file is a part of
        for (int current_transfer = 0; current_transfer < downloads[current_dl].orig_item_ids.size(); current_transfer++) {
            QTreeWidgetItem *transfer_item = findOrCreateTransferItem(downloads[current_dl].orig_item_ids[current_transfer],
                                             downloads[current_dl].librarymixer_names[current_transfer], transfers);
            //Create a subitem for the file to stick underneath the top level transfer item
            QTreeWidgetItem *file_item = new QTreeWidgetItem(transfer_item);
            file_item -> setText(DOWNLOAD_NAME_COLUMN, downloads[current_dl].paths[current_transfer]);
            file_item -> setText(DOWNLOAD_TOTAL_SIZE_COLUMN, QString::number(downloads[current_dl].size / 1024, 'f', 0).append(" K"));
            if (downloads[current_dl].size > 0)
                file_item -> setText(DOWNLOAD_PERCENT_COLUMN, QString::number(100 * downloads[current_dl].transfered / downloads[current_dl].size , 'f', 0).append("%"));
            QString status;
            bool complete = false;
            switch (downloads[current_dl].downloadStatus) {
                case FT_STATE_FAILED:
                    status = "Failed";
                    complete = true;
                    break;
                case FT_STATE_INIT:
                    status = "Initializing";
                    break;
                case FT_STATE_WAITING:
                    status = "Friend offline";
                    break;
                case FT_STATE_DOWNLOADING:
                    status = "Downloading";
                    break;
                case FT_STATE_IDLE:
                    status = "Trying";
                    break;
                case FT_STATE_COMPLETE:
                    status = "Complete";
                    complete = true;
                    break;
                case FT_STATE_COMPLETE_WAIT:
                    status = "File Complete";
                    complete = true;
                    break;
                default:
                    status = "Error";
                    complete = true;
                    break;
            }
            if (!complete) {
                //Can't check for online status before status is set, because when download is complete, peers becomes empty.
                if (!peers->isOnline(downloads[current_dl].peers.front().librarymixer_id)) {
                    status = "Friend offline";
                }
                //This is how Retroshare did it, but is this right?  If there is lost data, will size - transfered potentially be less than zero?
                file_item -> setText(DOWNLOAD_REMAINING_COLUMN, QString::number((downloads[current_dl].size - downloads[current_dl].transfered) / 1024, 'f', 0).append(" K"));
                file_item -> setText(DOWNLOAD_FRIEND_COLUMN, peers->getPeerName(downloads[current_dl].peers.front().librarymixer_id));
                file_item -> setText(DOWNLOAD_LIBRARYMIXER_ID, QString::number(downloads[current_dl].peers.front().librarymixer_id));
            }

            file_item -> setText(DOWNLOAD_STATUS_COLUMN, status);
            if (status == "Downloading"){
                file_item -> setText(DOWNLOAD_SPEED_COLUMN, QString::number(downloads[current_dl].tfRate, 'f', 2).append(" K/s"));
                //Also update the running total for the status bar
                downTotalRate += downloads[current_dl].tfRate;
            }

            file_item->setText(DOWNLOAD_HASH_COLUMN, downloads[current_dl].hash.c_str());
            file_item->setText(DOWNLOAD_ITEM_ID, QString::number(downloads[current_dl].orig_item_ids[current_transfer]));
        }
    }
    //Also put pendingRequests into downloadsList box
    std::list<pendingRequest> pendingRequests;
    files->getPendingRequests(pendingRequests);

    std::list<pendingRequest>::iterator request_it;
    for (request_it = pendingRequests.begin(); request_it != pendingRequests.end(); request_it++) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item -> setText(DOWNLOAD_NAME_COLUMN, request_it->name);
        item -> setText(DOWNLOAD_FRIEND_COLUMN, peers->getPeerName(request_it->librarymixer_id));
        item -> setText(DOWNLOAD_LIBRARYMIXER_ID, QString::number(request_it->librarymixer_id));
        QString status;
        switch (request_it->status) {
            case pendingRequest::REPLY_INTERNAL_ERROR:
                status = "Error: Your friend had an internal error when responding";
                break;
            case pendingRequest::REPLY_NONE:
            default:
                if (!peers->isOnline(request_it->librarymixer_id)) status = "Friend offline";
                else status = "Trying";
                break;
        }
        item -> setText(DOWNLOAD_STATUS_COLUMN, status);
        item -> setText(DOWNLOAD_ITEM_ID, QString::number(request_it->item_id));
        transfers.append(item);
    }
    //Step through the borrowing list and update display with that
    QList<int> item_ids;
    QList<borrowStatuses> statuses;
    QStringList names;
    files->getBorrowingInfo(item_ids, statuses, names);
    for(int i = 0; i < item_ids.count(); i++) {
        QTreeWidgetItem *transfer_item = findOrCreateTransferItem(item_ids[i], names[i], transfers);
        switch (statuses[i]) {
            case BORROW_STATUS_PENDING:
                transfer_item->setText(DOWNLOAD_STATUS_COLUMN, "Waiting for input");
                break;
            case BORROW_STATUS_GETTING:
                transfer_item->setText(DOWNLOAD_STATUS_COLUMN, "Borrowing");
                break;
            case BORROW_STATUS_BORROWED:
                transfer_item->setText(DOWNLOAD_STATUS_COLUMN, "Borrowed");
                break;
            case BORROW_STATUS_NOT:
                break;
        }
    }

    ui.downloadsList->insertTopLevelItems(0, transfers);
    ui.downloadsList->expandAll();
    ui.downloadsList->update();
    ui.downloadsList->resizeColumnToContents(0);
    ui.downloadsList->resizeColumnToContents(1);
    ui.downloadsList->resizeColumnToContents(2);
    ui.downloadsList->resizeColumnToContents(3);
    ui.downloadsList->resizeColumnToContents(4);
    ui.downloadsList->resizeColumnToContents(5);
    ui.downloadsList->resizeColumnToContents(6);
    ui.downloadsList->resizeColumnToContents(7);
    transfers.clear();

    //Put uploads into uploadsList box
    QList<FileInfo> uploads;
    files->FileUploads(uploads);
    QList<FileInfo>::const_iterator it;
    for (it = uploads.begin(); it != uploads.end(); it++) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item -> setText(UPLOAD_FILE_COLUMN, it->paths.first());
        item -> setText(UPLOAD_FRIEND_COLUMN, peers->getPeerName(it->peers.front().librarymixer_id));
        item -> setText(UPLOAD_TOTAL_SIZE_COLUMN, QString::number(it->size / 1024, 'f', 0).append(" K"));
        QString status;
        switch (it->peers.front().status) {
            case FT_STATE_FAILED:
                status = tr("Failed");
                break;
            case FT_STATE_INIT:
                status = tr("Initializing");
                break;
            case FT_STATE_WAITING:
                status = tr("Waiting");
                break;
            case FT_STATE_DOWNLOADING:
                if (time(NULL) - it->lastTS > 5) status = "";
                else status = tr("Uploading");
                break;
            case FT_STATE_IDLE:
                status = tr("Idled");
                break;
            case FT_STATE_COMPLETE:
                status = tr("Complete");
                break;
            default:
                status = QString::number(it->peers.front().status);
                break;
        }
        item -> setText(UPLOAD_STATUS_COLUMN, status);
        item -> setText(UPLOAD_LIBRARYMIXER_ID, QString::number(it->peers.front().librarymixer_id));
        if (status == "Uploading") {
            item -> setText(UPLOAD_SPEED_COLUMN, QString::number(it->tfRate, 'f', 2).append(" K/s"));
            //Also update the running total for the status bar
            upTotalRate += it->tfRate;
        }
        if (status != "Complete") item -> setText(UPLOAD_TRANSFERRED_COLUMN, QString::number(it->transfered / 1024, 'f', 0).append(" K"));
        transfers.append(item);
    }

    ui.uploadsList->insertTopLevelItems(0, transfers);
    ui.uploadsList->update();
    ui.uploadsList->resizeColumnToContents(UPLOAD_FILE_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_FRIEND_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_SPEED_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_TRANSFERRED_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_TOTAL_SIZE_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_STATUS_COLUMN);
    transfers.clear();

    //Set the status bar
    mainwindow->ratesstatus->setRatesStatus(downTotalRate, upTotalRate);
    //Set the system tray icon
    //mainwindow->setTrayIcon(downTotalRate, upTotalRate);
}

void TransfersDialog::insertTransferEvent(int event, int librarymixer_id, const QString &transfer_name, const QString &extra_info) {
    if (event == NOTIFY_TRANSFER_LEND) {
        QString friend_name = peers->getPeerName(librarymixer_id);
        QString message = friend_name +
                " will lend " +
                transfer_name +
                " to you, but wants it back at some point. Continue?";
        if (QMessageBox::question(this, transfer_name, message, QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            files->borrowPending(extra_info.toInt());
            mainwindow->peersDialog->insertUserOptional(librarymixer_id, NOTIFY_USER_BORROW_ACCEPTED, transfer_name);
        } else {
            mainwindow->peersDialog->insertUserOptional(librarymixer_id, NOTIFY_USER_BORROW_DECLINED, transfer_name);
        }
    }
}

void TransfersDialog::downloadsListContextMenu(QPoint point) {
    QMenu contextMenu(this);

    //Store variables needed in case cancel is called
    //If the hash column is empty, we're dealing with a pending request (or an empty row), so get what we need to cancel that.
    //Otherwise, get the hash which we can use to cancel a file transfer.
    QTreeWidgetItem *contextItem = ui.downloadsList->itemAt(point);
    if (contextItem != NULL) {
        if (!contextItem->text(DOWNLOAD_LIBRARYMIXER_ID).isEmpty()) {
            context_friend_id = contextItem->text(DOWNLOAD_LIBRARYMIXER_ID).toInt();
            QAction *chatAct = new QAction(QIcon(IMAGE_CHAT), "Chat", &contextMenu);
            connect(chatAct, SIGNAL(triggered()), this, SLOT(chat()));
            contextMenu.addAction(chatAct);
        }

        contextMenu.addSeparator();

        if (contextItem->text(DOWNLOAD_HASH_COLUMN).isEmpty()) {
            context_item_id = contextItem->text(DOWNLOAD_ITEM_ID).toInt();
            context_hash = "";
        } else {
            context_item_id = contextItem->text(DOWNLOAD_ITEM_ID).toInt();
            context_hash = contextItem->text(DOWNLOAD_HASH_COLUMN).toStdString();
        }

        /*Show cancel for all top level transfer items as well as
                 individual files as long as the file is not either complete or part of a borrow.*/
        if (contextItem->parent() == NULL ||
                        (contextItem->text(DOWNLOAD_STATUS_COLUMN) != "Complete" &&
                         !contextItem->parent()->text(DOWNLOAD_STATUS_COLUMN).startsWith("Borrow"))) {
            context_name = contextItem->text(DOWNLOAD_NAME_COLUMN);
            QAction *cancelAct = new QAction(QIcon(IMAGE_CANCEL), tr("Cancel"), &contextMenu);
            connect(cancelAct, SIGNAL(triggered()), this, SLOT(cancel()));
            contextMenu.addAction(cancelAct);
        }
        if (contextItem->text(DOWNLOAD_STATUS_COLUMN) == "Complete") {
            context_name = files->getDownloadDirectory() + QDir::separator() + contextItem->text(DOWNLOAD_NAME_COLUMN);
            QAction *openAct = new QAction(QIcon(IMAGE_OPEN), tr("Open"), &contextMenu);
            connect(openAct, SIGNAL(triggered()), this, SLOT(openFile()));
            contextMenu.addAction(openAct);

            QAction *openContainingAct = new QAction(QIcon(IMAGE_OPENCONTAINING), tr("Open Downloads Folder"), &contextMenu);
            connect(openContainingAct, SIGNAL(triggered()), this, SLOT(openContaining()));
            contextMenu.addAction(openContainingAct);
        }

        contextMenu.addSeparator();
    }
    QAction *clearCompletedAct = new QAction(QIcon(IMAGE_CLEARCOMPLETED), tr("Clear Completed"), &contextMenu);
    connect(clearCompletedAct , SIGNAL(triggered()), this, SLOT(clearCompleted()));
    contextMenu.addAction(clearCompletedAct);

    contextMenu.exec(ui.downloadsList->mapToGlobal(point));
}

void TransfersDialog::uploadsListContextMenu(QPoint point) {
    QMenu contextMenu(this);

    QTreeWidgetItem *contextItem = ui.uploadsList->itemAt(point);
    if (contextItem != NULL) {
        if (!contextItem->text(UPLOAD_LIBRARYMIXER_ID).isEmpty()) {
            context_friend_id = contextItem->text(UPLOAD_LIBRARYMIXER_ID).toInt();
            QAction *chatAct = new QAction(QIcon(IMAGE_CHAT), "Chat", &contextMenu);
            connect(chatAct, SIGNAL(triggered()), this, SLOT(chat()));
            contextMenu.addAction(chatAct);
        }

        contextMenu.addSeparator();
        context_name = contextItem->text(UPLOAD_FILE_COLUMN);
        QAction *openAct = new QAction(QIcon(IMAGE_OPEN), tr("Open"), &contextMenu);
        connect(openAct, SIGNAL(triggered()), this, SLOT(openFile()));
        contextMenu.addAction(openAct);

        contextMenu.addSeparator();
    }

    QAction *clearAct = new QAction(QIcon(IMAGE_CLEARCOMPLETED), tr("Clear All"), &contextMenu);
    connect(clearAct , SIGNAL(triggered()), this, SLOT(clearUploads()));
    contextMenu.addAction(clearAct);

    contextMenu.exec(ui.uploadsList->mapToGlobal(point));
}

void TransfersDialog::cancel() {
    QString message("Are you sure that you want to cancel and delete:\n");
    message.append(context_name);
    if ((QMessageBox::question(this, tr("Cancel Download"), message,
                               QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)) == QMessageBox::Yes) {
        if (context_hash.empty()) {
            files->LibraryMixerRequestCancel(context_item_id);
        } else {
            files->cancelFile(context_item_id, context_name, context_hash);
            context_hash.clear();
        }
    }
}

void TransfersDialog::openFile() {
    QDesktopServices::openUrl(QUrl("file:///" + context_name, QUrl::TolerantMode));
}

void TransfersDialog::openContaining() {
    QDesktopServices::openUrl(QUrl("file:///" + files->getDownloadDirectory(), QUrl::TolerantMode));
}

void TransfersDialog::clearCompleted() {
    files->clearCompletedFiles();
}

void TransfersDialog::clearUploads() {
    files->clearUploads();
}

void TransfersDialog::chat() {
    if (context_friend_id != 0)
        mainwindow->peersDialog->getOrCreateChat(context_friend_id, true);
}

QTreeWidgetItem *TransfersDialog::findOrCreateTransferItem(int item_id, QString name, QList<QTreeWidgetItem *> &transfers) {
    QTreeWidgetItem *transfer_item;
    QList<QTreeWidgetItem *>::iterator all_transfers_it;
    //See if we already have a top level transfer item to hold this
    for (all_transfers_it = transfers.begin(); all_transfers_it != transfers.end(); all_transfers_it++) {
        if ((*all_transfers_it)->text(DOWNLOAD_ITEM_ID) == QString::number(item_id)) {
            transfer_item = *all_transfers_it;
            break;
        }
    }
    //If not, create one
    if (all_transfers_it == transfers.end()) {
        transfer_item = new QTreeWidgetItem();
        transfers.append(transfer_item);
        //transfer_item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
        transfer_item -> setText(DOWNLOAD_NAME_COLUMN, name);
        transfer_item -> setText(DOWNLOAD_STATUS_COLUMN, "");
        transfer_item -> setText(DOWNLOAD_ITEM_ID, QString::number(item_id));
    }
    return transfer_item;
}
