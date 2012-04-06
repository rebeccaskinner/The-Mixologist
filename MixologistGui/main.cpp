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
#include <gui/FriendsLibraryDialog.h>
#include <gui/StartDialog.h>
#include <interface/init.h>
#include <interface/iface.h>
#include <interface/notifyqt.h>
#include <interface/settings.h>
#if defined(Q_WS_WIN)
#include <windows.h>
#endif

/* So that when we compile statically on Windows and OS X, gif support is enabled. */
#if defined(STATIC)
#include <QtPlugin>
Q_IMPORT_PLUGIN(qgif)
#endif

//Setting up global extern for MainWindow.h
MainWindow *mainwindow = NULL;
//Setting up global extern for iface.h
NotifyBase *notifyBase = NULL;
//Setting up global extern for notifyqt.h
NotifyQt *guiNotify = NULL;

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
        if (!args.isEmpty() && args.last().startsWith("mixology:", Qt::CaseInsensitive)) {
            instance.sendMessage(args.last());
        }
        return 0;
    }
    guiNotify = new NotifyQt();
    notifyBase = guiNotify;

    /* Login Dialog */
    StartDialog *start = new StartDialog();

    while (!start->loadedOk) {
        instance.processEvents();
#ifdef WIN32
        Sleep(10); //milliseconds
#else // __LINUX__
        usleep(10000); //microseconds
#endif
    }

    //Setup the universal extern mainwindow that is the actual GUI
    mainwindow = new MainWindow();
    instance.setActivationWindow(mainwindow);
    //The main window can minimize to system tray, and when minimized, we don't want the program to quit when the last visible window closes.
    instance.setQuitOnLastWindowClosed(false);

    //For MixologistApplication (extending QTSingleApplication) to pass urls to an already running instance.
    QObject::connect(&instance, SIGNAL(messageReceived(QString)),
                     mainwindow->transfersDialog, SLOT(download(QString)));

    QSettings *settings = new QSettings(*mainSettings, QSettings::IniFormat);
    if (!settings->value("Gui/StartMinimized", DEFAULT_START_MINIMIZED).toBool()) mainwindow->show();

    settings->deleteLater();

    if (!args.isEmpty() && args.last().startsWith("mixology:", Qt::CaseInsensitive)) {
        mainwindow->transfersDialog->download(args.last());
    }

    /* The main program GUI event loop. */
    int execResult = instance.exec();

    delete mainwindow;
    return execResult;
}
