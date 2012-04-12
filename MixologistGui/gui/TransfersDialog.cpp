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
#include "interface/notifyqt.h"
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
#include <QFileDialog>

/* Images for context menu icons */
#define IMAGE_CANCEL               ":/Images/Cancel.png"
#define IMAGE_SENDFILE             ":/Images/Send.png"
#define IMAGE_CLEARCOMPLETED       ":/Images/ClearCompleted.png"
#define IMAGE_CHAT                 ":/Images/Chat.png"
#define IMAGE_OPEN                 ":/Images/Play.png"
#define IMAGE_OPENCONTAINING       ":/Images/Folder.png"
#define IMAGE_DOWNLOAD             ":/Images/Download.png"

/* Upload column names */
#define UPLOAD_FILE_COLUMN         0
#define UPLOAD_FRIEND_COLUMN       1
#define UPLOAD_SPEED_COLUMN        2
#define UPLOAD_TRANSFERRED_COLUMN  3
#define UPLOAD_STATUS_COLUMN       4
#define UPLOAD_LIBRARYMIXER_ID     5
#define UPLOAD_ITEM_TYPE           6
#define UPLOAD_ITEM_ID             7

/* The possible results for the UPLOAD_ITEM_TYPE column. */
#define UPLOAD_FILE_TYPE         "File"
#define UPLOAD_PENDING_TYPE      "SuggestPending"
#define UPLOAD_PENDING_FILE_TYPE "PendingFile"

/* Download column names */
#define DOWNLOAD_NAME_COLUMN       0
#define DOWNLOAD_FRIEND_COLUMN     1
#define DOWNLOAD_SPEED_COLUMN      2
#define DOWNLOAD_REMAINING_COLUMN  3
#define DOWNLOAD_TOTAL_SIZE_COLUMN 4
#define DOWNLOAD_PERCENT_COLUMN    5
#define DOWNLOAD_STATUS_COLUMN     6
/* Hidden information columns. */
#define DOWNLOAD_FRIEND_ID         7
#define DOWNLOAD_ITEM_TYPE         8
#define DOWNLOAD_ITEM_ID           9
#define DOWNLOAD_FINAL_LOCATION    10

/* The possible results for the DOWNLOAD_ITEM_TYPE column */
#define DOWNLOAD_GROUP_TYPE        "Group"
#define DOWNLOAD_FILE_TYPE         "File"
#define DOWNLOAD_PENDING_TYPE      "Pending"
#define DOWNLOAD_BORROW_TYPE       "Borrow"

TransfersDialog::TransfersDialog(QWidget *parent)
: QWidget(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    connect(ui.downloadButton, SIGNAL(clicked()), this, SLOT(download()));
    connect(ui.downloadsList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(downloadsListContextMenu(QPoint)));
    connect(ui.uploadsList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(uploadsListContextMenu(QPoint)));

    //To popup a box informing that a friend is attempting to send a file.
    //QList<qlonglong> is not a default registerd type for QT's meta-object system, so we must first manually register it.
    qRegisterMetaType<QList<qlonglong> >("QList<qlonglong>");
    QObject::connect(guiNotify, SIGNAL(suggestionReceived(unsigned int, QString, QStringList, QStringList, QList<qlonglong>)),
                     this, SLOT(suggestionReceived(unsigned int, QString, QStringList, QStringList, QList<qlonglong>)), Qt::QueuedConnection);

    QHeaderView *header = ui.downloadsList->header() ;
    header->hideSection(DOWNLOAD_FRIEND_ID);
    header->hideSection(DOWNLOAD_ITEM_TYPE);
    header->hideSection(DOWNLOAD_ITEM_ID);
    header->hideSection(DOWNLOAD_FINAL_LOCATION);

    header = ui.uploadsList->header() ;
    header->hideSection(UPLOAD_LIBRARYMIXER_ID);
    header->hideSection(UPLOAD_ITEM_ID);
    header->hideSection(UPLOAD_ITEM_TYPE);

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
    ui.uploadsList->resizeColumnToContents(UPLOAD_STATUS_COLUMN);

    ui.downloadsList->sortItems(0, Qt::AscendingOrder);
    ui.uploadsList->sortItems(0, Qt::AscendingOrder);

    connect(files, SIGNAL(responseLendOfferReceived(uint,uint,QString,QStringList,QStringList,QList<qlonglong>)),
            this, SLOT(responseLendOfferReceived(uint,uint,QString,QStringList,QStringList,QList<qlonglong>)), Qt::QueuedConnection);

    /* This needs to change in the future, but for now, we simply refresh the entire view (including total transfer rates in corner) once every second. */
    QTimer *timer = new QTimer(this);
    timer->connect(timer, SIGNAL(timeout()), this, SLOT(insertTransfers()));
    timer->start(1000);
}

