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
#include "interface/settings.h"

#include "ui_StartDialog.h"

/*
 * The window that appears with login information before the Mixologist starts in earnest.
 */

class StartDialog : public QMainWindow {
	Q_OBJECT

public:
	/** Default constructor */
        StartDialog(QWidget *parent = 0, Qt::WFlags flags = 0);

	bool loadedOk;

public slots:
	/** Overloaded QWidget.show */

private slots:
        //Clears out saved password when auto logon box is unchecked
        void autoLogonChecked(int state);
        //Enables or disables the login button if we have all necessary login information
        void edited();
        //Begin 1st step after clicking Log In button to check client version
	void checkVersion();
        //Begin 2nd step to query server for: id, name, communications links
        void downloadedVersion(qulonglong version, QString description, QString importances);
        //Begin 3rd step to upload info to server
        void downloadedInfo(QString name, unsigned int librarymixer_id,
                            QString checkout_link1, QString contact_link1, QString link_title1,
                            QString checkout_link2, QString contact_link2, QString link_title2,
                            QString checkout_link3, QString contact_link3, QString link_title3,
                            QDomElement libraryNode);
        //Begin 4th step to download friends
        void uploadedInfo();
        //Begin 5th step to download library of friends items
        void downloadedFriends();
        //Loading complete, start the Mixologist
	void finishLoading();
        //Updates the progress bar from librarymixer-connect with information on data transfer progress
        void updateDataReadProgress(int bytesRead, int totalBytes);
        //Used by librarymixer-connect to signal a problem in a data transfer
        void errorReceived(int errorCode);

private:
	void closeEvent (QCloseEvent * event);

	/* Qt Designer generated object */
        Ui::StartDialog ui;

        /* This variable is populated from saved settings, and indicates that version numbers <= this we are not interested in. */
        qulonglong skip_to_version;

        /* This variable is populated from the server, and indicates the latest version of the Mixologist we know about.
           If own version or skip_to_version is greater, will use that instead (can come up with test versions. */
        qulonglong latest_known_version;
    };

#endif

