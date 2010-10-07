/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006, crypton
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


#ifndef _PreferencesWindow_H
#define _PreferencesWindow_H

#include <QMainWindow>

#include "GeneralDialog.h"
#include "TransfersPrefDialog.h"
#include "ServerDialog.h"
#include "NotifyDialog.h"

#include "ui_PreferencesWindow.h"

class PreferencesWindow : public QMainWindow {
	Q_OBJECT

public:
	/** Default Constructor */
        PreferencesWindow(QWidget *parent = 0, Qt::WFlags flags = 0);
	/** Default destructor */
        ServerDialog* connectionDialog;
        /** Qt Designer generated object */
        Ui::PreferencesWindow ui;
        /* List of actions and their associated pages used to switch between pages. */
        QHash<QAction*, ConfigPage*> _pages;

protected:
	void closeEvent (QCloseEvent * event);

private slots:
        /** Displays the page associated with the activated action. */
        void showPage(QAction *pageAction);

	/** Called when user clicks "Save Settings" */
	void saveChanges();

	void cancelpreferences();

};

#endif