void TransfersDialog::download() {
    unsigned int friend_id;
    unsigned int item_id;
    QString name;

    mainwindow->show();
    mainwindow->raise();
    mainwindow->activateWindow();

    bool ok;
    QString text = QInputDialog::getText(this, tr("Enter link"), tr("Enter a mixology link:"), QLineEdit::Normal, "", &ok);

    /* ok will be false when the user hits cancel. */
    if (!ok || text.isEmpty()) return;

    if (!parseMixologyLink(text, &friend_id, &name, &item_id)) {
        QMessageBox::warning (this, "Unable to read link", "The link you pasted in there seemed a little messed up", QMessageBox::Ok);
        return;
    }

    handleMixologyLink(friend_id, name, item_id, true);

    return;
}

void TransfersDialog::download(const QString &link) {
    unsigned int friend_id;
    unsigned int item_id;
    QString name;

    mainwindow->show();
    mainwindow->raise();
    mainwindow->activateWindow();

    if (!parseMixologyLink(link, &friend_id, &name, &item_id)) {
        QMessageBox::warning (this, "Unable to read link", "The Mixologist was passed a malformed link by your browser or another program", QMessageBox::Ok);
    }

    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    if (settings.value("Transfers/MixologyLinkAsk", DEFAULT_MIXOLOGY_LINK_ASK).toBool()) {
        QMessageBox confirmBox(this);
        confirmBox.setIconPixmap(QPixmap(IMAGE_DOWNLOAD));
        confirmBox.setWindowTitle("Mixology Link");
        confirmBox.setText(QString("The Mixologist has received a mixology link!\n") +
                           "Request: " + name + "\n" +
                           "Friend: " + peers->getPeerName(friend_id) + "\n" +
                           "Continue?");
        confirmBox.addButton(QMessageBox::Yes);
        confirmBox.addButton(QMessageBox::No);
        confirmBox.setDefaultButton(QMessageBox::Yes);
        int result = confirmBox.exec();
        if (result == QMessageBox::No) return;
    }

    handleMixologyLink(friend_id, name, item_id, false);
}

bool TransfersDialog::parseMixologyLink(const QString &inputText, unsigned int *friend_id, QString *name, unsigned int *item_id) {
    /* mixology links are case insensitive and will take the form of:
       mixology:userid==INTEGER¦name==STRING¦itemid==INTEGER¦ */

    /* In case the user pasted in a percent encoded url, undo it again. */
    QString text = QUrl::fromPercentEncoding(inputText.toUtf8());

    if (!text.startsWith("mixology:", Qt::CaseInsensitive)) return false;

    /* Remove the first 'mixology:' bit. */
    text = text.remove(0, 9);

    /* If the user pastes in something with a carriage return, don't let it miss up parsing. */
    text = text.remove("\n");

    /* Some web browsers seem to like to stick an extraneous "/" on the end of all links. */
    if (text.endsWith("/")) text.chop(1);

    /* On the server side, spaces are encoded into underscores, so convert them back. */
    text = text.replace("_", " ");

    QStringList argList = text.split("¦", QString::SkipEmptyParts);

    bool ok;
    for(int i = 0; i < argList.count(); i++) {
        QStringList keyValue = argList[i].split("==");
        if (keyValue.count() != 2) return false;
        if (QString::compare(keyValue[0], "userid", Qt::CaseInsensitive) == 0) {
            *friend_id = keyValue[1].toInt(&ok);
            if (!ok) return false;
        } else if (QString::compare(keyValue[0], "itemid", Qt::CaseInsensitive) == 0) {
            *item_id = keyValue[1].toInt(&ok);
            if (!ok) return false;
        } else if (QString::compare(keyValue[0], "name", Qt::CaseInsensitive) == 0) {
            *name =  keyValue[1];
        }
    }

    if (*friend_id == 0) return false;

    return true;
}

