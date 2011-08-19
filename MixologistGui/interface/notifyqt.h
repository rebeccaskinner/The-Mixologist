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

#include "interface/iface.h"
#include <QObject>

#include <string>

class NetworkDialog;
class PeersDialog;
class LibraryDialog;
class TransfersDialog;

class NotifyQt: public QObject, public NotifyBase {
	Q_OBJECT
public:
        NotifyQt(){return;}

        virtual ~NotifyQt() {return;}

        //When there is a suggestion to download something from a friend
        virtual void notifySuggestion(int librarymixer_id, int item_id, QString name);
        //When there are failed requests from a friend
        virtual void notifyRequestEvent(transferEvent event, int librarymixer_id, int item_id = 0);
        //When there are responses from a friend on a requested transfer
        virtual void notifyTransferEvent(transferEvent event, int librarymixer_id, QString transfer_name, QString extra_info = "");
        //When library list updated
        virtual void notifyLibraryUpdated();
        //When there are status updates for chat buddies
        virtual void notifyChatStatus(int librarymixer_id, const QString& status_string);
        //When a file is being hashed
        virtual void notifyHashingInfo(QString fileinfo);
        //When there is something to stick in the log of the Network dialog
        virtual void notifyLog(QString message);
        //Something happened involving a user that is low priority, and only optionally should be displayed
        virtual void notifyUserOptional(int librarymixer_id, userOptionalCodes code, QString message);

signals:
	// It's beneficial to send info to the GUI using signals, because signals are thread-safe
	// as they get queued by Qt.

        //When there is a suggestion to download something from a friend
        void suggestionReceived(int librarymixer_id, int item_id, const QString& name);
        //When there are failed requests from a friend
        void requestEventOccurred(int, int, int);
        //When there are responses from a friend on a requested transfer that require discussion
        void transferChatEventOccurred(int, int, const QString&, const QString&);
        //When there are responses from a friend on a requested transfer that require a query
        void transferQueryEventOccurred(int, int, const QString&, const QString&);
        //When someone unrecognized tries to connect
        void connectionDenied();
        //When a file is being hashed
        void hashingInfoChanged(const QString&) const ;
        //When library list updated
        void libraryUpdated();
        //When transfers updated
	void transfersChanged() const ;
        //When friends updated
        void friendsChanged() const ;
        //When there are status updates for chat buddies
        void chatStatusChanged(int,const QString&) const ;
        //When there is something to stick in the log of the Network dialog
        void logInfoChanged(const QString&) const ;
        //When there is optional information on a user
        void userOptionalInfo(int librarymixer_id, int code, QString message);

public slots:
	void	UpdateGUI(); /* called by timer */
};

#endif
