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


#include "MainWindow.h"
#include "NetworkDialog.h"
#include "PeersDialog.h"
#include "TransfersDialog.h"
#include "LibraryDialog.h"
#include "FriendsLibraryDialog.h"
#include "gui/Util/GuiSettingsUtil.h"
#include "Statusbar/peerstatus.h"
#include "Statusbar/ratesstatus.h"

#include <interface/iface.h>
#include <interface/notify.h>
#include <interface/settings.h>

/* Images for toolbar icons */
#define IMAGE_CLOSE ":/images/close_normal.png"

/** Constructor */
MainWindow::MainWindow(NotifyQt *_notify, QWidget *, Qt::WFlags)
    :notify(_notify) {
    /* Invoke the Qt Designer generated QObject setup routine */
    ui.setupUi(this);

    QSettings settings(*mainSettings, QSettings::IniFormat, this);

    /* Tutorials */
    tutorial_library_done = settings.value("Tutorial/Library", DEFAULT_TUTORIAL_DONE_LIBRARY).toBool();
    tutorial_friends_library_done = settings.value("Tutorial/FriendsLibrary", DEFAULT_TUTORIAL_DONE_FRIENDS_LIBRARY).toBool();

    /* Toolbar and stack pages */
    //These toolbar buttons change the page in the stack page
    QActionGroup *grp = new QActionGroup(this);

    peersDialog = new PeersDialog(ui.stackPages);
    actionPages.insert(ui.actionFriends, peersDialog);
    grp->addAction(ui.actionFriends);
    ui.stackPages->insertWidget(ui.stackPages->count(), peersDialog);

    transfersDialog = new TransfersDialog(ui.stackPages);
    actionPages.insert(ui.actionRequests, transfersDialog);
    ui.stackPages->insertWidget(ui.stackPages->count(), transfersDialog);
    grp->addAction(ui.actionRequests);

    libraryDialog = new LibraryDialog(ui.stackPages);
    actionPages.insert(ui.actionLibrary, libraryDialog);
    grp->addAction(ui.actionLibrary);
    ui.stackPages->insertWidget(ui.stackPages->count(), libraryDialog);

    friendsLibraryDialog = new FriendsLibraryDialog(ui.stackPages);
    actionPages.insert(ui.actionFriendsLibrary, friendsLibraryDialog);
    grp->addAction(ui.actionFriendsLibrary);
    ui.stackPages->insertWidget(ui.stackPages->count(), friendsLibraryDialog);

    /* Now add the conditional tabs */
    grp->addAction(ui.actionNetwork);
    if (settings.value("Gui/ShowAdvanced", DEFAULT_SHOW_ADVANCED).toBool()) {
        networkDialog = new NetworkDialog(ui.stackPages);
        actionPages.insert(ui.actionNetwork, networkDialog);
        ui.stackPages->insertWidget(ui.stackPages->count(), networkDialog);
        QObject::connect(notify, SIGNAL(logInfoChanged(QString)),
                         networkDialog, SLOT(setLogInfo(QString)));

    } else ui.actionNetwork->setVisible(false);

    connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(showPage(QAction *)));

    if (settings.value("Gui/LastView", "peersDialog").toString() == "libraryDialog") switchToDialog(libraryDialog);
    else if (settings.value("Gui/LastView", "peersDialog").toString() == "transfersDialog") switchToDialog(transfersDialog);
    else if (settings.value("Gui/LastView", "peersDialog").toString() == "friendsLibraryDialog") switchToDialog(friendsLibraryDialog);
    else switchToDialog(peersDialog);

    //These toolbar buttons are popups
    connect(ui.actionOptions, SIGNAL(triggered()), this, SLOT(showPreferencesWindow()));
    connect(ui.actionQuit, SIGNAL(triggered()), this, SLOT(QuitAction()));

    preferencesWindow = NULL;

    /* StatusBar */
    peerstatus = new PeerStatus();
    ui.statusbar->addWidget(peerstatus);

    QWidget *widget = new QWidget();
    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(widget->sizePolicy().hasHeightForWidth());
    widget->setSizePolicy(sizePolicy);
    QHBoxLayout *horizontalLayout = new QHBoxLayout(widget);
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
    _hashing_info_label = new QLabel(widget) ;
    _hashing_info_label->setObjectName(QString::fromUtf8("label"));

    horizontalLayout->addWidget(_hashing_info_label);
    QSpacerItem *horizontalSpacer = new QSpacerItem(1000, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalLayout->addItem(horizontalSpacer);

    ui.statusbar->addPermanentWidget(widget);
    _hashing_info_label->hide() ;

    ratesstatus = new RatesStatus();
    ui.statusbar->addPermanentWidget(ratesstatus);

    /* System tray */
    trayIconMenu = new QMenu(this);
    QObject::connect(trayIconMenu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));
    toggleVisibilityAction =
        trayIconMenu->addAction(tr("Show/Hide"), this, SLOT(toggleVisibilitycontextmenu()));
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(QIcon(IMAGE_CLOSE), tr("&Quit"), this, SLOT(QuitAction()));

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setToolTip(tr("The Mixologist"));
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setIcon(QIcon(":/Images/Peers.png"));

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
            SLOT(toggleVisibility(QSystemTrayIcon::ActivationReason)));
    connect(trayIcon, SIGNAL(messageClicked()), this, SLOT(trayMsgClicked()));

    trayOpenDownloadsFolder = false;

    trayIcon->show();

    //Load backed up window settings
    GuiSettingsUtil::loadWidgetInformation(this);
}

