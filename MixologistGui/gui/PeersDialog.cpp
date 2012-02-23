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


#include "interface/peers.h"
#include "interface/msgs.h"
#include "interface/notify.h"
#include "interface/librarymixer-connect.h"
#include "gui/PeersDialog.h"
#include "gui/Statusbar/peerstatus.h"
#include "gui/PopupChatDialog.h"
#include "gui/MainWindow.h" //for settings file location

/***
#define PEERS_DEBUG 1
***/

#ifdef PEERS_DEBUG
#include <iostream>
#endif


/* Images for context menu icons */
#define IMAGE_CHAT              ":/Images/Chat.png"
#define IMAGE_SENDFILE          ":/Images/Send.png"
#define IMAGE_CONNECT           ":/Images/ReconnectFriend.png"

/* Images for Status icons */
#define IMAGE_CONNECTED         ":/Images/StatusConnected.png"
#define IMAGE_CONNECTING        ":/Images/StatusConnecting.png"
#define IMAGE_OFFLINE           ":/Images/StatusOffline.png"

#define FRIEND_ICON_AND_SORT_COLUMN 0
#define FRIEND_NAME_COLUMN 1
#define FRIEND_STATUS_COLUMN 2
#define FRIEND_LIBRARYMIXER_ID_COLUMN 3

/** Constructor */
PeersDialog::PeersDialog(QWidget *parent)
    : QWidget(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    connect(ui.friendsList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(friendsListContextMenu(QPoint)));
    connect(ui.friendsList, SIGNAL(itemDoubleClicked (QTreeWidgetItem *, int)), this, SLOT(friendDoubleClicked()));
    connect(ui.addFriendsButton, SIGNAL(clicked()), this, SLOT(addFriendClicked()));
    connect(ui.updateFriendsButton, SIGNAL(clicked()), this, SLOT(updateFriends()));
    connect(librarymixerconnect, SIGNAL(downloadedFriends()), this, SLOT(updatedFriends()));

    /* Set header resize modes and initial section sizes */
    QHeaderView *_header = ui.friendsList->header();
    _header->resizeSection (FRIEND_ICON_AND_SORT_COLUMN, 28);
    _header->resizeSection (FRIEND_STATUS_COLUMN, 150);
    _header->setResizeMode (FRIEND_ICON_AND_SORT_COLUMN, QHeaderView::Fixed);
    _header->setResizeMode (FRIEND_NAME_COLUMN, QHeaderView::Stretch);
    _header->setResizeMode (FRIEND_STATUS_COLUMN, QHeaderView::Fixed);

    _header->hideSection(FRIEND_LIBRARYMIXER_ID_COLUMN);

    /*Can't figure out why, but when actually running the program (not visible in QT Creator preview)
      a number appears in the header of this column as the text. This gets rid of it.*/
    ui.friendsList->headerItem()->setText(FRIEND_ICON_AND_SORT_COLUMN, "");

    QTimer *timer = new QTimer(this);
    timer->connect(timer, SIGNAL(timeout()), this, SLOT(insertChat()));
    timer->start(500); /* half a second */

    ui.friendsList->sortItems(FRIEND_ICON_AND_SORT_COLUMN, Qt::AscendingOrder);

    ui.friendsList->resizeColumnToContents(FRIEND_NAME_COLUMN);
    ui.friendsList->resizeColumnToContents(FRIEND_STATUS_COLUMN);

    updateTimer = new QTimer(this);
    updateTimer->connect(updateTimer, SIGNAL(timeout()), this, SLOT(updateFriends()));
    resetUpdateTimer();
}

void PeersDialog::friendsListContextMenu(QPoint point) {
    QMenu contextMenu(this);

    QTreeWidgetItem *selection = ui.friendsList->itemAt(point);
    if (selection == NULL) return;

    int id = getFriendLibraryMixerId(selection);

    PeerDetails detail;
    if (!peers->getPeerDetails(id, detail)) return;

    chatAct = new QAction(QIcon(IMAGE_CHAT), tr("Chat"), this);
    connect(chatAct , SIGNAL(triggered()), this, SLOT(chatFriend()));
    contextMenu.addAction(chatAct);
    sendAct = new QAction(QIcon(IMAGE_SENDFILE), tr("Send File"), this);
    connect(sendAct , SIGNAL(triggered()), this, SLOT(sendFileFriend()));
    contextMenu.addAction(sendAct);

    if (!peers->isOnline(id)){
        chatAct->setEnabled(false);
        chatAct->setText(tr("Chat (friend offline)"));
        sendAct->setEnabled(false);
        sendAct->setText(tr("Send File (friend offline)"));
    }

    contextMenu.exec(ui.friendsList->mapToGlobal(point));
}

