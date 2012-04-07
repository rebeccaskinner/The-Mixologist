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
#include "Statusbar/ratesstatus.h"

#include <interface/iface.h>
#include <interface/notifyqt.h>
#include <interface/settings.h>

/* Images for toolbar icons */
#define IMAGE_CLOSE ":/images/close_normal.png"

MainWindow::MainWindow(QWidget *, Qt::WFlags) {
    /* Invoke the Qt Designer generated QObject setup routine */
    ui.setupUi(this);

    connect(peers, SIGNAL(connectionStateChanged(int,bool)), this, SLOT(updateConnectionStatus(int,bool)), Qt::QueuedConnection);
    connect(peers, SIGNAL(ownConnectionReadinessChanged(bool)), this, SLOT(updateConnectionReadiness(bool)), Qt::QueuedConnection);

    QSettings settings(*mainSettings, QSettings::IniFormat, this);

    /* Tutorials */
    tutorial_library_done = settings.value("Tutorial/Library", DEFAULT_TUTORIAL_DONE_LIBRARY).toBool();
    tutorial_friends_library_done = settings.value("Tutorial/FriendsLibrary", DEFAULT_TUTORIAL_DONE_FRIENDS_LIBRARY).toBool();
    offLM_enabled = settings.value("Transfers/EnableOffLibraryMixer", DEFAULT_ENABLE_OFF_LIBRARYMIXER_SHARING).toBool();

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
        QObject::connect(guiNotify, SIGNAL(logInfoChanged(QString)),
                         networkDialog, SLOT(setLogInfo(QString)), Qt::QueuedConnection);

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
    connectionStatus = new QWidget();
    QSizePolicy connectionStatusSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connectionStatusSizePolicy.setHorizontalStretch(0);
    connectionStatusSizePolicy.setVerticalStretch(0);
    connectionStatusSizePolicy.setHeightForWidth(connectionStatus->sizePolicy().hasHeightForWidth());
    connectionStatus->setSizePolicy(connectionStatusSizePolicy);
    QHBoxLayout *connectionStatusHorizontalLayout = new QHBoxLayout(connectionStatus);
    connectionStatusHorizontalLayout->setContentsMargins(10, 0, 0, 0); //left margin 10
    connectionStatusMovieLabel = new QLabel(connectionStatus);
    QMovie *movie = new QMovie(":/Images/AnimatedLoading.gif");
    connectionStatusMovieLabel->setMovie(movie);
    movie->start();
    movie->setSpeed(100); //100%
    connectionStatusHorizontalLayout->addWidget(connectionStatusMovieLabel);
    connectionStatusLabel = new QLabel(connectionStatus);
    connectionStatusLabel->setMargin(5);
    connectionStatusHorizontalLayout->addWidget(connectionStatusLabel);
    ui.statusbar->addWidget(connectionStatus);
    connect(connectionStatusLabel, SIGNAL(linkActivated(QString)), this, SLOT(connectionStatusClicked()));

    hashingHolder = new QWidget();
    QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(hashingHolder->sizePolicy().hasHeightForWidth());
    hashingHolder->setSizePolicy(sizePolicy);
    QHBoxLayout *horizontalLayout = new QHBoxLayout(hashingHolder);
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
    hashingInfoLabel = new QLabel(hashingHolder);
    hashingInfoLabel->setObjectName(QString::fromUtf8("label"));
    horizontalLayout->addWidget(hashingInfoLabel);
    QSpacerItem *horizontalSpacer = new QSpacerItem(1000, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalLayout->addItem(horizontalSpacer);

    ui.statusbar->addPermanentWidget(hashingHolder);
    hashingInfoLabel->hide() ;
    hashingHolder->hide();
    QObject::connect(guiNotify, SIGNAL(hashingInfoChanged(QString)), this, SLOT(updateHashingInfo(QString)));

    ratesstatus = new RatesStatus();
    ui.statusbar->addPermanentWidget(ratesstatus);

    /* Load up starting values. */
    updateConnectionStatus(peers->getConnectionStatus(), peers->getConnectionAutoConfigEnabled());
    updateConnectionReadiness(peers->getConnectionReadiness());

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

    trayIcon->show();

    /* Load backed up window settings. */
    GuiSettingsUtil::loadWidgetInformation(this);
}

void MainWindow::switchToDialog(QWidget *dialog) {
    if (actionPages.key(dialog) != NULL) {
        ui.stackPages->setCurrentWidget(dialog);
        actionPages.key(dialog)->setChecked(true);
    }
}

void MainWindow::showPreferencesWindow() {
    if (preferencesWindow == NULL) {
        preferencesWindow = new PreferencesWindow(this);
        preferencesWindow->show();
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

void MainWindow::showPage(QAction *pageAction) {
    ui.stackPages->setCurrentWidget(actionPages.value(pageAction));

    /* If this is our first time displaying one of the two library dialogs, and they've enabled the complex
       double libraries, pop-up a box explaining what they're seeing. */
    if (ui.stackPages->currentWidget() == libraryDialog && !tutorial_library_done && offLM_enabled) {
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
    } else if (ui.stackPages->currentWidget() == friendsLibraryDialog && !tutorial_friends_library_done && offLM_enabled) {
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

/* Status Bar */

void MainWindow::connectionStatusClicked() {
    displayInfoText(infoTextTypeToDisplay);
}

void MainWindow::updateHashingInfo(const QString &newText) {
    if (newText == "")
        hashingInfoLabel->hide() ;
    else {
        hashingInfoLabel->setText("Hashing file " + newText) ;
        hashingInfoLabel->show() ;
    }
}

void MainWindow::updateConnectionStatus(int newStatus, bool autoConfigEnabled) {
    switch (newStatus) {
    case CONNECTION_STATUS_FINDING_STUN_FRIENDS:
    case CONNECTION_STATUS_FINDING_STUN_FALLBACK_SERVERS:
        connectionStatusLabel->setText("Initializing");
        break;
    case CONNECTION_STATUS_STUNNING_INITIAL:
        if (autoConfigEnabled)
            connectionStatusLabel->setText("Auto Connection Config: Probing");
        else
            connectionStatusLabel->setText("Internet: Manual Connection (testing)");
        break;
    case CONNECTION_STATUS_TRYING_UPNP:
    case CONNECTION_STATUS_STUNNING_UPNP_TEST:
        connectionStatusLabel->setText("Auto Connection Config: Trying UPNP");
        break;
    case CONNECTION_STATUS_STUNNING_MAIN_PORT:
    case CONNECTION_STATUS_STUNNING_UDP_HOLE_PUNCHING_TEST:
    case CONNECTION_STATUS_STUNNING_FIREWALL_RESTRICTION_TEST:
        connectionStatusLabel->setText("Auto Connection Config: Trying UDP Firewall Hole Punching");
        break;
    /* If we get here, we are done with connection set up. */
    case CONNECTION_STATUS_UNFIREWALLED:
        if (autoConfigEnabled) {
            connectionStatusLabel->setText("Direct Connection");
            infoTextTypeToDisplay = INFO_TEXT_UNFIREWALLED;
        } else {
            connectionStatusLabel->setText("Manual Connection (direct)");
            infoTextTypeToDisplay = INFO_TEXT_MANUAL_OKAY;
        }
        break;
    case CONNECTION_STATUS_PORT_FORWARDED:
        if (autoConfigEnabled) {
            connectionStatusLabel->setText("Port-forwarded Connection");
            infoTextTypeToDisplay = INFO_TEXT_PORT_FORWARDED;
        } else {
            connectionStatusLabel->setText("Manual Connection (port-forwarded)");
            infoTextTypeToDisplay = INFO_TEXT_MANUAL_OKAY;
        }
        break;
    case CONNECTION_STATUS_UPNP_IN_USE:
        connectionStatusLabel->setText("UPNP-configured Connection");
        infoTextTypeToDisplay = INFO_TEXT_UPNP;
        break;
    case CONNECTION_STATUS_UDP_HOLE_PUNCHING:
        connectionStatusLabel->setText("UDP Hole-Punching");
        infoTextTypeToDisplay = INFO_TEXT_UDP_HOLE_PUNCHING;
        if (isNotifyBadInternet()) {
            mainwindow->trayMessageClickedAction = TRAY_MESSAGE_CLICKED_FULL_CONE_NAT_INFO;
            mainwindow->trayIcon->showMessage("Connection good to go!",
                                              QString("However, a restrictive firewall has been detected.\n") +
                                              "You can configure your firewall to get better transfer rates and more reliable connections with other friends that are firewalled.\n" +
                                              "Do you want to fix this?",
                                              QSystemTrayIcon::Information, 30);
        }
        break;
    case CONNECTION_STATUS_RESTRICTED_CONE_UDP_HOLE_PUNCHING:
        connectionStatusLabel->setText("UDP Hole-Punching (restricted-cone)");
        infoTextTypeToDisplay = INFO_TEXT_RESTRICTED_CONE_UDP_HOLE_PUNCHING;
        if (isNotifyBadInternet()) {
            mainwindow->trayMessageClickedAction = TRAY_MESSAGE_CLICKED_RESTRICTED_NAT_INFO;
            mainwindow->trayIcon->showMessage("Connection good to go!",
                                              QString("However, a restrictive firewall has been detected.\n") +
                                              "You can configure your firewall to get better transfer rates, faster connection times, and more reliable connections with other friends that are firewalled.\n" +
                                              "Do you want to fix this?",
                                              QSystemTrayIcon::Information, 30);
        }
        break;
    case CONNECTION_STATUS_SYMMETRIC_NAT:
        connectionStatusLabel->setText("Symmetric NAT (outbound connections only)");
        infoTextTypeToDisplay = INFO_TEXT_SYMMETRIC_NAT;
        if (isNotifyBadInternet()) {
            mainwindow->trayMessageClickedAction = TRAY_MESSAGE_CLICKED_SYMMETRIC_NAT_INFO;
            mainwindow->trayIcon->showMessage("Connection good to go!",
                                              QString("However, a very restrictive firewall has been detected.\n") +
                                              "If you do not configure your firewall you will not be able to connect to any firewalled friends and connecting to unfirewalled friends will be slow.\n" +
                                              "Do you want to fix this?",
                                              QSystemTrayIcon::Information, 30);
        }
        break;
    case CONNECTION_STATUS_UNKNOWN:
    default:
        if (autoConfigEnabled) {
            connectionStatusLabel->setText("Unknown");
            infoTextTypeToDisplay = INFO_TEXT_UNKNOWN_CONNECTION;
            if (isNotifyBadInternet())
                mainwindow->trayIcon->showMessage("Connection Warning",
                                                  QString("The Mixologist was not able to automatically configure your connection. ") +
                                                  "You may experience problems connecting to friends.",
                                                  QSystemTrayIcon::Information, 30);
        } else {
            connectionStatusLabel->setText("Manual Connection (test failed)");
            infoTextTypeToDisplay = INFO_TEXT_MANUAL_BAD;
            if (isNotifyBadInternet()) {
                mainwindow->trayMessageClickedAction = TRAY_MESSAGE_CLICKED_MANUAL_CONNECTION_FAIL;
                mainwindow->trayIcon->showMessage("Connection Warning",
                                                  QString("You have auto-connection config disabled, but the test of your connection failed. ") +
                                                  "You may experience problems connecting to friends.",
                                                  QSystemTrayIcon::Information, 30);
            }
        }
    }
    if (connectionStatusInFinalState(newStatus)) {
        if (peers->getConnectionReadiness()) {
            connectionStatusLabel->setText("<span style='color:grey'>Internet: <a href='filler' style='color:grey'>" +
                                           connectionStatusLabel->text() +
                                           "</a></span>");
        }
        /* If we are in a final state, but the connection is not yet ready, that must mean that we are currently updating our info to LibraryMixer. */
        else {
            connectionStatusLabel->setText("Publishing Address to Friends");
        }
    }
}

void MainWindow::updateConnectionReadiness(bool ready) {
    if (ready) {
        /* If we are now ready, make sure we didn't previously defer showing the Internet connection status in favor of displaying that we were
           publishing our address to LibraryMixer. */
        updateConnectionStatus(peers->getConnectionStatus(), peers->getConnectionAutoConfigEnabled());

        connectionStatusMovieLabel->hide();
        hashingHolder->show();
    } else {
        connectionStatusMovieLabel->show();
        hashingHolder->hide();
    }
}

bool MainWindow::isNotifyBadInternet() {
    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    return settings.value("Gui/NotifyBadInternet", DEFAULT_NOTIFY_BAD_INTERNET).toBool();
}

/* System Tray */

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
    if (TRAY_MESSAGE_CLICKED_DOWNLOADS_FOLDER == trayMessageClickedAction) {
        transfersDialog->openContaining();
    } else if (TRAY_MESSAGE_CLICKED_TRANSFERS_DIALOG == trayMessageClickedAction) {
        ui.stackPages->setCurrentWidget(transfersDialog);
        show();
        raise();
        activateWindow();
    } else if (TRAY_MESSAGE_CLICKED_FULL_CONE_NAT_INFO == trayMessageClickedAction) {
        displayInfoText(INFO_TEXT_UDP_HOLE_PUNCHING);
    } else if (TRAY_MESSAGE_CLICKED_RESTRICTED_NAT_INFO == trayMessageClickedAction) {
        displayInfoText(INFO_TEXT_RESTRICTED_CONE_UDP_HOLE_PUNCHING);
    } else if (TRAY_MESSAGE_CLICKED_SYMMETRIC_NAT_INFO == trayMessageClickedAction) {
        displayInfoText(INFO_TEXT_SYMMETRIC_NAT);
    } else if (TRAY_MESSAGE_CLICKED_MANUAL_CONNECTION_FAIL == trayMessageClickedAction) {
        displayInfoText(INFO_TEXT_MANUAL_BAD);
    }
}

/* General Utility */

void MainWindow::displayInfoText(InfoTextType type) {
    QMessageBox helpBox(this);
    QString info;
    QAbstractButton* disableAutoConfig = NULL;
    QAbstractButton* reenableAutoConfig = NULL;
    /* Used by disableAutoConfig to set which port to change to. */
    int changePortTo = 0;
    if (INFO_TEXT_UNFIREWALLED == type) {
        info += "<b>Direct Connection</b>";
        info += "<p>You've got a direct, unrestricted connection to the Internet.</p>";
        info += "<p>Everything's doing great!</p>";
    } else if (INFO_TEXT_PORT_FORWARDED == type) {
        info += "<b>Port-forwarded Connection</b>";
        info += "<p>You're behind a firewall that is forwarding traffic to the Mixologist.</p>";
        info += "<p>Everything's doing great!</p>";
    } else if (INFO_TEXT_UPNP == type) {
        info += "<b>UPNP-forwarded Connection</b>";
        info += "<p>You're behind a firewall that has been automatically configured using Universal Plug and Play to enable the Mixologist to connect.</p>";
        info += "<p>Everything's doing great!</p>";
    } else if (INFO_TEXT_UDP_HOLE_PUNCHING == type) {
        info += "<b>Firewall detected (full-cone NAT type)</b>";
        info += "<p>You are behind a firewall that is blocking inbound connections.</p>";
        info += "<p>Fortunately, the Mixologist is working around this using a technique called 'UDP Tunneling', which enables the Mixologist to remain fully-functional, but is less reliable, slower, and less bandwidth-efficient than if there were no firewall.</p>";
    } else if (INFO_TEXT_RESTRICTED_CONE_UDP_HOLE_PUNCHING == type) {
        info += "<b>Firewall detected (restricted-cone NAT type)</b>";
        info += "<p>You are behind a restricted-cone firewall that is blocking inbound connections.</p>";
        info += "<p>Fortunately, the Mixologist is working around this using a technique called 'UDP Tunneling', which enables the Mixologist to remain fully-functional, but is less reliable, slower, and less bandwidth-efficient than if there were no firewall, and connecting to your firewalled friends will be slow, taking up to 5 minutes or more.</p>";
    } else if (INFO_TEXT_SYMMETRIC_NAT == type) {
        info += "<b>Firewall detected (symmetric NAT type)</b>";
        info += "<p>You are behind a highly-restrictive symmetric NAT firewall that is blocking inbound connections.</p>";
        info += "<p>You will not be able to connect to your friends that have firewalls at all, and your connections to all friends will be slow, taking up to 5 minutes or more after they come online.</p>";
    } else if (INFO_TEXT_UNKNOWN_CONNECTION == type) {
        info += "<b>Unknown Connection</b>";
        info += "<p>The Mixologist automatic connection configuration has failed!</p>";
        info += "<p>You might be able to connect to your friends still, or you might not.</p>";
        info += "<p>Try restarting to resolve this problem.</p>";
        info += "<p>If you always see this message when you have a working Internet connection, consider filing a bug report with details about your network situation.</p>";
    } else if (INFO_TEXT_MANUAL_OKAY == type) {
        info += "<b>Manually Configured</b>";
        info += "<p>You've disabled auto-connection configuration in Options.</p>";
        info += "<p>Everything appears to be doing great!</p>";
        info += "<p>However, if you recently closed and then quickly reopened the Mixologist, or if you just switched to a manual connection in Options without restarting, it is possible this is a false-positive.</p>";
        info += "<p>If you see this message and from when you opened the Mixologist this time it was more than an hour since the previous time it was open, you can be pretty sure everything is doing great!</p>";
    } else if (INFO_TEXT_MANUAL_BAD == type) {
        info += "<b>Manually Configured</b>";
        info += "<p>You've disabled auto-connection configuration in Options.</p>";
        info += "<p>However, when the Mixologist tested its connection, it still was being blocked by a firewall.</p>";
        info += "<p>You may not be able to connect to your friends that have firewalls at all, and your connections to all friends may be slow, taking up to 5 minutes or more after they come online.</p>";
    } else {
        return;
    }

    /* Display the special fix-it message for problematic connections. */
    if (INFO_TEXT_UDP_HOLE_PUNCHING == type ||
        INFO_TEXT_RESTRICTED_CONE_UDP_HOLE_PUNCHING == type ||
        INFO_TEXT_SYMMETRIC_NAT == type ||
        INFO_TEXT_MANUAL_BAD == type) {
        PeerDetails detail;
        QString ownIP;
        QString portString;
        QString portChangedString = "current";
        if (peers->getPeerDetails(peers->getOwnLibraryMixerId(), detail)) {
            ownIP = detail.localAddr;
            changePortTo = detail.localPort;
            /* If we are switching from auto to manual, we change our local port to avoid false-positives. */
            if (INFO_TEXT_MANUAL_BAD != type) {
                changePortTo++;
                portChangedString = "new";
            }
            portString = QString::number(changePortTo);
        }
        info += "<p><b>Fixing the Problem</b></p>";
        if (INFO_TEXT_MANUAL_BAD != type) {
            info += "<p>Ideally, you should fix this by configuring your firewall to open up a port for your computer.</p>";
        } else {
            info += "<p>This is very likely an indication that something went wrong when you were configuring your firewall. If you want to give it another shot, here's the information again:</p>";
        }
        info += QString("<p>For help on configuring your router, ") +
                "<a href='http://www.pcwintech.com/port-forwarding-guides'>click here for a guide</a> (not affiliated with the Mixologist).</p>";
        info += "<p>To use the guide, first find the brand of your router, and then choose the model number that best matches the model written on your router.</p>";
        info += QString("<p>When following the guide, set the internal IP address to forward to as <b>" + ownIP + "</b> (your computer's internal network IP), and both the external and internal port as <b>") + portString +
                "</b> (your Mixologist's " + portChangedString + " manual-mode port), with both TCP and UDP traffic forwarded.</p>";

        if (INFO_TEXT_MANUAL_BAD != type) {
            info += "<p>After you have done everything in the guide to configure your router, click the Turn Off Auto Config button to let the Mixologist know you want to switch to manually configured router mode. (You're totally awesome and savvy!)</p>";

            helpBox.addButton(QMessageBox::Cancel);
            disableAutoConfig = helpBox.addButton("Turn Off Auto Config", QMessageBox::AcceptRole);
        } else {
            info += "<p>Alternatively, if you'd like to give auto-connection configuration another shot, you can do so now.</p>";

            helpBox.addButton(QMessageBox::Cancel);
            reenableAutoConfig = helpBox.addButton("Re-enable Auto Config", QMessageBox::AcceptRole);
        }
    }

    helpBox.setText(info);
    helpBox.setTextFormat(Qt::RichText);
    helpBox.exec();

    show();
    raise();
    activateWindow();

    if (disableAutoConfig != NULL &&
        (helpBox.clickedButton() == disableAutoConfig)) {
        PeerDetails ownDetails;
        peers->getPeerDetails(peers->getOwnLibraryMixerId(), ownDetails);
        QSettings settings(*mainSettings, QSettings::IniFormat, this);
        settings.setValue("Network/AutoOrPort", changePortTo);
        peers->restartOwnConnection();
    }

    if (reenableAutoConfig != NULL &&
        (helpBox.clickedButton() == reenableAutoConfig)) {
        QSettings settings(*mainSettings, QSettings::IniFormat, this);
        settings.setValue("Network/AutoOrPort", DEFAULT_NETWORK_AUTO_OR_PORT);
        peers->restartOwnConnection();
    }
}

/*
void MainWindow::setTrayIcon(float downKb, float upKb){
    if (upKb > 0 && downKb <= 0) trayIcon->setIcon(QIcon(":/Images/Up1Down0.png"));
    else if (upKb <= 0 && downKb > 0) trayIcon->setIcon(QIcon(":/Images/Up0Down1.png"));
    else if (upKb > 0 && downKb > 0) trayIcon->setIcon(QIcon(":/Images/Up1Down1.png"));
    else trayIcon->setIcon(QIcon(":/Images/Up0Down0.png"));
}*/
