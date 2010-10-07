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


#ifndef _TRANSFERSPREFDIALOG_H
#define _TRANSFERSPREFDIALOG_H

#include "configpage.h"
#include "ui_TransfersPrefDialog.h"


class TransfersPrefDialog : public ConfigPage {
	Q_OBJECT

public:
        TransfersPrefDialog(QWidget *parent = 0);
        ~TransfersPrefDialog(){}
	/** Saves the changes on this page */
        bool save();

private slots:
        /** Used to prevent maximum total rate from being adjusted below maximum individual rate */
        void setMaxIndivDownloadRate(int);
        void setMaxIndivUploadRate(int);
        /** Browser for the downloads directory */
        void browseDownloadsDirectory();
        /** Browser for the partials directory */
        void browsePartialsDirectory();

private:
        /** Qt Designer generated object */
        Ui::TransfersPrefDialog ui;

};

#endif //_TRANSFERSPREFDIALOG_H
