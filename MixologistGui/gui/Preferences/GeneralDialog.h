/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006-2007, crypton
 *  Copyright 2006, Matt Edman, Justin Hipple
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

#ifndef _GENERALDIALOG_H
#define _GENERALDIALOG_H

#include "configpage.h"
#include "ui_GeneralDialog.h"


class GeneralDialog : public ConfigPage {
	Q_OBJECT

public:
	GeneralDialog(QWidget *parent = 0);
        ~GeneralDialog(){}
        /* Saves the changes on this page */
        bool save();

private slots:
        /* Immediately clears stored login information */
        void clearLogin();
        /* Set mixology: links to be associated with the Mixologist. */
        void AssociateMixologyLinks();
        /* Changes the display and cascades changes to all windows on showing advanced. */
        void showAdvanced(bool enable);

private:
        /* Called when the "show on startup" checkbox is toggled. */
        void toggleShowOnStartup(bool checked);
        /* Enables/disables the clearLogin button and sets its tooltip */
        void enableClearLogin(bool enable);
        /* Enables/disables the associateLinks button and sets its tooltip */
        void enableAssociateLinks(bool enable);
        /* Qt Designer generated object */
	Ui::GeneralDialog ui;


};

#endif //_GENERALDIALOG_H
