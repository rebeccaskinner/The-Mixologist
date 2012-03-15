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

#include "interface/notifyqt.h"
#include "interface/notify.h"
#include "interface/peers.h"
#include "interface/settings.h"

#include "gui/MainWindow.h"
#include "gui/NetworkDialog.h"
#include "gui/LibraryDialog.h"
#include "gui/TransfersDialog.h"
#include "gui/Toaster/OnlineToaster.h"

#include <version.h>

#include <iostream>
#include <sstream>
#include <time.h>

void NotifyQt::notifySuggestion(unsigned int librarymixer_id, const QString &title, const QStringList &paths, const QStringList &hashes, const QList<qlonglong> &filesizes) {
    emit userOptionalInfo(librarymixer_id, NOTIFY_USER_SUGGEST_RECEIVED, title);
    emit suggestionReceived(librarymixer_id, title, paths, hashes, filesizes);
}

void NotifyQt::notifyRequestEvent(transferEvent event, unsigned int librarymixer_id, unsigned int item_id) {
    emit requestEventOccurred(event, librarymixer_id, item_id);
}

void NotifyQt::notifyTransferEvent(transferEvent event, unsigned int librarymixer_id, QString transfer_name, QString extra_info) {
    emit transferChatEventOccurred(event, librarymixer_id, transfer_name, extra_info);
}

void NotifyQt::notifyChatStatus(unsigned int librarymixer_id, const QString &status_string) {
    emit chatStatusChanged(librarymixer_id, status_string) ;
}

void NotifyQt::notifyHashingInfo(QString fileinfo) {
    emit hashingInfoChanged(fileinfo);
}

void NotifyQt::notifyLog(QString message) {
    emit logInfoChanged(message);
}

void NotifyQt::notifyUserOptional(unsigned int librarymixer_id, userOptionalCodes code, QString message) {
    emit userOptionalInfo(librarymixer_id, code, message);
}

/* New Timer Based Update scheme ...
 * means correct thread seperation
 *
 * uses Flags, to detect changes
 */

void NotifyQt::UpdateGUI() {
    /* hack to force updates until we've fixed that part */
    static  time_t lastTs = 0;

    if (time(NULL) > lastTs) {              // always update, every 1 sec.
        emit transfersChanged(); //This is the timer that keeps TransfersDialog up to date
        emit friendsChanged(); //This is the time that keeps PeersDialog up to date
    }

    lastTs = time(NULL) ;
}


void NotifyQt::displaySysMessage(int type, QString title, QString msg) {
    switch (type) {
        case SYS_ERROR:
            QMessageBox::critical(0, title, msg);
            break;
        case SYS_WARNING:
            QMessageBox::warning(0, title, msg);
            break;
        default:
        case SYS_INFO:
            QMessageBox::information(0, title, msg);
            break;
    }
}

void NotifyQt::displayPopupMessage(int type, QString name, QString msg) {
    QSettings settings(*mainSettings, QSettings::IniFormat);

    switch (type) {
        case POPUP_CONNECT:
            if (settings.value("Gui/NotifyConnect", DEFAULT_NOTIFY_CONNECT).toBool()) {
                OnlineToaster *onlineToaster = new OnlineToaster();
                onlineToaster->setMessage(name);
                onlineToaster->show();
            }
            break;
        case POPUP_DOWNDONE:
            if (settings.value("Gui/NotifyDownloadDone", DEFAULT_NOTIFY_DOWNLOAD_DONE).toBool()) {
                mainwindow->trayOpenDownloadsFolder = true;
                mainwindow->trayIcon->showMessage("Download complete",
                                                  name.append("\nhas finished downloading."),
                                                  QSystemTrayIcon::Information, INT_MAX );
            }
            break;
        case POPUP_NEW_VERSION_FROM_FRIEND:
            if (QMessageBox::question(NULL,
                                      "New version",
                                      name +
                                      " connected to you with a newer version of the Mixologist (" +
                                      VersionUtil::convert_to_display_version(msg.toLongLong()) +
                                      "). Quit now and download the update?",
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                    == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl(QString("http://librarymixer.com/download/mixologist/")));
                exit(1);
            }
            break;
        case POPUP_MISC:
            mainwindow->trayIcon->showMessage(name,
                                              msg,
                                              QSystemTrayIcon::Information, INT_MAX);
    }
}
