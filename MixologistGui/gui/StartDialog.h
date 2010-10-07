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


#ifndef _STARTDIALOG_H
#define _STARTDIALOG_H

#include "interface/librarymixer-connect.h"
#include "interface/notifyqt.h"
#include "interface/settings.h"

#include "ui_StartDialog.h"

class StartDialog : public QMainWindow {
	Q_OBJECT

public:
	/** Default constructor */
        StartDialog(NotifyQt *_notify, QWidget *parent = 0, Qt::WFlags flags = 0);

	bool loadedOk;

public slots:
	/** Overloaded QWidget.show */

private slots:

	void edited();
	void checkVersion();
        void downloadedVersion(qlonglong version, QString description, QString importances);
        void downloadInfo();
        void downloadedInfo(QString name, int librarymixer_id,
                            QString checkout_link1, QString contact_link1, QString link_title1,
                            QString checkout_link2, QString contact_link2, QString link_title2,
                            QString checkout_link3, QString contact_link3, QString link_title3);
	void downloadFriends();
        void downloadLibrary();
	void finishLoading();
	void updateDataReadProgress(int bytesRead, int totalBytes);
	void errorReceived(int errorCode);

private:
	void closeEvent (QCloseEvent * event);

	/** Qt Designer generated object */
        Ui::StartDialog ui;

        //This variable is populated from saved settings, and indicates that version numbers <= this we are not interested in
        qlonglong skip_to_version;

	NotifyQt* notify;

};

#endif

