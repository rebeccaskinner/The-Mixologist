/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
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

#include <QtGui>
#include <QObject>
#include <version.h>
#include <qtsingleapplication/mixologistapplication.h>
#include <gui/MainWindow.h>
#include <gui/PeersDialog.h>
#include <gui/NetworkDialog.h>
#include <gui/TransfersDialog.h>
#include <gui/LibraryDialog.h>
#include <gui/StartDialog.h>
#include <interface/init.h>
#include <interface/iface.h>
#include <interface/notifyqt.h>
#include <interface/settings.h>
#if defined(Q_WS_WIN)
#include <windows.h>
#endif


//Setting up global extern for MainWindow.h
MainWindow *mainwindow = NULL;

int main(int argc, char *argv[]) {

    QStringList args;
    for (int i = 1; i < argc; i++) {
        args << QString(argv[i]);
    }

    Q_INIT_RESOURCE(images);

    //Seed the random number generator so we can use qrand
    qsrand(QDateTime::currentDateTime().toTime_t());

    Init::InitNetConfig();
    Init::processCmdLine(argc, argv);
    Init::loadBaseDir();

    /* The main QT application controller */
    MixologistApplication instance(argc, argv);

    if (instance.isRunning()) {
        if (args.last().startsWith("mixology:", Qt::CaseInsensitive)) {
            instance.sendMessage(args.last());
        }
        return 0;
    }
    NotifyQt *notify = new NotifyQt();

    /* Login Dialog */
    StartDialog *start = new StartDialog(notify);

    while (!start->loadedOk) {
        instance.processEvents();
#ifdef WIN32
        Sleep(10); //milliseconds
#else // __LINUX__
        usleep(10000); //microseconds
#endif
    }

    //Setup the universal extern mainwindow that is the actual GUI
    mainwindow = new MainWindow(notify);
    instance.setActivationWindow(mainwindow);
    //The main window can minimize to system tray, and when minimized, we don't want the program to quit when the last visible window closes.
    instance.setQuitOnLastWindowClosed(false);
    //For MixologistApplication (extending QTSingleApplication) to pass urls to an already running instance.
    QObject::connect(&instance, SIGNAL(messageReceived(const QString &)),
                     mainwindow->transfersDialog, SLOT(download(const QString &)));
    //To display a label at the bottom while hashing files.
    QObject::connect(notify, SIGNAL(hashingInfoChanged(const QString &)),
                     mainwindow, SLOT(updateHashingInfo(const QString &)));
    //To display library on the LibraryDialog.
    QObject::connect(notify, SIGNAL(libraryUpdated()),
                     mainwindow->libraryDialog, SLOT(insertLibrary()));
    //To display transfers on the TransfersDialog, as well as the total rate info in the corner.
    QObject::connect(notify, SIGNAL(transfersChanged()),
                     mainwindow->transfersDialog, SLOT(insertTransfers()));
    //To display friends on the PeersDialog, as well as the friends in the corner.
    QObject::connect(notify, SIGNAL(friendsChanged()),
                     mainwindow->peersDialog, SLOT(insertPeers()));
    //To display chat status in chat windows.
    QObject::connect(notify, SIGNAL(chatStatusChanged(int,const QString &)),
                     mainwindow->peersDialog, SLOT(updatePeerStatusString(int,const QString &)));
    //To popup a chat box on incoming requests where a chat window is needed
    QObject::connect(notify, SIGNAL(requestEventOccurred(int,int,int)),
                     mainwindow->peersDialog, SLOT(insertRequestEvent(int,int,int)));
    //To popup a chat box on transfer events where the friend's response requests chatting or indicates an error.
    QObject::connect(notify, SIGNAL(transferChatEventOccurred(int,int,QString,QString)),
                     mainwindow->peersDialog, SLOT(insertTransferEvent(int,int,QString,QString)));
    //To popup a query box on transfer events where the friend's response requires further input.
    QObject::connect(notify, SIGNAL(transferQueryEventOccurred(int,int,QString,QString)),
                     mainwindow->transfersDialog, SLOT(insertTransferEvent(int,int,QString,QString)));
    //To popup a box informing that a friend is attempting to send a file.
    QObject::connect(notify, SIGNAL(suggestionReceived(int,int,QString)),
                     mainwindow->transfersDialog, SLOT(suggestionReceived(int,int,QString)));
    /* When we have an attempted connection from a peer with a bad authentication certificate, update friends from the server
       to make sure that it's not because we have outdated information. Routing this through the GUI enables us
       to show the update graphics while doing an automatic update. */
    QObject::connect(notify, SIGNAL(connectionDenied()),
                     mainwindow->peersDialog, SLOT(updateFriends()));
    QObject::connect(notify, SIGNAL(userOptionalInfo(int,int,QString)),
                     mainwindow->peersDialog, SLOT(insertUserOptional(int,int,QString)));

    QSettings *settings = new QSettings(*mainSettings, QSettings::IniFormat);
    if (!settings->value("Gui/StartMinimized", DEFAULT_START_MINIMIZED).toBool()) mainwindow->show();
    settings->deleteLater();

    if (!args.isEmpty() && args.last().startsWith("mixology:", Qt::CaseInsensitive)) {
        mainwindow->transfersDialog->download(args.last());
    }

    /* Startup a Timer to keep the gui's updated */
    QTimer *timer = new QTimer(mainwindow);
    timer->connect(timer, SIGNAL(timeout()), notify, SLOT(UpdateGUI()));
    timer->start(1000);

    /* the main loop */
    int ti = instance.exec();
    delete mainwindow;
    return ti;

}