void TransfersDialog::handleMixologyLink(unsigned int friend_id, const QString &name, unsigned int item_id, bool popupMessages) {
    if (friend_id == peers->getOwnLibraryMixerId()) {
        QMessageBox::warning (this, "The Mixologist", "FYI, you just tried to connect to yourself", QMessageBox::Ok);
        return;
    } else if (!peers->isFriend(friend_id)) {
        /* If not in friends list, download friends list and try again. */
        librarymixerconnect->downloadFriends(true);
        if (!peers->isFriend(friend_id)) {
            QMessageBox::warning (this,  "The Mixologist", "You can't connect to someone that's not your friend on LibraryMixer", QMessageBox::Ok);
            return;
        }
    }

    if (item_id != 0 && !name.isEmpty()) {
        files->LibraryMixerRequest(friend_id, item_id, name);
        if (popupMessages) {
            //We must construct our own QMessageBox rather than use a static function in order to avoid making a system sound on window popup
            QMessageBox confirmBox(this);
            confirmBox.setIconPixmap(QPixmap(IMAGE_DOWNLOAD));
            confirmBox.setWindowTitle(name);
            confirmBox.setText("A request will be sent to " + peers->getPeerName(friend_id));
            confirmBox.addButton("OK", QMessageBox::RejectRole);
            confirmBox.exec();
        }
    }
    else mainwindow->peersDialog->getOrCreateChat(friend_id, true);
}

void TransfersDialog::suggestionReceived(unsigned int librarymixer_id, QString title, QStringList paths, QStringList hashes, QList<qlonglong> filesizes) {
    QString friend_name = peers->getPeerName(librarymixer_id);
    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    if (settings.value("Transfers/IncomingAsk", DEFAULT_INCOMING_ASK).toBool()){
        mainwindow->trayMessageClickedAction = MainWindow::TRAY_MESSAGE_CLICKED_TRANSFERS_DIALOG;
        mainwindow->trayIcon->showMessage("Incoming File",
                                          "Received an invitation to download '" + title + "' from " + friend_name + ".",
                                          QSystemTrayIcon::Information);
        mainwindow->show();
        mainwindow->raise();
        mainwindow->activateWindow();
        if (QMessageBox::question(this, "Incoming File",
                                  "Received an invitation to download '" + title +
                                  "' from " + friend_name +
                                  ".  Download?", QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)
                    == QMessageBox::Yes) {
            files->downloadFiles(librarymixer_id, title, paths, hashes, filesizes);
        }
    } else {
        if (files->downloadFiles(librarymixer_id, title, paths, hashes, filesizes)) {
            mainwindow->trayMessageClickedAction = MainWindow::TRAY_MESSAGE_CLICKED_TRANSFERS_DIALOG;
            mainwindow->trayIcon->showMessage("Incoming File",
                                              "Receiving '" + title + "' from " + friend_name + ".",
                                              QSystemTrayIcon::Information);
        }
    }
}

void TransfersDialog::insertTransfers() {
    float downTotalRate = insertDownloads();
    float upTotalRate = insertUploads();

    //Set the status bar
    mainwindow->ratesstatus->setRatesStatus(downTotalRate, upTotalRate);

    //Set the system tray icon
    //mainwindow->setTrayIcon(downTotalRate, upTotalRate);
}

