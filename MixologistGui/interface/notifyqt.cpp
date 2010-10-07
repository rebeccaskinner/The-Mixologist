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

#include <iostream>
#include <sstream>
#include <time.h>

/*****
 * #define NOTIFY_DEBUG
 ****/

void NotifyQt::notifySuggestion(int librarymixer_id, int item_id, QString name) {
    emit userOptionalInfo(librarymixer_id, NOTIFY_USER_SUGGEST_RECEIVED, name);
    emit suggestionReceived(librarymixer_id, item_id, name);
}

void NotifyQt::notifyRequestEvent(transferEvent event, int librarymixer_id, int item_id) {
    emit requestEventOccurred(event, librarymixer_id, item_id);
}

void NotifyQt::notifyTransferEvent(transferEvent event, int librarymixer_id, QString transfer_name, QString extra_info) {
    if (event == NOTIFY_TRANSFER_LEND) {
        emit transferQueryEventOccurred(event, librarymixer_id, transfer_name, extra_info);
    } else {
        emit transferChatEventOccurred(event, librarymixer_id, transfer_name, extra_info);
    }
}

void NotifyQt::notifyChatStatus(int librarymixer_id, const QString &status_string) {
#ifdef NOTIFY_DEBUG
    std::cerr << "NotifyQt::notifyChatStatus()" << std::endl;
#endif
    emit chatStatusChanged(librarymixer_id, status_string) ;
}

void NotifyQt::notifyHashingInfo(QString fileinfo) {
#ifdef NOTIFY_DEBUG
    std::cerr << "NotifyQt::notifyHashingInfo()" << std::endl;
#endif
    emit hashingInfoChanged(fileinfo);
}

void NotifyQt::notifyLibraryUpdated() {
    emit libraryUpdated();
}

void NotifyQt::notifyLog(QString message) {
    emit logInfoChanged(message);
}

void NotifyQt::notifyUserOptional(int librarymixer_id, userOptionalCodes code, QString message) {
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

    //  std::cerr << "Got update signal t=" << lastTs << std::endl ;

    if (time(NULL) > lastTs) {              // always update, every 1 sec.
        emit transfersChanged(); //This is the timer that keeps TransfersDialog up to date
        emit friendsChanged(); //This is the time that keeps PeersDialog up to date
    }

    lastTs = time(NULL) ;

    /* Finally Check for PopupMessages / System Error Messages */

    if (notify) {
        uint32_t sysid;
        uint32_t type;
        QString title, name, msg;

        if (notify->NotifyPopupMessage(type, name, msg)) {
            QSettings settings(*mainSettings, QSettings::IniFormat);

            switch (type) {
                    /* id is the name */
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
                case POPUP_UNMATCHED:
                    if (settings.value("Gui/NotifyUnmatched", DEFAULT_NOTIFY_UNMATCHED).toBool()) {
                        mainwindow->trayOpenTo = mainwindow->libraryDialog;
                        mainwindow->trayIcon->showMessage("New library items",
                                                          "You have new items in your library on LibraryMixer that you have not yet setup in the Mixologist.",
                                                          QSystemTrayIcon::Warning);
                    }
                    break;
            }
        }

        if (notify->NotifySysMessage(sysid, type, title, msg)) {
            /* make a warning message */
            switch (type) {
                case SYS_ERROR:
                    QMessageBox::critical(0,title,msg);
                    break;
                case SYS_WARNING:
                    QMessageBox::warning(0,title,msg);
                    break;
                default:
                case SYS_INFO:
                    QMessageBox::information(0,title,msg);
                    break;
            }
        }
    }
}