/* get the list of peers from the Interface.  */
void  PeersDialog::insertPeers() {
    std::list<int> peersList;
    std::list<int>::iterator it;

    if (!peers) {
        /* not ready yet! */
        return;
    }

    peers->getFriendList(peersList);

    /* get a link to the table */
    QTreeWidget *peerWidget = ui.friendsList;
    QTreeWidgetItem *selected = getCurrentPeer();
    QTreeWidgetItem *newSelect = NULL;

    unsigned int selected_librarymixer_id = 0;
    if (selected) selected_librarymixer_id = getFriendLibraryMixerId(selected);

    /* remove old items */
    peerWidget->clear();
    QList<QTreeWidgetItem *> items;
    for (it = peersList.begin(); it != peersList.end(); it++) {
        PeerDetails detail;
        if (!peers->getPeerDetails(*it, detail)) {
            continue; /* BAD */
        }

        /* make a widget per friend */
        QTreeWidgetItem *item = new QTreeWidgetItem(ui.friendsList, 0);

        /* add all the labels */
        item -> setTextColor(FRIEND_ICON_AND_SORT_COLUMN, Qt::transparent);

        item -> setText(FRIEND_NAME_COLUMN, detail.name);

        /* Hidden column: LibraryMixer ID */
        {
            item -> setText(FRIEND_LIBRARYMIXER_ID_COLUMN, QString::number(detail.librarymixer_id));
            if ((selected) && (selected_librarymixer_id == detail.librarymixer_id)) {
                newSelect = item;
            }
        }

        /* Input state information */
        int i;
        if (detail.state == PEER_STATE_CONNECTED) {
            for (i = 1; i <= 2; i++) {
                item -> setTextColor(i,(Qt::darkCyan));
                QFont font;
                font.setBold(true);
                item -> setFont(i,font);
                item -> setIcon(FRIEND_ICON_AND_SORT_COLUMN,(QIcon(IMAGE_CONNECTED)));
                item -> setText(FRIEND_ICON_AND_SORT_COLUMN, QString("1").append(detail.name.toLower()));
                item -> setText(FRIEND_STATUS_COLUMN, QString("Connected"));
            }
        } else if (detail.state == PEER_STATE_TRYING) {
            item -> setIcon(FRIEND_ICON_AND_SORT_COLUMN,(QIcon(IMAGE_CONNECTING)));
            item -> setText(FRIEND_ICON_AND_SORT_COLUMN, QString("2").append(detail.name.toLower()));
            item -> setText(FRIEND_STATUS_COLUMN, QString("Trying"));
        } else if (detail.state == PEER_STATE_WAITING_FOR_RETRY) {
            item -> setIcon(FRIEND_ICON_AND_SORT_COLUMN,(QIcon(IMAGE_CONNECTING)));
            item -> setText(FRIEND_ICON_AND_SORT_COLUMN, QString("2").append(detail.name.toLower()));
            QSettings settings(*mainSettings, QSettings::IniFormat, this);
            if (settings.value("Gui/ShowAdvanced", DEFAULT_SHOW_ADVANCED).toBool()) {
                item -> setText(FRIEND_STATUS_COLUMN, QString("Continuing"));
            } else {
                item -> setText(FRIEND_STATUS_COLUMN, QString("Trying"));
            }
        } else if (detail.state == PEER_STATE_NO_CERT) {
            item -> setText(FRIEND_ICON_AND_SORT_COLUMN, QString("4").append(detail.name.toLower()));
            item -> setText(FRIEND_STATUS_COLUMN, QString("Not signed up for the Mixologist"));
            item -> setTextColor(FRIEND_NAME_COLUMN, Qt::lightGray);
            item -> setTextColor(FRIEND_STATUS_COLUMN, Qt::lightGray);
        } else {
            item -> setIcon(FRIEND_ICON_AND_SORT_COLUMN,(QIcon(IMAGE_OFFLINE)));
            item -> setText(FRIEND_ICON_AND_SORT_COLUMN, QString("3").append(detail.name.toLower()));
            item -> setText(FRIEND_STATUS_COLUMN, QString("Offline"));
        }

        /* add to the list */
        items.append(item);

        /* If there is a chat window open for this peer, keep it updated on connection state. */
        std::map<int, PopupChatDialog *>::iterator chatit;
        chatit = chatDialogs.find(detail.librarymixer_id);
        if (chatit != chatDialogs.end()) chatit->second->setOnlineStatus(detail.state == PEER_STATE_CONNECTED);
    }

    /* add the items in */
    peerWidget->insertTopLevelItems(0, items);
    ui.friendsList->resizeColumnToContents(FRIEND_NAME_COLUMN);
    ui.friendsList->resizeColumnToContents(FRIEND_STATUS_COLUMN);
    if (newSelect) {
        peerWidget->setCurrentItem(newSelect);
    }

    //Update the status bar friend display and then update view.
    std::list<int> online_friends;
    peers->getOnlineList(online_friends);
    std::list<int> signed_up_friends;
    peers->getSignedUpList(signed_up_friends);

    mainwindow->peerstatus->setPeerStatus(online_friends.size(), signed_up_friends.size());
    peerWidget->update();
}

