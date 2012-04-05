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
#include "interface/notifyqt.h"
#include "interface/librarymixer-connect.h"
#include "gui/PeersDialog.h"
#include "gui/PopupChatDialog.h"
#include "gui/MainWindow.h" //for settings file location

#define FRIEND_ICON_AND_SORT_COLUMN 0
#define FRIEND_NAME_COLUMN 1
#define FRIEND_STATUS_COLUMN 2
#define FRIEND_LIBRARYMIXER_ID_COLUMN 3

PeersDialog::PeersDialog(QWidget *parent)
    : QWidget(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    connect(ui.friendsList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(friendsListContextMenu(QPoint)));
    connect(ui.friendsList, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(friendDoubleClicked()));
    connect(ui.addFriendsButton, SIGNAL(clicked()), this, SLOT(addFriendClicked()));
    connect(ui.updateFriendsButton, SIGNAL(clicked()), this, SLOT(updateFriends()));
    connect(librarymixerconnect, SIGNAL(downloadedFriends()), this, SLOT(updatedFriends()), Qt::QueuedConnection);
    connect(peers, SIGNAL(ownConnectionReadinessChanged(bool)), this, SLOT(connectionReadinessChanged(bool)), Qt::QueuedConnection);

    /* To display chat status in chat windows. */
    connect(guiNotify, SIGNAL(chatStatusChanged(unsigned int, QString)), this, SLOT(updatePeerStatusString(unsigned int, QString)));
    /* To popup a chat box on incoming requests where a chat window is needed. */
    connect(guiNotify, SIGNAL(requestEventOccurred(int,unsigned int,unsigned int)), this, SLOT(insertRequestEvent(int,unsigned int,unsigned int)));
    /* To popup a chat box on transfer events where the friend's response requests chatting or indicates an error. */
    connect(guiNotify, SIGNAL(transferChatEventOccurred(int,unsigned int,QString,QString)), this, SLOT(insertTransferEvent(int,unsigned int,QString,QString)));
    connect(guiNotify, SIGNAL(userOptionalInfo(unsigned int,int,QString)), this, SLOT(insertUserOptional(unsigned int,int,QString)));

    /* Check for new chats every half a second. */
    QTimer *timer = new QTimer(this);
    timer->connect(timer, SIGNAL(timeout()), this, SLOT(insertChat()));
    timer->start(500); /* 500 milliseconds */

    /* Set up the columns */
    /* This makes it so the icon column has a fixed size, the status column will be dynamically resized to the contents,
       and the bulk of the space will be taken by the name column. */
    ui.friendsList->header()->setResizeMode(FRIEND_ICON_AND_SORT_COLUMN, QHeaderView::Fixed);
    ui.friendsList->header()->setResizeMode(FRIEND_NAME_COLUMN, QHeaderView::Stretch);
    ui.friendsList->header()->setResizeMode(FRIEND_STATUS_COLUMN, QHeaderView::Fixed);

    ui.friendsList->header()->hideSection(FRIEND_LIBRARYMIXER_ID_COLUMN);

    /* Can't figure out why, but when actually running the program (not visible in QT Creator preview) a number appears in the header of this column as the text.
       This gets rid of it. */
    ui.friendsList->headerItem()->setText(FRIEND_ICON_AND_SORT_COLUMN, "");

    /* Set up the default sort. */
    ui.friendsList->sortItems(FRIEND_ICON_AND_SORT_COLUMN, Qt::AscendingOrder);

    /* We will fill the main friendsList box with a placeholder letting users know that the connection is initializing. */
    QList<QTreeWidgetItem *> initialItems;
    QTreeWidgetItem *placeholderItem = new QTreeWidgetItem(ui.friendsList, 0);
    placeholderItem->setText(FRIEND_ICON_AND_SORT_COLUMN, "\n\n\nThe Mixologist is initializing your connection, you'll be ready to connect to your friends soon!");
    placeholderItem->setTextColor(FRIEND_ICON_AND_SORT_COLUMN, Qt::lightGray);
    placeholderItem->setTextAlignment(FRIEND_ICON_AND_SORT_COLUMN, Qt::AlignHCenter);
    /* We set the size of the item to be 50,000, an arbitrary number that should be large enough that it completely fills any screen. */
    placeholderItem->setSizeHint(FRIEND_ICON_AND_SORT_COLUMN, QSize(50000, 50000));
    initialItems.append(placeholderItem);
    /* We disallow selections of the big placeholder. */
    ui.friendsList->setSelectionMode(QAbstractItemView::NoSelection);
    ui.friendsList->insertTopLevelItems(0, initialItems);
    /* We don't need to show the header for this. */
    ui.friendsList->header()->hide();
    /* We hide the extraneous columns so we can easily resize the column we're using to max width. */
    ui.friendsList->hideColumn(FRIEND_NAME_COLUMN);
    ui.friendsList->hideColumn(FRIEND_STATUS_COLUMN);
    ui.friendsList->header()->setResizeMode(FRIEND_ICON_AND_SORT_COLUMN, QHeaderView::Stretch);
}

