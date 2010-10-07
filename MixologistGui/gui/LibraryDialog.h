/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2009, RetroShare Team
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

#ifndef _LIBRARYDIALOG_H
#define _LIBRARYDIALOG_H

#include <QMovie>

#include "ui_LibraryDialog.h"
#include "LibraryHelper.h"

class LibraryDialog : public QWidget {
	Q_OBJECT

    public:
	/** Default Constructor */
        LibraryDialog(QWidget *parent = 0);
	/** Default Destructor */

    public slots:
        void insertLibrary();

    private slots:
        //Updates the library
        void updateLibrary();
        //Updates the UI that we are no longer updating the library
        void updatedLibrary();
        //Opens a text dialog for the user to set an auto response message
        void setMatchToMessage();
        //Opens a file dialog to set files
        void setMatchToFiles();
        //Opens a file dialog to set files to lend
        void setMatchToLend();
        //Sets an item to chat, and never bother again about auto match
        void setMatchToChat();
        //Toggle an item set to file to lend
        void setFilesToLend();
        //Toggle an item set to lend to file
        void setLendToFiles();
        //Shows the help window
        void showHelp();
        //Chat with the borrower of a lent item
        void chatBorrower();
        //Opens the file on a matched or lend item
        void openFile();

        //Opens the context menu for unmatched items
        void unmatchedContextMenu(QPoint point);
        //Opens the context for matched items
        void matchedContextMenu(QPoint point);

    private:
        QMovie* movie;

        //Pointers for keeping track of context menu actions
        QTreeWidgetItem* contextItem;
        LibraryBox* contextBox;

	/** Qt Designer generated object */
        Ui::LibraryDialog ui;
};

#endif
