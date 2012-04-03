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


#ifndef _POPUPCHATDIALOG_H
#define _POPUPCHATDIALOG_H

#include <QtGui>
#include <QDialog>

#include <ui_PopupChatDialog.h>
#include <interface/iface.h>

class QAction;
class QTextEdit;
class QTextCharFormat;

class ChatInfo;

//The amount of time to wait before adding a timestamp
#define TIMESTAMP_TIMEOUT_LENGTH 60000 //60 seconds

class PopupChatDialog : public QMainWindow {
	Q_OBJECT

public:
	/** Default constructor */
        PopupChatDialog(int _librarymixer_id, QWidget *parent = 0, Qt::WFlags flags = 0);
        //Adds a message from a friend
        void addMsgFromFriend(ChatInfo *ci);
        //Adds message text to the display of the PopupChatDialog with formatting for a system message.
        void addSysMsg(const QString& text);
        //Handles an incoming request event
        void insertRequestEvent(int event, unsigned int item_id);
        //Handles a response to an outgoing request
        void insertTransferEvent(int event, const QString& transfer_name, const QString& extra_info);
        /*Handles notifications of messages about the friend.
         These are considered "optional" because a window will not be popped up for them if it does not already exist.*/
        void insertUserOptional(int code, QString input);
        //Updates the chat window on the online status of the friend, and if status has changed, then prints a message about it
        void setOnlineStatus(bool status);

protected:
        //These two are used to handle drag and drop of files into window for send
        void dragEnterEvent(QDragEnterEvent *event);
        void dropEvent(QDropEvent *event);
        //This is used to handle sending a message when enter or return is pressed
        bool eventFilter(QObject *obj, QEvent *event);

signals:
        void closeChat(unsigned int librarymixer_id) const;

public slots:
        //Resets the statusbar to default.
	void resetStatusBar() ;
        //Called through notifyQt to display when the peer is typing.
        void updateStatusTyping() ;
        //Called through notifyQt to display the peer's status.
        void updateStatusString(const QString&) ;
        //Opens a window to select files to send to friend
        void selectFiles();

private slots:
        //Toggles the right hand avatar frame's visibility.
	void showAvatarFrame(bool show);
        //Checks the inputed text to make sure it does not exceed maximum length, and sends it if it ends in a \n
	void checkChat();
        //Sends the entered text
	void sendChat();

#ifdef false
        void getAvatar();
#endif
        //Reimplements the close event to send the closeChat signal.
        void closeEvent(QCloseEvent *event);

        //When there is a displayed request, these slots will match the item for future requests
        void setMatchToMessage();
        void setMatchToFiles();
        void setMatchToLend();
        void setMatchToChat();
        //Clears the requested item from the chat dialog
        void clearRequest();
        //Displays a timestamp in the chat window. Connected to time_stamp_timer;
        void addTimeStamp();

private:
#ifdef false
        //Updates the avatar of friend
        void updatePeerAvatar();
        //Updates own avatar
        void updateAvatar();
#endif

        /* Sends the files to friends.
           Called internally after a drag and drop of files or a selection of files. */
        void sendFiles(QStringList paths);

        //Displays a message from self or friend
        void addChatMsg(const QString &message, bool self);
        //Takes a message and transfers all of the contained urls into links
        void linkify(QString &message);
        //The most basic function for adding text to the display.
        //Called by all methods that wish to add text to the display with the preformatted text to display.
        void addText(QString &text);

        //Information on friend that user is chatting with
        unsigned int librarymixer_id;
        QString friendName;

        /*If there is a particular item that was requested by a friend, it shows up here
          requestedItemId is also used as a signal that the underlying item may be able to use a match to a file. */
        int requestedItemId;

        time_t last_status_send_time;

        /*Counts down each time we receive a message, and if we do not receive another message within 60 seconds, adds a timestamp.*/
        QTimer* time_stamp_timer;

        bool online_status;

	/** Qt Designer generated object */
	Ui::PopupChatDialog ui;


};

#endif