float TransfersDialog::insertDownloads() {
    ui.downloadsList->clear();

    QList<QTreeWidgetItem *> transfers;

    float downTotalRate = 0;

    /* Insert borrowed items. */
    QStringList borrowTitles;
    QStringList borrowKeys;
    QList<unsigned int> friendIds;
    files->getBorrowings(borrowTitles, borrowKeys, friendIds);
    for(int i = 0; i < borrowTitles.count(); i++) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(DOWNLOAD_NAME_COLUMN, borrowTitles[i]);
        item->setText(DOWNLOAD_STATUS_COLUMN, "Borrowed");
        item->setText(DOWNLOAD_FRIEND_COLUMN, peers->getPeerName(friendIds[i]));
        item->setText(DOWNLOAD_FRIEND_ID, QString::number(friendIds[i]));
        item->setText(DOWNLOAD_ITEM_TYPE, DOWNLOAD_BORROW_TYPE);
        item->setText(DOWNLOAD_ITEM_ID, borrowKeys[i]);
        transfers.append(item);
    }

    /* Insert file downloads. */
    QList<downloadGroupInfo> downloads;
    files->FileDownloads(downloads);
    foreach (downloadGroupInfo group, downloads) {
        QTreeWidgetItem *transferItem = new QTreeWidgetItem();
        transfers.append(transferItem);
        double groupTransferRate = 0;

        transferItem->setText(DOWNLOAD_NAME_COLUMN, group.title);
        //We can do just take the first file's first peer for the group's friend because multi-source isn't enabled
        transferItem->setText(DOWNLOAD_FRIEND_COLUMN, peers->getPeerName(group.filesInGroup.first().peers.first().librarymixer_id));
        transferItem->setText(DOWNLOAD_FRIEND_ID, QString::number(group.filesInGroup.first().peers.first().librarymixer_id));
        transferItem->setText(DOWNLOAD_ITEM_TYPE, DOWNLOAD_GROUP_TYPE);
        transferItem->setText(DOWNLOAD_ITEM_ID, QString::number(group.groupId));
        if (!group.finalDestination.isEmpty()) {
            transferItem->setText(DOWNLOAD_STATUS_COLUMN, "Complete");
            transferItem->setText(DOWNLOAD_FINAL_LOCATION, group.finalDestination);
        }

        for (int fileNumber = 0; fileNumber < group.filesInGroup.count(); fileNumber++) {
            QTreeWidgetItem *fileItem = new QTreeWidgetItem(transferItem);
            fileItem->setText(DOWNLOAD_NAME_COLUMN, group.filenames[fileNumber]);
            fileItem->setText(DOWNLOAD_TOTAL_SIZE_COLUMN, QString::number(group.filesInGroup[fileNumber].totalSize / 1024, 'f', 0).append(" K"));
            fileItem->setText(DOWNLOAD_ITEM_TYPE, DOWNLOAD_FILE_TYPE);
            fileItem->setText(DOWNLOAD_ITEM_ID, group.filesInGroup[fileNumber].hash);
            /* For percentages, size 0 is a special case because we cannot calculate percent without divide by 0.
               If we haven't transferred anything at all yet, leave percentage blank for a cleaner look. */
            if (group.filesInGroup[fileNumber].totalSize == 0) {
                fileItem->setText(DOWNLOAD_PERCENT_COLUMN, "100%");
            } else if (group.filesInGroup[fileNumber].downloadedSize > 0) {
                fileItem->setText(DOWNLOAD_PERCENT_COLUMN,
                                  QString::number(100 * group.filesInGroup[fileNumber].downloadedSize / group.filesInGroup[fileNumber].totalSize , 'f', 0).append("%"));
            }

            bool complete = false;
            QString status;
            //We can only do this with first() because multisource isn't implemented yet
            if (!peers->isOnline(group.filesInGroup[fileNumber].peers.first().librarymixer_id) &&
                group.filesInGroup[fileNumber].downloadStatus != FT_STATE_COMPLETE)
                status = "Friend offline";
            else {
                switch (group.filesInGroup[fileNumber].downloadStatus) {
                case FT_STATE_WAITING:
                    status = "Waiting";
                    break;
                case FT_STATE_TRANSFERRING:
                    if (group.filesInGroup[fileNumber].totalTransferRate > 0) {
                        status = "Downloading";
                    } else {
                        status = "Trying";
                    }
                    fileItem->setText(DOWNLOAD_SPEED_COLUMN, QString::number(group.filesInGroup[fileNumber].totalTransferRate, 'f', 2).append(" K/s"));
                    //Also update the running total for the status bar
                    downTotalRate += group.filesInGroup[fileNumber].totalTransferRate;
                    groupTransferRate += group.filesInGroup[fileNumber].totalTransferRate;
                    break;
                case FT_STATE_COMPLETE:
                    status = "Complete";
                    complete = true;
                    break;
                }
            }
            fileItem->setText(DOWNLOAD_STATUS_COLUMN, status);

            if (!complete) {
                fileItem->setText(DOWNLOAD_REMAINING_COLUMN,
                                  QString::number((group.filesInGroup[fileNumber].totalSize - group.filesInGroup[fileNumber].downloadedSize) / 1024, 'f', 0).append(" K"));
                fileItem->setText(DOWNLOAD_FRIEND_COLUMN, peers->getPeerName(group.filesInGroup[fileNumber].peers.first().librarymixer_id));
                fileItem->setText(DOWNLOAD_FRIEND_ID, QString::number(group.filesInGroup[fileNumber].peers.first().librarymixer_id));
            } else {
                fileItem->setText(DOWNLOAD_FINAL_LOCATION, group.finalDestination + QDir::separator() + group.filenames[fileNumber]);
            }
        }

        if (groupTransferRate > 0) {
            transferItem->setText(DOWNLOAD_SPEED_COLUMN, QString::number(groupTransferRate, 'f', 2).append(" K/s"));
        }
    }

    /* Insert requests for LibraryMixer items. */
    std::list<pendingRequest> pendingRequests;
    files->getPendingRequests(pendingRequests);
    std::list<pendingRequest>::iterator request_it;
    for (request_it = pendingRequests.begin(); request_it != pendingRequests.end(); request_it++) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(DOWNLOAD_NAME_COLUMN, request_it->name);
        item->setText(DOWNLOAD_FRIEND_COLUMN, peers->getPeerName(request_it->friend_id));
        item->setText(DOWNLOAD_FRIEND_ID, QString::number(request_it->friend_id));
        QString status;
        switch (request_it->status) {
        case pendingRequest::REPLY_INTERNAL_ERROR:
            status = "Error: Your friend had an internal error when responding";
            break;
        case pendingRequest::REPLY_LENT_OUT:
            status = "Response: Your friend has currently lent it out (will keep trying)";
            break;
        case pendingRequest::REPLY_UNMATCHED:
            //REPLY_UNMATCHED shares behavior with REPLY_CHAT, so we double them up
        case pendingRequest::REPLY_CHAT:
            status = "Response: Open chat window";
            break;
        case pendingRequest::REPLY_MESSAGE:
            status = "Response: Show message";
            break;
        case pendingRequest::REPLY_NO_SUCH_ITEM:
            status = "Error: Your friend says they don't have that item";
            break;
        case pendingRequest::REPLY_BROKEN_MATCH:
            status = "Error: Your friend set a file to send, but it's been moved or deleted";
            break;
        default:
            if (!peers->isOnline(request_it->friend_id)) status = "Friend offline";
            else status = "Trying";
            break;
        }
        item->setText(DOWNLOAD_STATUS_COLUMN, status);
        item->setText(DOWNLOAD_ITEM_TYPE, DOWNLOAD_PENDING_TYPE);
        item->setText(DOWNLOAD_ITEM_ID, QString::number(request_it->item_id));
        transfers.append(item);
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

    return downTotalRate;
}

