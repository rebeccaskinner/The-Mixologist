/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006-2007, crypton
 *
 *  This file is part of The Mixologist.
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

#ifndef _PEERSDIALOG_H
#define _PEERSDIALOG_H

#include "ui_PeersDialog.h"
#include "interface/iface.h"

class QAction;
class PopupChatDialog;

class PeersDialog : public QWidget {
	Q_OBJECT

public:
	/** Default Constructor */
	PeersDialog(QWidget *parent = 0);
        /*Gets an existing chat, or creates a new one if one doesn't exist.
          If top is true, will unminimize (if applicable) the chat and pull it to the top.
          If provided with newChat, will set whether or not it is a new chat */
        PopupChatDialog *getOrCreateChat(unsigned int librarymixer_id, bool top, bool* newChat = NULL);


public slots:
        //Connected to notifyqt through main, event corresponds to a NotifyBase::transferEvent (in iface.h).
        //For incoming requests
        void insertRequestEvent(int event, unsigned int librarymixer_id, unsigned int item_id);
        //Connected to notifyqt through main, event corresponds to a NotifyBase::transferEvent (in iface.h).
        //Opens a chat window and passes the transfer event to it.
        void insertTransferEvent(int event, unsigned int librarymixer_id, const QString& transfer_name, const QString& extra_info);
        //Passes along optional information to a chat window if it already exists, or else does nothing if it doesn't.
        void insertUserOptional(unsigned int librarymixer_id, int code, QString message);
        //Connected to notifyqt through main, updates the friends list.
        void insertPeers();
        /*Connected to a button that updates friend list from server, and also connected through main for when
          there is a connection attempt by an unknown peer.*/
        void updateFriends();
        /* Connected through main, updates the status string of a chat. */
        void updatePeerStatusString(unsigned int friend_librarymixer_id, const QString& status_string) ;

private slots:
        /* Opens the Mixology server (generally LibraryMixer) in the browser to manage friends. */
        void addFriendClicked();
        /* addFriendClicked sets a timer before popping up a box, this finishes adding the action. */
        void addFriendClickedComplete();
        //Connected to double click event
        void friendDoubleClicked();
        //Connected to a timer in constructor to update chat status.
        void insertChat();
        //On completion of updateFriends(), updates GUI.
	void updatedFriends();
        /* Create the context popup menu and it's submenus */
	void friendsListContextMenu( QPoint point );
        /* Used to start a chat with a friend in response to clicking on the friend */
        void chatFriend();
        /* Used to start a file send with a friend in response to clicking on the friend */
        void sendFileFriend();
        /* Connected to the closed window slot of a chatDialog, removes it from the chatDialogs map */
        void removeChat(unsigned int librarymixer_id);

private:
        /*If a PopupChatDialog is already open with the friend, returns it, otherwise returns NULL.
          If top is true, will unminimize (if applicable) the chat and pull it to the top.*/
        PopupChatDialog *getChat(unsigned int librarymixer_id, bool top);
        /*Creates a new chat with the friend.
          If top is true, will unminimize (if applicable) the chat and pull it to the top.
          Be careful with this, as it will overwrite (but not free the memory for) any existing
          chat window in the chatDialogs list. Always call getChat first.*/
        PopupChatDialog *createChat(unsigned int librarymixer_id, bool top);

        /* Utility Fns */
        /* Returns the name of the person clicked on */
        QString getPeerName(QTreeWidgetItem *selection);
        /* Returns the LibraryMixer ID of the person clicked on */
        int getFriendLibraryMixerId(QTreeWidgetItem *selection);
        /* Just pulls the current selected item from the table */
        QTreeWidgetItem *getCurrentPeer();
        /* Resets the update timer's count to a new random value */
        void resetUpdateTimer();

        /* Defines the actions for the context menu */
	QAction* chatAct;
        QAction* sendAct;
        QAction* connectfriendAct;

        //This is used to update the friends list periodically
        QTimer* updateTimer;

	QTreeWidget *friendsList;
        //This holds a list of all chats, and is a map on librarymixer ids
        std::map<int, PopupChatDialog *> chatDialogs;

	/** Qt Designer generated object */
	Ui::PeersDialog ui;
};
#endif