void PeersDialog::friendsListContextMenu(QPoint point) {
    QMenu contextMenu(this);

    QTreeWidgetItem *selection = ui.friendsList->itemAt(point);
    if (selection == NULL) return;

    int id = getFriendLibraryMixerId(selection);

    chatAct = new QAction(QIcon(":/Images/Chat.png"), tr("Chat"), this);
    connect(chatAct , SIGNAL(triggered()), this, SLOT(chatFriend()));
    contextMenu.addAction(chatAct);
    sendAct = new QAction(QIcon(":/Images/Send.png"), tr("Send File"), this);
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

#define DEFAULT_SORT_PRIORITY_ONLINE '1'
#define DEFAULT_SORT_PRIORITY_SEARCHING '2'
#define DEFAULT_SORT_PRIORITY_NOT_SIGNED_UP '3'

void  PeersDialog::insertPeers() {
    if (!peers) return;

    QList<unsigned int> peersList;
    peers->getFriendList(peersList);

    /* Save the current selection so it isn't lost when we update the view. */
    QTreeWidgetItem *selected = getCurrentPeer();
    QTreeWidgetItem *newSelect = NULL;

    unsigned int selected_librarymixer_id = 0;
    if (selected) selected_librarymixer_id = getFriendLibraryMixerId(selected);

    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    bool showAdvanced = settings.value("Gui/ShowAdvanced", DEFAULT_SHOW_ADVANCED).toBool();

    /* Must be done before we create the new QTreeWidgetItems that will be parented to the friendsList. */
    ui.friendsList->clear();

    QList<QTreeWidgetItem *> items;
    foreach (unsigned int librarymixer_id, peersList) {
        PeerDetails detail;
        if (!peers->getPeerDetails(librarymixer_id, detail)) continue;

        /* make a widget per friend */
        QTreeWidgetItem *item = new QTreeWidgetItem(ui.friendsList, 0);

        /* add all the labels */
        item->setTextColor(FRIEND_ICON_AND_SORT_COLUMN, Qt::transparent);

        item->setText(FRIEND_NAME_COLUMN, detail.name);

        /* Hidden column: LibraryMixer ID */
        {
            item->setText(FRIEND_LIBRARYMIXER_ID_COLUMN, QString::number(detail.librarymixer_id));
            if ((selected) && (selected_librarymixer_id == detail.librarymixer_id)) {
                newSelect = item;
            }
        }

        /* Input state information */
        int i;
        if (detail.state == FCS_CONNECTED_TCP ||
            detail.state == FCS_CONNECTED_UDP) {
            for (i = 1; i <= 2; i++) {
                item->setTextColor(i,(Qt::darkCyan));
                QFont font;
                font.setBold(true);
                item->setFont(i,font);
            }
            item->setIcon(FRIEND_ICON_AND_SORT_COLUMN,(QIcon(":/Images/StatusConnected.png")));
            item->setText(FRIEND_ICON_AND_SORT_COLUMN, DEFAULT_SORT_PRIORITY_ONLINE + detail.name.toLower());
            item->setText(FRIEND_STATUS_COLUMN, QString("Connected "));
            if (detail.state == FCS_CONNECTED_UDP) {
                if (showAdvanced) {
                    item->setText(FRIEND_STATUS_COLUMN, QString("Connected (UDP) "));
                    item->setToolTip(FRIEND_STATUS_COLUMN,
                                     QString("UDP connections are slower, less reliable, and less efficient than regular connections. ") +
                                     "You should only ever see these if you are behind a firewall.");
                }
            }
        } else if (detail.state == FCS_NOT_MIXOLOGIST_ENABLED) {
            item->setText(FRIEND_ICON_AND_SORT_COLUMN, DEFAULT_SORT_PRIORITY_NOT_SIGNED_UP + detail.name.toLower());
            item->setText(FRIEND_STATUS_COLUMN, QString("Not signed up for the Mixologist "));
            item->setTextColor(FRIEND_NAME_COLUMN, Qt::lightGray);
            item->setTextColor(FRIEND_STATUS_COLUMN, Qt::lightGray);
        } else {
            item->setText(FRIEND_ICON_AND_SORT_COLUMN, DEFAULT_SORT_PRIORITY_SEARCHING + detail.name.toLower());
            item->setText(FRIEND_STATUS_COLUMN, QString("Searching for friend...      "));
        }

        /* add to the list */
        items.append(item);

        /* If there is a chat window open for this peer, keep it updated on connection state. */
        std::map<int, PopupChatDialog *>::iterator chatit;
        chatit = chatDialogs.find(detail.librarymixer_id);
        if (chatit != chatDialogs.end()) {
            chatit->second->setOnlineStatus(detail.state == FCS_CONNECTED_TCP ||
                                            detail.state == FCS_CONNECTED_UDP);
        }
    }

    /* add the items in */
    ui.friendsList->insertTopLevelItems(0, items);

    ui.friendsList->resizeColumnToContents(FRIEND_STATUS_COLUMN);
    if (newSelect) ui.friendsList->setCurrentItem(newSelect);

    ui.friendsList->update();

    static QMovie movie(":/Images/StatusSearching.gif");
    movie.start();
    movie.setSpeed(100);
    foreach (QTreeWidgetItem *item, items) {
        if (item->text(FRIEND_ICON_AND_SORT_COLUMN).startsWith(DEFAULT_SORT_PRIORITY_SEARCHING)) {
            QLabel *movieLabel = new QLabel();
            /* Our movie is 24 pixels wide. However, unlike the graphic icons, QT doesn't automatically insert an unremoveable margin around it.
               Therefore, we need to insert a comparable margin ourself.
               Unfortunately, setting a margin seems to introduce the graphical glitching visible on the animation. */
            movieLabel->setMargin(3);
            movieLabel->setMovie(&movie);
            ui.friendsList->setItemWidget(item, FRIEND_ICON_AND_SORT_COLUMN, movieLabel);
        }
    }
}

void PeersDialog::updateFriends() {
    ui.updateFriendsButton->setEnabled(false);
    if (librarymixerconnect->downloadFriends() < 0) {
        updatedFriends();
    }
}

void PeersDialog::updatedFriends() {
    /* We only want to call connect all if the friend list download was triggered by this button. */
    if (!ui.updateFriendsButton->isEnabled()) {
        ui.updateFriendsButton->setEnabled(true);
        peers->connectAll();
    }
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

void PeersDialog::connectionReadinessChanged(bool ready) {
    if (ready) {
        /* Now that the connection is ready, we can begin displaying the friends list. */
        QObject::connect(guiNotify, SIGNAL(friendsChanged()), this, SLOT(insertPeers()));

        insertPeers();
        /* Undo all the tweaks we did to set the placeholder text. */
        ui.friendsList->showColumn(FRIEND_NAME_COLUMN);
        ui.friendsList->showColumn(FRIEND_STATUS_COLUMN);
        ui.friendsList->header()->setResizeMode(FRIEND_ICON_AND_SORT_COLUMN, QHeaderView::Custom);
        /* For some reason, when inserting an icon into the friendsList, a 4 pixel margin in inserted, and I can't find any way to disable it.
           Therefore, in order to avoid the icon being cut off, we make the column 30 pixels. */
        ui.friendsList->header()->resizeSection(FRIEND_ICON_AND_SORT_COLUMN, 30);
        ui.friendsList->setSelectionMode(QAbstractItemView::SingleSelection);
        ui.friendsList->header()->show();
    }
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
    if (detail.state == FCS_CONNECTED_TCP ||
        detail.state == FCS_CONNECTED_UDP) chatFriend();
    else return;
}

void PeersDialog::insertChat() {
    if (!msgs->chatAvailable()) return;

    QList<ChatInfo> newChats;
    if (!msgs->getNewChat(newChats)) return;

    foreach (ChatInfo info, newChats) {
        if (info.chatflags & CHAT_PRIVATE) {
            bool isNewChat;
            PopupChatDialog *pcd = getOrCreateChat(info.librarymixer_id, false, &isNewChat);
            if (isNewChat) pcd->show();
            QApplication::alert(pcd, 3000);
            pcd->addMsgFromFriend(&info);
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
    return selection->text(FRIEND_NAME_COLUMN);
}

int PeersDialog::getFriendLibraryMixerId(QTreeWidgetItem *selection) {
    return (selection->text(FRIEND_LIBRARYMIXER_ID_COLUMN)).toInt();
}

QTreeWidgetItem *PeersDialog::getCurrentPeer() {
    /* get a link to the table */
    QTreeWidget *peerWidget = ui.friendsList;
    QTreeWidgetItem *item = peerWidget->currentItem();
    if (!item) return NULL;
    return item;
}

void PeersDialog::removeChat(unsigned int librarymixer_id) {
    chatDialogs.erase(librarymixer_id);
}
