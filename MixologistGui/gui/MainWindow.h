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
#include <interface/peers.h>

class MainWindow;
extern MainWindow *mainwindow;

class NetworkDialog;
class PeersDialog;
class TransfersDialog;
class LibraryDialog;
class FriendsLibraryDialog;

class RatesStatus;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0, Qt::WFlags flags = 0);

    /* Public variables for the GUI */

    /* Tray Icon stuff, accessed to create balloon pop ups. */
    QSystemTrayIcon* trayIcon;

    /* The actions to execute when the tray balloon pop up message is clicked. */
    enum TrayMessageClickedAction {
        /* Popup information about how to fix the full-cone NAT we have. */
        TRAY_MESSAGE_CLICKED_FULL_CONE_NAT_INFO,
        /* Popup information about how to fix the restricted-cone NAT we have. */
        TRAY_MESSAGE_CLICKED_RESTRICTED_NAT_INFO,
        /* Popup information about how to fix the symmetric NAT we have. */
        TRAY_MESSAGE_CLICKED_SYMMETRIC_NAT_INFO,
        /* Switch to the transfers dialog. */
        TRAY_MESSAGE_CLICKED_TRANSFERS_DIALOG,
        /* Open the downloads folder. */
        TRAY_MESSAGE_CLICKED_DOWNLOADS_FOLDER
    };
    TrayMessageClickedAction trayMessageClickedAction;

    /* Changes the system tray icon based on transfers.
       Currently disabled for being ugly. */
    //void setTrayIcon(float downKb, float upKb);

    /* Dialogs and Windows */
    PeersDialog *peersDialog;
    TransfersDialog *transfersDialog;
    LibraryDialog *libraryDialog;
    NetworkDialog *networkDialog;
    FriendsLibraryDialog *friendsLibraryDialog;
    PreferencesWindow *preferencesWindow;

    /* Status bar */
    QWidget *connectionStatus;
    QWidget *hashingHolder;
    RatesStatus *ratesstatus;

    /* Exposed so settings window can create and remove pages. */
    Ui::MainWindow ui;

    /* List of actions and their associated pages, used to switch between pages. */
    QHash<QAction*, QWidget*> actionPages;

    /* Called from other GUI classes to change which page is currently active.
       dialog is any of the dialog page pointers from above. */
    void switchToDialog(QWidget* dialog);

public slots:
    /* Sets the status bar to display that a file with title equal to the supplied string is being hashed. */
    void updateHashingInfo(const QString&);

    /* Updates the status bar display with the status of our connection set up. */
    void updateConnectionStatus(int newStatus);

private slots:
    void updateMenu();

    void toggleVisibility(QSystemTrayIcon::ActivationReason e);
    void toggleVisibilitycontextmenu();

    /* Brings up the mainwindow to the appropriate page when a system tray popup is clicked. */
    void trayMsgClicked();

    /* When the connection status is clicked, pops up the appropriate informative window. */
    void connectionStatusClicked();

    /* Toolbar fns. */

    /* Creates and displays the PreferencesWindow. */
    void showPreferencesWindow();

    /* Confirmation dialog for quit button */
    void QuitAction();

    /* Displays the page associated with the activated action. */
    void showPage(QAction *pageAction);

private:
    /* Redirect close button on main window to close. */
    void closeEvent(QCloseEvent *e);

    /* Immediately quits the Mixologist. */
    void doQuit();

    /* Displays a pop up message with information about the given connection type. */
    enum InfoTextType {
        INFO_TEXT_UNFIREWALLED,
        INFO_TEXT_PORT_FORWARDED,
        INFO_TEXT_UPNP,
        INFO_TEXT_UDP_HOLE_PUNCHING,
        INFO_TEXT_RESTRICTED_CONE_UDP_HOLE_PUNCHING,
        INFO_TEXT_SYMMETRIC_NAT,
        INFO_TEXT_UNKNOWN_CONNECTION
    };
    void displayInfoText(InfoTextType type);

    /* Checks the settings if we need to notify for bad Internet connections. */
    bool isNotifyBadInternet();

    QAction *toggleVisibilityAction;
    QMenu *trayIconMenu;
    QLabel *connectionStatusLabel;
    QLabel *connectionStatusMovieLabel;
    QLabel *hashingInfoLabel;

    /* Which InfoTextType to display when clicked. */
    InfoTextType infoTextTypeToDisplay;

    /* Whether the Off-LM tutorial for the library page has been displayed before. */
    bool tutorial_library_done;

    /* Whether the Off-LM tutorial for the friends library page has been displayed before. */
    bool tutorial_friends_library_done;

    /* We read this to see if we need the tutorials. */
    bool offLM_enabled;
};
#endif
