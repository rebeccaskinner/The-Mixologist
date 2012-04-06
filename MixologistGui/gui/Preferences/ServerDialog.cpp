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


#include "ServerDialog.h"
#include "interface/settings.h"

#include "interface/iface.h"
#include "interface/peers.h"

ServerDialog::ServerDialog(QWidget *parent)
    : ConfigPage(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    connect(ui.mixologyServer, SIGNAL(editingFinished()), this, SLOT(editedServer()));
    connect(ui.disableAutoConfig, SIGNAL(clicked(bool)), this, SLOT(autoConfigClicked(bool)));

    /* Setup display */
    QSettings settings(*mainSettings, QSettings::IniFormat, this);

    /* Load up actual port number  */
    PeerDetails detail;
    if (peers->getPeerDetails(peers->getOwnLibraryMixerId(), detail)) {
        ui.portNumber->setValue(detail.localPort);
    }
    ui.portNumber->setMinimum(Peers::MIN_PORT);
    ui.portNumber->setMaximum(Peers::MAX_PORT);

    if (settings.value("Network/AutoOrPort", DEFAULT_NETWORK_AUTO_OR_PORT) == DEFAULT_NETWORK_AUTO_OR_PORT) {
        ui.disableAutoConfig->setChecked(false);
    } else {
        ui.disableAutoConfig->setChecked(true);;
    }

    QSettings serverSettings(*startupSettings, QSettings::IniFormat, this);
    ui.mixologyServer->setText(serverSettings.value("MixologyServer", DEFAULT_MIXOLOGY_SERVER).toString());

    showAdvanced(settings.value("Gui/ShowAdvanced", DEFAULT_SHOW_ADVANCED).toBool());
}

bool ServerDialog::save() {
    QSettings settings(*mainSettings, QSettings::IniFormat, this);

    /* We'll need to restart the connection if auto-config has been disabled or enabled, or if it has remained disabled but the port number has changed. */
    bool autoConfigEnabled = (settings.value("Network/AutoOrPort", DEFAULT_NETWORK_AUTO_OR_PORT) == DEFAULT_NETWORK_AUTO_OR_PORT);
    bool needConnectionRestart = ((ui.disableAutoConfig->isChecked() == autoConfigEnabled) ||
                                  (!autoConfigEnabled && (ui.portNumber->value() != settings.value("Network/AutoOrPort", DEFAULT_NETWORK_AUTO_OR_PORT).toInt())));

    if (ui.disableAutoConfig->isChecked()) {
        settings.setValue("Network/AutoOrPort", ui.portNumber->value());
    } else {
        settings.setValue("Network/AutoOrPort", DEFAULT_NETWORK_AUTO_OR_PORT);
    }
    QSettings serverSettings(*startupSettings, QSettings::IniFormat, this);
    serverSettings.setValue("MixologyServer", ui.mixologyServer->text());

    if (needConnectionRestart) peers->restartOwnConnection();

    return true;
}

void ServerDialog::showAdvanced(bool enabled) {
    ui.remoteBox->setVisible(enabled);
    /* Only show the ports if we are both on advanced mode and auto-config is disabled. */
    ui.portNumber->setVisible(enabled && ui.disableAutoConfig->isChecked());
    ui.portLabel->setVisible(enabled && ui.disableAutoConfig->isChecked());
}

void ServerDialog::editedServer(){
    if (ui.mixologyServer->text().isEmpty()){
        ui.mixologyServer->setText(DEFAULT_MIXOLOGY_SERVER);
    }
}

void ServerDialog::autoConfigClicked(bool autoConfigDisabled) {
    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    /* Only show the ports if we are both on advanced mode and auto-config is disabled. */
    ui.portNumber->setVisible((autoConfigDisabled && settings.value("Gui/ShowAdvanced", DEFAULT_SHOW_ADVANCED).toBool()));
    ui.portLabel->setVisible((autoConfigDisabled && settings.value("Gui/ShowAdvanced", DEFAULT_SHOW_ADVANCED).toBool()));
}