void PeersDialog::updateFriends() {
    ui.updateFriendsButton->setEnabled(false);
    if (librarymixerconnect->downloadFriends() < 0) updatedFriends();
    resetUpdateTimer();
}

void PeersDialog::updatedFriends() {
    ui.updateFriendsButton->setEnabled(true);
    peers->connectAll();
}

void PeersDialog::chatFriend() {
    QTreeWidgetItem *selection = getCurrentPeer();

    if (!selection) return;

    unsigned int librarymixer_id = getFriendLibraryMixerId(selection);

    PeerDetails detail;
    if (!peers->getPeerDetails(librarymixer_id, detail)) return;

    getOrCreateChat(librarymixer_id, true);
    return;
}

void PeersDialog::sendFileFriend() {
    QTreeWidgetItem *selection = getCurrentPeer();

    if (!selection) return;

    unsigned int librarymixer_id = getFriendLibraryMixerId(selection);

    PeerDetails detail;
    if (!peers->getPeerDetails(librarymixer_id, detail)) return;

    getOrCreateChat(librarymixer_id, true)->selectFiles();
    return;
}

void PeersDialog::updatePeerStatusString(unsigned int friend_librarymixer_id, const QString &status_string) {
    PopupChatDialog *pcd = getChat(friend_librarymixer_id, false);
    if (pcd != NULL) pcd->updateStatusString(status_string);
}

void PeersDialog::addFriendClicked() {
    QSettings settings(*startupSettings, QSettings::IniFormat);
    QString host = settings.value("MixologyServer", DEFAULT_MIXOLOGY_SERVER).toString();

    if (host.compare(DEFAULT_MIXOLOGY_SERVER, Qt::CaseInsensitive) == 0){
        host = DEFAULT_MIXOLOGY_SERVER_FIXED_VALUE;
    }

    QDesktopServices::openUrl(QUrl(host + "/friends"));

    QTimer::singleShot(2000, this, SLOT(addFriendClickedComplete()));
}

void PeersDialog::addFriendClickedComplete() {
    /* We don't just use the static QMessageBox::information box because, at least on Windows, it makes the annoying system beep when it pops up. */
    QMessageBox msgBox;
    msgBox.setWindowTitle("The Mixologist");
    msgBox.setText("<p>Hit ok when you are done making changes to your friends list to update the Mixologist.</p><p>(Or you can always hit Update Friends List in the upper-right later.)</p>");
    msgBox.exec();

    updateFriends();
}

