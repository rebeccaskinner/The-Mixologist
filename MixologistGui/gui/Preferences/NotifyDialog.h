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


#ifndef _NOTIFYDIALOG_H
#define _NOTIFYDIALOG_H

#include "configpage.h"
#include "ui_NotifyDialog.h"

class NotifyDialog : public ConfigPage {
	Q_OBJECT

public:
	/** Default Constructor */
	NotifyDialog(QWidget *parent = 0);
	/** Default Destructor */

	/** Saves the changes on this page */
        bool save();

private:
	/** Qt Designer generated object */
	Ui::NotifyDialog ui;
};

#endif