float TransfersDialog::insertUploads() {
    QList<QTreeWidgetItem *> transfers;

    ui.uploadsList->clear();
    float upTotalRate = 0;

    /* Put uploads into uploadsList box. */
    QList<uploadFileInfo> uploads;
    files->FileUploads(uploads);

    foreach (uploadFileInfo currentFileInfo, uploads) {
        foreach (TransferInfo currentFriendInfo, currentFileInfo.peers) {
            QTreeWidgetItem *uploadedFile = new QTreeWidgetItem();
            uploadedFile->setText(UPLOAD_FILE_COLUMN, currentFileInfo.path);
            uploadedFile->setText(UPLOAD_FRIEND_COLUMN, peers->getPeerName(currentFriendInfo.librarymixer_id));
            uploadedFile->setText(UPLOAD_TRANSFERRED_COLUMN, QString::number(currentFriendInfo.transferred / 1024, 'f', 0).append(" K"));

            if (currentFriendInfo.status == FT_STATE_TRANSFERRING) {
                uploadedFile->setText(UPLOAD_SPEED_COLUMN, QString::number(currentFriendInfo.transferRate, 'f', 2).append(" K/s"));
                uploadedFile->setText(UPLOAD_STATUS_COLUMN, tr("Uploading"));
                /* Also update the running total for the status bar. */
                upTotalRate += currentFriendInfo.transferRate;
            }
            uploadedFile->setText(UPLOAD_LIBRARYMIXER_ID, QString::number(currentFriendInfo.librarymixer_id));
            uploadedFile->setText(UPLOAD_ITEM_TYPE, UPLOAD_FILE_TYPE);
            transfers.append(uploadedFile);
        }
    }

    /* Put pending suggestions into uploadsList box. */
    QList<pendingSuggest> pendingSuggestions;
    files->getPendingSuggestions(pendingSuggestions);

    foreach (pendingSuggest currentSuggest, pendingSuggestions) {
        QTreeWidgetItem *currentSuggestItem = new QTreeWidgetItem();
        currentSuggestItem->setText(UPLOAD_FILE_COLUMN, currentSuggest.title);
        currentSuggestItem->setText(UPLOAD_FRIEND_COLUMN, peers->getPeerName(currentSuggest.friend_id));
        currentSuggestItem->setText(UPLOAD_STATUS_COLUMN, "Queued for when friend comes online");
        currentSuggestItem->setText(UPLOAD_LIBRARYMIXER_ID, QString::number(currentSuggest.friend_id));
        currentSuggestItem->setText(UPLOAD_ITEM_TYPE, UPLOAD_PENDING_TYPE);
        currentSuggestItem->setText(UPLOAD_ITEM_ID, QString::number(currentSuggest.uniqueSuggestionId));
        foreach (QString file, currentSuggest.files) {
            QTreeWidgetItem *currentFileItem = new QTreeWidgetItem(currentSuggestItem);
            currentFileItem->setText(UPLOAD_FILE_COLUMN, file);
            currentFileItem->setText(UPLOAD_LIBRARYMIXER_ID, QString::number(currentSuggest.friend_id));
            currentFileItem->setText(UPLOAD_ITEM_TYPE, UPLOAD_PENDING_FILE_TYPE);
            currentFileItem->setText(UPLOAD_ITEM_ID, QString::number(currentSuggest.uniqueSuggestionId));
        }
        transfers.append(currentSuggestItem);
    }

    ui.uploadsList->insertTopLevelItems(0, transfers);
    ui.uploadsList->expandAll();
    ui.uploadsList->update();
    ui.uploadsList->resizeColumnToContents(UPLOAD_FILE_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_FRIEND_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_SPEED_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_TRANSFERRED_COLUMN);
    ui.uploadsList->resizeColumnToContents(UPLOAD_STATUS_COLUMN);
    transfers.clear();

    return upTotalRate;
}

