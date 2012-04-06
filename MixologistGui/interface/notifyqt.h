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

#ifndef NOTIFY_TXT_H
#define NOTIFY_TXT_H

#include "interface/notify.h"
#include <QObject>
#include <QStringList>

class NetworkDialog;
class PeersDialog;
class LibraryDialog;
class TransfersDialog;

/* This class served as an interface between the C++ library and the QT GUI.
   However, now that the library has been moved to QT, this should be phased out in favor of direct use of signals and slots. */

class NotifyQt;
extern NotifyQt *guiNotify;

class NotifyQt: public NotifyBase {
    Q_OBJECT
public:
    NotifyQt() {}

    virtual ~NotifyQt() {}

    //When there is a suggestion to download something from a friend
    virtual void notifySuggestion(unsigned int librarymixer_id, const QString &title, const QStringList &paths, const QStringList &hashes, const QList<qlonglong> &filesizes);
    //When there are failed requests from a friend
    virtual void notifyRequestEvent(transferEvent event, unsigned int librarymixer_id, unsigned int item_id = 0);
    //When there are responses from a friend on a requested transfer
    virtual void notifyTransferEvent(transferEvent event, unsigned int librarymixer_id, QString transfer_name, QString extra_info = "");
    //When there are status updates for chat buddies
    virtual void notifyChatStatus(unsigned int librarymixer_id, const QString& status_string);
    //When a file is being hashed
    virtual void notifyHashingInfo(QString fileinfo);
    //When there is something to stick in the log of the Network dialog
    virtual void notifyLog(QString message);
    //Something happened involving a user that is low priority, and only optionally should be displayed
    virtual void notifyUserOptional(unsigned int librarymixer_id, userOptionalCodes code, QString message);

signals:
    //When there is a suggestion to download something from a friend
    void suggestionReceived(unsigned int friend_id, QString title, QStringList paths, QStringList hashes, QList<qlonglong> filesizes);
    //When there are failed requests from a friend
    void requestEventOccurred(int event, unsigned int librarymixer_id, unsigned int item_id);
    //When there are responses from a friend on a requested transfer that require discussion
    void transferChatEventOccurred(int event, unsigned int librarymixer_id, QString transfer_name, QString extra_info);
    //When there are responses from a friend on a requested transfer that require a query
    void transferQueryEventOccurred(int event, unsigned int librarymixer_id, QString transfer_name, QString extra_info);
    //When a file is being hashed
    void hashingInfoChanged(QString fileinfo);
    //When transfers updated
    void transfersChanged();
    //When there are status updates for chat buddies
    void chatStatusChanged(unsigned int librarymixer_id, QString status_string);
    //When there is something to stick in the log of the Network dialog
    void logInfoChanged(QString message);
    //When there is optional information on a user
    void userOptionalInfo(unsigned int librarymixer_id, int code, QString message);

public slots:
    /* Displays a system message. */
    virtual void displaySysMessage(int type, QString title, QString msg);

    /* Displays a popup messages. */
    virtual void displayPopupMessage(int type, QString name, QString msg);

};

#endif
