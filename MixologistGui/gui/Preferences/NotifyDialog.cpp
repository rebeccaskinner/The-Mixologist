/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006, crypton
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

#include <iostream>
#include <sstream>
#include <QSettings>
#include <QTimer>

#include <interface/notify.h>
#include <gui/Preferences/NotifyDialog.h>
#include <gui/MainWindow.h> //for settings files




/** Constructor */
NotifyDialog::NotifyDialog(QWidget *parent)
    : ConfigPage(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    /* Setup display */
    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    ui.popup_Connect->setChecked(settings.value("Gui/NotifyConnect", DEFAULT_NOTIFY_CONNECT).toBool());
    ui.tray_DownDone->setChecked(settings.value("Gui/NotifyDownloadDone", DEFAULT_NOTIFY_DOWNLOAD_DONE).toBool());
}

/** Saves the changes on this page */
bool NotifyDialog::save() {
    /* extract from notify the flags */
    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    settings.setValue("Gui/NotifyConnect", ui.popup_Connect->isChecked());
    settings.setValue("Gui/NotifyDownloadDone", ui.tray_DownDone->isChecked());
    return true;
}
