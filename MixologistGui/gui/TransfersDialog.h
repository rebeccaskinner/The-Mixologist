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
        void suggestionReceived(unsigned int librarymixer_id, QString title, QStringList paths, QStringList hashes, QList<qlonglong> filesizes);
        //Connected to notifyqt throughg main, populates the view.
        void insertTransfers();
        /* When a response is received from a request, and it is an offer to lend a set of files. */
        void responseLendOfferReceived(unsigned int friend_id, unsigned int item_id, QString title, QStringList paths, QStringList hashes, QList<qlonglong> filesizes);
        /* Opens the folder containing the file */
        void openContaining();

private slots:
        /* Create the context popup menu and it's submenus */
        void downloadsListContextMenu(QPoint point);
        /* Create the context popup menu and it's submenus */
        void uploadsListContextMenu(QPoint point);
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
        /* Opens a file browser for picking files to return for a borrow */
        void returnFiles();

private:
        /* Called by insertTransfers() to handle the downloads and uploads separately.
           Each return the total transfer rate as a float in that direction. */
        float insertDownloads();
        float insertUploads();

        /* Info for keeping track of context menu actions. */
        QString context_name; //display name of the item
        QString context_parent; //item id of the parent, only used for subitems
        QString context_item_type; //one of the pre-defined types allowed for display
        QString context_item_id; //meaning varies depending on the type
        QString context_item_location; //Only set for completed files and uploaded files
        int context_friend_id; //librarymixer_id of the relevant friend
        QString context_friend_name; //name of the relevant friend

        /** Qt Designer generated object */
	Ui::TransfersDialog ui;

};

#endif