void PeersDialog::friendDoubleClicked() {
    QTreeWidgetItem *selection = getCurrentPeer();
    if (!selection) return;

    int id = getFriendLibraryMixerId(selection);

    PeerDetails detail;
    if (!peers->getPeerDetails(id, detail)) return;
    if (detail.state == PEER_STATE_CONNECTED) chatFriend();
    else return;
}

void PeersDialog::insertChat() {
    if (!msgs->chatAvailable()) {
        return;
    }

    std::list<ChatInfo> newchat;
    if (!msgs->getNewChat(newchat)) {
        return;
    }

    std::list<ChatInfo>::iterator it;

    /* add in lines at the bottom */
    for (it = newchat.begin(); it != newchat.end(); it++) {
        if (it->chatflags & CHAT_PRIVATE) {
            bool isNewChat;
            PopupChatDialog *pcd = getOrCreateChat(peers->findLibraryMixerByCertId(it->rsid), false, &isNewChat);
            if (isNewChat) {
                pcd->show();
            }
            QApplication::alert(pcd, 3000);
            pcd->addMsgFromFriend(&(*it));
            continue;
        }

    }
}

PopupChatDialog *PeersDialog::getChat(unsigned int librarymixer_id, bool top){
    std::map<int, PopupChatDialog *>::iterator it;
    if (chatDialogs.end() != (it = chatDialogs.find(librarymixer_id))) {
        if (top) {
            it->second->show();
            it->second->raise();
        }
        return it->second;
    }
    return NULL;
}

PopupChatDialog *PeersDialog::createChat(unsigned int librarymixer_id, bool top){
    PopupChatDialog *popupchatdialog = new PopupChatDialog(librarymixer_id);
    connect(popupchatdialog, SIGNAL(closeChat(unsigned int)), this, SLOT(removeChat(unsigned int)));
    chatDialogs[librarymixer_id] = popupchatdialog;
    if (top) {
        popupchatdialog->show();
        popupchatdialog->raise();
    }
    return popupchatdialog;
}

PopupChatDialog *PeersDialog::getOrCreateChat(unsigned int librarymixer_id, bool top, bool *newChat){
    PopupChatDialog *popupchatdialog = NULL;
    popupchatdialog = getChat(librarymixer_id, top);
    if (popupchatdialog != NULL){
        if (newChat != NULL) *newChat = false;
    } else {
        popupchatdialog = createChat(librarymixer_id, top);
        if (newChat != NULL) *newChat = true;
    }
    return popupchatdialog;
}

void PeersDialog::insertRequestEvent(int event, unsigned int librarymixer_id, unsigned int item_id) {
    getOrCreateChat(librarymixer_id, true)->insertRequestEvent(event, item_id);
}

void PeersDialog::insertTransferEvent(int event, unsigned int librarymixer_id, const QString &transfer_name, const QString &extra_info) {
    getOrCreateChat(librarymixer_id, true)->insertTransferEvent(event, transfer_name, extra_info);
}

void PeersDialog::insertUserOptional(unsigned int librarymixer_id, int code, QString message) {
    PopupChatDialog* chat = getChat(librarymixer_id, false);
    if (chat != NULL) chat->insertUserOptional(code, message);
}

/* Utility Fns */
QString PeersDialog::getPeerName(QTreeWidgetItem *selection) {
    return selection -> text(FRIEND_NAME_COLUMN);
}

int PeersDialog::getFriendLibraryMixerId(QTreeWidgetItem *selection) {
    return (selection -> text(FRIEND_LIBRARYMIXER_ID_COLUMN)).toInt();
}

QTreeWidgetItem *PeersDialog::getCurrentPeer() {
    /* get a link to the table */
    QTreeWidget *peerWidget = ui.friendsList;
    QTreeWidgetItem *item = peerWidget -> currentItem();
    if (!item) return NULL;
    return item;
}

void PeersDialog::resetUpdateTimer(){
    int seconds = qrand() % 3600; //Random number of seconds between 0 and 1 hour
    seconds += 1800; //Seconds is now between 30 and 90 minutes
    updateTimer->start(seconds * 1000);
}

void PeersDialog::removeChat(unsigned int librarymixer_id) {
    chatDialogs.erase(librarymixer_id);
}
