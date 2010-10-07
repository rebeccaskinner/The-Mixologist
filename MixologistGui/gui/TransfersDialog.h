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

#ifndef _TRANSFERSDIALOG_H
#define _TRANSFERSDIALOG_H

#include "ui_TransfersDialog.h"


class DLListDelegate;
class ULListDelegate;
class QStandardItemModel;

class TransfersDialog : public QWidget {
	Q_OBJECT

public:
	/** Default Constructor */
	TransfersDialog(QWidget *parent = 0);
	/** Default Destructor */
        ~TransfersDialog(){}

public slots:
        //Pops up an input box for the user to enter a Mixology link.
        void download(const QString & link = "");
        //Connected to notifyqt through main, triggered when a file download suggestion is received.
        void suggestionReceived(int librarymixer_id, int item_id, const QString& name);
        //Connected to notifyqt throughg main, populates the view.
        void insertTransfers();
        //Connected to notifyqt through main, event corresponds to a NotifyBase::transferEvent (in iface.h).
        //Opens a dialog window to get further user input.
        void insertTransferEvent(int event, int librarymixer_id, const QString& transfer_name, const QString& extra_info);
        /* Opens the folder containing the file */
        void openContaining();

private slots:
        /* Create the context popup menu and it's submenus */
        void downloadsListContextMenu( QPoint point );
        /* Create the context popup menu and it's submenus */
        void uploadsListContextMenu( QPoint point );
        /* Cancels either a transfer or a pending request */
	void cancel();
        /* Removes finished downloads */
        void clearCompleted();
        /* Removes all from the uploads list */
        void clearUploads();
        /* Opens the file */
        void openFile();
        /* Chats with the friend indicated in context_friend_id */
        void chat();

private:
        /*Finds or creates a top level item in the list transfers with that item id.
          If it creates, uses name.*/
        QTreeWidgetItem* findOrCreateTransferItem(int item_id, QString name, QList<QTreeWidgetItem*> &transfers);
        //Info for keeping track of context menu actions
        QString context_name;
        //This stores the hash of the file that requested the context menu, or is necessarily null when a pending request requested the context menu
        std::string context_hash;
        int context_item_id;
        int context_friend_id;

        /** Qt Designer generated object */
	Ui::TransfersDialog ui;

};

#endif

