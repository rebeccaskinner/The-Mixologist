/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006-2007, crypton
 *
 *  This file is part of the Mixologist.
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

#ifndef _MainWindow_H
#define _MainWindow_H
#include <QObject>
#include <QSystemTrayIcon>
#include "ui_MainWindow.h"
#include "Preferences/PreferencesWindow.h"
#include <interface/notifyqt.h>
#include <interface/settings.h>

class MainWindow;
extern MainWindow *mainwindow;

class NetworkDialog;
class PeersDialog;
class TransfersDialog;
class LibraryDialog;
class FriendsLibraryDialog;

class PeerStatus;
class RatesStatus;

class MainWindow : public QMainWindow{
	Q_OBJECT

public:
        /* Default Constructor */
        MainWindow(NotifyQt *_notify, QWidget *parent = 0, Qt::WFlags flags = 0);

        /* Public variables for the GUI */

        //Tray Icon stuff, accessed to create balloon pop ups
        QSystemTrayIcon* trayIcon;
        //Which page to open on click
        QWidget* trayOpenTo;
        //If set to true, instead of opening trayOpenTo, opens download folder on click
        bool trayOpenDownloadsFolder;
        //Changes the system tray icon based on transfers
        void setTrayIcon(float downKb, float upKb);

        //Dialogs and Windows
        PeersDialog *peersDialog;
        TransfersDialog *transfersDialog;
        LibraryDialog *libraryDialog;
        NetworkDialog *networkDialog;
        FriendsLibraryDialog *friendsLibraryDialog;
        PreferencesWindow *preferencesWindow;

        //Status bar
        PeerStatus *peerstatus;
        RatesStatus *ratesstatus;

        //Used for setting up signal and slot connections
        NotifyQt *notify;

        //Exposed so settings window can create and remove pages
        Ui::MainWindow ui;
        //List of actions and their associated pages, used to switch between pages.
        QHash<QAction*, QWidget*> actionPages;

        /* Called from other GUI classes to change which page is currently active.
           dialog is a pointer to any of the dialog pages from above. */
        void switchToDialog(QWidget* dialog);

public slots:
        void updateHashingInfo(const QString&) ;

private slots:

	void updateMenu();

	void toggleVisibility(QSystemTrayIcon::ActivationReason e);
	void toggleVisibilitycontextmenu();

        //Brings up the mainwindow to the appropriate page when a system tray popup is clicked.
        void trayMsgClicked();

        /* Toolbar fns. */
        /* Creates and displays the PreferencesWindow. */
        void showPreferencesWindow();
        /* Confirmation dialog for quit button */
        void QuitAction();
        /* Displays the page associated with the activated action. */
	void showPage(QAction *pageAction);

private:
        //Redirect close button on main window to close
        void closeEvent(QCloseEvent *e);
        //Immediately quits the Mixologist
        void doQuit();

        QAction *toggleVisibilityAction;
        QMenu *trayIconMenu;
	QLabel *_hashing_info_label ;

        bool tutorial_library_done;
        bool tutorial_friends_library_done;
};

#endif