void MainWindow::switchToDialog(QWidget *dialog) {
    if (actionPages.key(dialog) != NULL) {
        ui.stackPages->setCurrentWidget(dialog);
        actionPages.key(dialog)->setChecked(true);
    }
}

void MainWindow::updateHashingInfo(const QString &s) {
    if (s == "")
        _hashing_info_label->hide() ;
    else {
        _hashing_info_label->setText("Hashing file " + s) ;
        _hashing_info_label->show() ;
    }
}

void MainWindow::showPreferencesWindow() {
    if (preferencesWindow == NULL) {
        preferencesWindow = new PreferencesWindow(this);
        preferencesWindow->show();;
    } else {
        preferencesWindow->activateWindow();
    }
}

void MainWindow::QuitAction() {
    if ((QMessageBox::question(this, "The Mixologist", "Do you really want to exit the Mixologist?",
                               QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes))== QMessageBox::Yes) {
        doQuit();
    } else return;
}

void MainWindow::closeEvent(QCloseEvent *e) {
    QMessageBox msgBox;
    msgBox.setWindowTitle("The Mixologist");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText("Do you want to minimize the Mixologist to the system tray or exit?");
    QAbstractButton *buttonMinimize = msgBox.addButton("Minimize to Tray", QMessageBox::AcceptRole);
    QAbstractButton *buttonQuit = msgBox.addButton("Exit", QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();
    if (msgBox.clickedButton() == buttonMinimize) {
        QTimer::singleShot(0, this, SLOT(hide()));
        e->ignore();
    } else if (msgBox.clickedButton() == buttonQuit) {
        doQuit();
    } else {
        e->ignore();
    }
}

void MainWindow::doQuit() {
    //Increase responsiveness by immediately hiding
    hide();
    trayIcon->hide();
    GuiSettingsUtil::saveWidgetInformation(this);
    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    if (ui.stackPages->currentWidget() == libraryDialog) {
        settings.setValue("Gui/LastView", "libraryDialog");
    } else if (ui.stackPages->currentWidget() == transfersDialog) {
        settings.setValue("Gui/LastView", "transfersDialog");
    } else if (ui.stackPages->currentWidget() == friendsLibraryDialog) {
        settings.setValue("Gui/LastView", "friendsLibraryDialog");
    } else {
        settings.setValue("Gui/LastView", "peersDialog");
    }
    control->ShutdownMixologist();
    qApp->quit();
}

/** Shows the Main page associated with the activated action. */
void MainWindow::showPage(QAction *pageAction) {
    ui.stackPages->setCurrentWidget(actionPages.value(pageAction));

    /* If this is our first time displaying one of the two library dialogs, and they've enabled the complex
       double libraries, pop-up a box explaining what they're seeing. */
    if (ui.stackPages->currentWidget() == libraryDialog && !tutorial_library_done && friendsLibraryDialog) {
        QSettings settings(*mainSettings, QSettings::IniFormat, this);
        settings.setValue("Tutorial/Library", true);
        tutorial_library_done = true;
        QMessageBox helpBox(this);
        QString info("You should now be looking at the My Library tab of the Mixologist for the first time.");
        info += "<p>The <b>upper box</b> syncs with LibraryMixer to display the things you've listed as in your library and available for your friends to check out on the website.</p>";
        info += "<p>You can setup automatic responses for when your friends ask you for things you've listed here, but most people find it easier just to set it in the chat window the first time a friend requests something.</p>";
        info += "<p>The <b>lower box</b> lists files and folders you're sharing with your friends only in the Mixologist and not on the LibraryMixer website.</p>";
        info += "<p>This is the optional second method of file transfers you enabled on startup.</p>";
        info += "<p>If you drag and drop files or folders here, the list of what you're sharing will be automatically synced with your friends' Mixologists.</p>";
        helpBox.setText(info);
        helpBox.setTextFormat(Qt::RichText);
        helpBox.exec();
    } else if (ui.stackPages->currentWidget() == friendsLibraryDialog && !tutorial_friends_library_done && friendsLibraryDialog) {
        QSettings settings(*mainSettings, QSettings::IniFormat, this);
        settings.setValue("Tutorial/FriendsLibrary", true);
        tutorial_friends_library_done = true;
        QMessageBox helpBox(this);
        QString info("This is the Friends' Library tab of the Mixologist.");
        info += "<p>The <b>upper box</b> syncs with LibraryMixer to display the things your friends have listed on LibraryMixer as available for you to check out.</p>";
        info += "<p>The <b>lower box</b> lists files and folders your friends have shared with you only in the Mixologist and not on the LibraryMixer website.</p>";
        info += "<p>This is the optional second method of file transfers you enabled on startup.</p>";
        info += "<p>The <b>search bar</b> up top lets you just type to search both of these at the same time.</p>";
        helpBox.setText(info);
        helpBox.setTextFormat(Qt::RichText);
        helpBox.exec();
    }
}

void MainWindow::updateMenu() {
    toggleVisibilityAction->setText(isVisible() ? tr("Hide") : tr("Show"));
}

void MainWindow::toggleVisibility(QSystemTrayIcon::ActivationReason e) {
    if (e == QSystemTrayIcon::Trigger || e == QSystemTrayIcon::DoubleClick) {
        if (isHidden()) {
            show();
            if (isMinimized()) {
                if (isMaximized()) showMaximized();
                else showNormal();
            }
            raise();
            activateWindow();
        } else hide();
    }
}

void MainWindow::toggleVisibilitycontextmenu() {
    if (isVisible()) hide();
    else show();
}

void MainWindow::trayMsgClicked() {
    if(trayOpenDownloadsFolder){
        trayOpenDownloadsFolder = false;
        transfersDialog->openContaining();
    } else {
        ui.stackPages->setCurrentWidget(trayOpenTo);
        show();
        raise();
        activateWindow();
    }
}

/*
void MainWindow::setTrayIcon(float downKb, float upKb){
    if (upKb > 0 && downKb <= 0) trayIcon->setIcon(QIcon(":/Images/Up1Down0.png"));
    else if (upKb <= 0 && downKb > 0) trayIcon->setIcon(QIcon(":/Images/Up0Down1.png"));
    else if (upKb > 0 && downKb > 0) trayIcon->setIcon(QIcon(":/Images/Up1Down1.png"));
    else trayIcon->setIcon(QIcon(":/Images/Up0Down0.png"));
}*/
