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

#include "ui_LibraryDialog.h"

class LibraryModel;

class LibraryDialog : public QWidget {
	Q_OBJECT

    public:
        /* Default Constructor */
        LibraryDialog(QWidget *parent = 0);

    private slots:
        /* Opens the Mixology server (generally LibraryMixer) in the browser to manage library. */
        void addLibraryClicked();
        /* addLibraryClicked sets a timer before popping up a box, this finishes the action. */
        void addLibraryClickedComplete();
        //Updates the library
        void updateLibrary();
        //Updates the UI that we are no longer updating the library
        void updatedLibrary();

    private:
        LibraryModel* unmatchedModel;
        LibraryModel* libraryModel;

	/** Qt Designer generated object */
        Ui::LibraryDialog ui;
};

#endif