void TransfersDialog::responseLendOfferReceived(unsigned int friend_id, unsigned int item_id, QString title, QStringList paths, QStringList hashes, QList<qlonglong>filesizes) {
    QString friend_name = peers->getPeerName(friend_id);
    QString message = friend_name +
                      " will lend " +
                      title +
                      " to you, but wants it back at some point. Continue?";
    if (QMessageBox::question(this, title, message, QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
        files->borrowFiles(friend_id, title, paths, hashes, filesizes, FILE_HINTS_ITEM, QString::number(item_id));
        mainwindow->peersDialog->insertUserOptional(friend_id, NotifyBase::NOTIFY_USER_BORROW_ACCEPTED, title);
    } else {
        mainwindow->peersDialog->insertUserOptional(friend_id, NotifyBase::NOTIFY_USER_BORROW_DECLINED, title);
    }
}

void TransfersDialog::downloadsListContextMenu(QPoint point) {
    QMenu contextMenu(this);

    //Store variables needed in case cancel is called
    //If the hash column is empty, we're dealing with a pending request (or an empty row), so get what we need to cancel that.
    //Otherwise, get the hash which we can use to cancel a file transfer.
    QTreeWidgetItem *contextItem = ui.downloadsList->itemAt(point);
    if (contextItem != NULL) {
        if (!contextItem->text(DOWNLOAD_FRIEND_ID).isEmpty()) {
            context_friend_id = contextItem->text(DOWNLOAD_FRIEND_ID).toInt();
            context_friend_name = contextItem->text(DOWNLOAD_FRIEND_COLUMN);
            QAction *chatAct = new QAction(QIcon(IMAGE_CHAT), "Chat", &contextMenu);
            if (!peers->isOnline(context_friend_id)){
                chatAct->setEnabled(false);
                chatAct->setText(tr("Chat (friend offline)"));
            }
            connect(chatAct, SIGNAL(triggered()), this, SLOT(chat()));
            contextMenu.addAction(chatAct);
        }

        contextMenu.addSeparator();

        context_name = contextItem->text(DOWNLOAD_NAME_COLUMN);
        context_parent = (contextItem->parent() == NULL) ? "" : contextItem->parent()->text(DOWNLOAD_ITEM_ID);
        context_item_type = contextItem->text(DOWNLOAD_ITEM_TYPE);
        context_item_id = contextItem->text(DOWNLOAD_ITEM_ID);
        context_item_location = "";

        if (context_item_type == DOWNLOAD_GROUP_TYPE ||
            context_item_type == DOWNLOAD_FILE_TYPE ||
            context_item_type == DOWNLOAD_PENDING_TYPE) {
            if (contextItem->text(DOWNLOAD_STATUS_COLUMN) != "Complete") {
                QAction *cancelAct = new QAction(QIcon(IMAGE_CANCEL), tr("Cancel"), &contextMenu);
                connect(cancelAct, SIGNAL(triggered()), this, SLOT(cancel()));
                contextMenu.addAction(cancelAct);
            } else {
                context_item_location = contextItem->text(DOWNLOAD_FINAL_LOCATION);
                QAction *openAct = new QAction(QIcon(IMAGE_OPEN), tr("Open"), &contextMenu);
                connect(openAct, SIGNAL(triggered()), this, SLOT(openFile()));
                contextMenu.addAction(openAct);

                QAction *openContainingAct = new QAction(QIcon(IMAGE_OPENCONTAINING), tr("Open Downloads Folder"), &contextMenu);
                connect(openContainingAct, SIGNAL(triggered()), this, SLOT(openContaining()));
                contextMenu.addAction(openContainingAct);
            }
        }

        if (context_item_type == DOWNLOAD_BORROW_TYPE) {
            QAction *returnAct = new QAction(QIcon(IMAGE_SENDFILE), tr("Return files"), &contextMenu);
            connect(returnAct, SIGNAL(triggered()), this, SLOT(returnFiles()));
            contextMenu.addAction(returnAct);

            QAction *cancelAct = new QAction(QIcon(IMAGE_CANCEL), tr("Remove"), &contextMenu);
            connect(cancelAct, SIGNAL(triggered()), this, SLOT(cancel()));
            contextMenu.addAction(cancelAct);
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
        context_item_location = contextItem->text(UPLOAD_FILE_COLUMN);
        context_item_type = contextItem->text(UPLOAD_ITEM_TYPE);

        if (context_item_type == UPLOAD_FILE_TYPE) {
            if (!contextItem->text(UPLOAD_LIBRARYMIXER_ID).isEmpty()) {
                context_friend_id = contextItem->text(UPLOAD_LIBRARYMIXER_ID).toInt();
                QAction *chatAct = new QAction(QIcon(IMAGE_CHAT), "Chat", &contextMenu);
                connect(chatAct, SIGNAL(triggered()), this, SLOT(chat()));
                contextMenu.addAction(chatAct);
                if (!peers->isOnline(context_friend_id)){
                    chatAct->setEnabled(false);
                    chatAct->setText(tr("Chat (friend offline)"));
                }

                contextMenu.addSeparator();
            }

            QAction *openAct = new QAction(QIcon(IMAGE_OPEN), tr("Open"), &contextMenu);
            connect(openAct, SIGNAL(triggered()), this, SLOT(openFile()));
            contextMenu.addAction(openAct);

            contextMenu.addSeparator();
        }

        if (context_item_type == UPLOAD_PENDING_TYPE ||
            context_item_type == UPLOAD_PENDING_FILE_TYPE) {
            context_item_id = contextItem->text(UPLOAD_ITEM_ID);
            context_name = (contextItem->parent() == NULL) ? contextItem->text(UPLOAD_FILE_COLUMN) : contextItem->parent()->text(UPLOAD_FILE_COLUMN);

            QAction *cancelAct = new QAction(QIcon(IMAGE_CANCEL), tr("Cancel"), &contextMenu);
            connect(cancelAct, SIGNAL(triggered()), this, SLOT(cancel()));
            contextMenu.addAction(cancelAct);

            contextMenu.addSeparator();
        }
    }

    QAction *clearAct = new QAction(QIcon(IMAGE_CLEARCOMPLETED), tr("Clear Uploads"), &contextMenu);
    connect(clearAct , SIGNAL(triggered()), this, SLOT(clearUploads()));
    contextMenu.addAction(clearAct);

    contextMenu.exec(ui.uploadsList->mapToGlobal(point));
}

void TransfersDialog::cancel() {
    if (context_item_type == DOWNLOAD_GROUP_TYPE ||
        context_item_type == DOWNLOAD_FILE_TYPE ||
        context_item_type == DOWNLOAD_PENDING_TYPE) {
        QString message("Are you sure that you want to cancel and delete:\n");
        message.append(context_name);
        if ((QMessageBox::question(this, tr("Cancel Download"), message,
                                   QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)) == QMessageBox::Yes) {
            if (context_item_type == DOWNLOAD_GROUP_TYPE) {
                files->cancelDownloadGroup(context_item_id.toInt());
            } else if (context_item_type == DOWNLOAD_FILE_TYPE) {
                files->cancelFile(context_parent.toInt(), context_item_id);
            } else if (context_item_type == DOWNLOAD_PENDING_TYPE) {
                files->LibraryMixerRequestCancel(context_item_id.toInt());
            }
        }
    } else if (context_item_type == DOWNLOAD_BORROW_TYPE) {
        if ((QMessageBox::question(this, tr("Cancel Download"),
                                   "Are you sure you never want to return " + context_name +
                                   " to " + peers->getPeerName(context_friend_id) +
                                   "?\nRemoving something from the borrowed list can't be undone.",
                                   QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)) == QMessageBox::Yes) {
            files->deleteBorrowed(context_item_id);
        }
    } else if (context_item_type == UPLOAD_PENDING_TYPE ||
               context_item_type == UPLOAD_PENDING_FILE_TYPE) {
        if ((QMessageBox::question(this, tr("Cancel Send"),
                                   "Are you sure you want to cancel the queued sending of " + context_name +
                                   " to " + peers->getPeerName(context_friend_id) + "?",
                                   QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)) == QMessageBox::Yes) {
            files->removeSavedSuggestion(context_item_id.toUInt());
        }
    }
}

void TransfersDialog::openFile() {
    QFileInfo info(context_item_location);
    QDesktopServices::openUrl(QUrl("file:///" + info.absoluteFilePath(), QUrl::TolerantMode));
}

void TransfersDialog::openContaining() {
    QFileInfo info(context_item_location);
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

void TransfersDialog::returnFiles() {
    if (context_friend_id != 0) {
        QStringList paths = QFileDialog::getOpenFileNames(this, "It's easier to open a chat with " + context_friend_name + " and just drag in and drop files, but you can also do it this way");
        if (!paths.empty()) {
            files->returnFiles(context_name, paths, context_friend_id, context_item_id);
            //We must construct our own QMessageBox rather than use a static function in order to avoid making a system sound on window popup
            QMessageBox confirmBox(this);
            confirmBox.setIconPixmap(QPixmap(IMAGE_SENDFILE));
            confirmBox.setWindowTitle(context_name);
            confirmBox.setText("A return offer will be sent to " + context_friend_name);
            confirmBox.addButton("OK", QMessageBox::RejectRole);
            confirmBox.exec();
        }
    }
}
