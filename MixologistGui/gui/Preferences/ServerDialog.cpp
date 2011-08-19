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
#include "gui/MainWindow.h" //for settings files
#include <QSettings>
#include <iostream>
#include <sstream>

#include "interface/iface.h"
#include "interface/peers.h"

#include <QTimer>

/** Constructor */
ServerDialog::ServerDialog(QWidget *parent)
    : ConfigPage(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    connect(ui.mixologyServer, SIGNAL(editingFinished()), this, SLOT(editedServer()));
    //        connect(ui.randomizePorts, SIGNAL(toggled(bool)), this, SLOT(randomizeToggled(bool)));

    /* Setup display */
    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    ui.UPNP->setChecked(settings.value("Network/UPNP", DEFAULT_UPNP).toBool());
    ui.randomizePorts->setChecked(settings.value("Network/RandomizePorts", DEFAULT_RANDOM_PORTS).toBool());

    //Until we can save port settings, these are disabled
    ui.localPort  -> setEnabled(false);
    ui.extPort    -> setEnabled(false);

    /* load up configuration from peers */
    PeerDetails detail;
    if (!peers->getPeerDetails(peers->getOwnLibraryMixerId(), detail)) {
        return;
    }

    /* set net mode */
    int netIndex = 0;
    switch (detail.tryNetMode) {
        case NETMODE_EXT:
            netIndex = 2;
            break;
        case NETMODE_UDP:
            netIndex = 1;
            break;
        default:
        case NETMODE_UPNP:
            netIndex = 0;
            break;
    }
    ui.netModeComboBox->setCurrentIndex(netIndex);

    /* set dht */
    netIndex = 1;
    if (detail.visState & VS_DHT_ON) {
        netIndex = 0;
    }
    ui.dhtComboBox->setCurrentIndex(netIndex);

    //Temporarily disabled
    //toggleUPnP();

    /* Addresses must be set here - otherwise can't edit it */
    /* set local address */
    ui.localAddress->setText(QString::fromStdString(detail.localAddr));
    ui.localPort -> setValue(detail.localPort);
    /* set the server address */
    ui.extAddress->setText(QString::fromStdString(detail.extAddr));
    ui.extPort -> setValue(detail.extPort);

    ui.localAddress->setEnabled(false);
    ui.extAddress -> setEnabled(false);

    //Temporarily disabled
    ui.NetConfigBox->setVisible(false);

    QSettings serverSettings(*startupSettings, QSettings::IniFormat, this);
    ui.mixologyServer->setText(serverSettings.value("MixologyServer", DEFAULT_MIXOLOGY_SERVER).toString());
}

/** Saves the changes on this page */
bool ServerDialog::save() {

    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    settings.setValue("Network/UPNP", ui.UPNP->isChecked());
    //RandomizePorts requires UPNP
    settings.setValue("Network/RandomizePorts", ui.UPNP->isChecked() && ui.randomizePorts->isChecked());
    QSettings serverSettings(*startupSettings, QSettings::IniFormat, this);
    serverSettings.setValue("MixologyServer", ui.mixologyServer->text());
#ifdef false
    bool saveAddr = false;

    PeerDetails detail;
    std::string ownId = peers->getOwnCertId();

    if (!peers->getPeerDetails(ownId, detail)) return false;

    int netIndex = ui.netModeComboBox->currentIndex();

    /* Check if netMode has changed */
    uint32_t netMode = 0;
    switch (netIndex) {
        case 2:
            netMode = NETMODE_EXT;
            break;
        case 1:
            netMode = NETMODE_UDP;
            break;
        default:
        case 0:
            netMode = NETMODE_UPNP;
            break;
    }

    if (detail.tryNetMode != netMode) {
        peers->setNetworkMode(ownId, netMode);
    }

    uint32_t visState = 0;
    /* Check if vis has changed */
    if (0 == ui.dhtComboBox->currentIndex()) {
        visState |= VS_DHT_ON;
    }

    if (visState != detail.visState) {
        peers->setVisState(ownId, visState);
    }

    if (0 != netIndex) {
        saveAddr = true;
    }

    if (saveAddr) {
        peers->setLocalAddress(peers->getOwnCertId(), ui.localAddress->text().toStdString(), ui.localPort->value());
        peers->setExtAddress(peers->getOwnCertId(), ui.extAddress->text().toStdString(), ui.extPort->value());
    }
#endif
    return true;
}

void ServerDialog::editedServer(){
    if (ui.mixologyServer->text().isEmpty()){
        ui.mixologyServer->setText(DEFAULT_MIXOLOGY_SERVER);
    }
}

//void ServerDialog::randomizeToggled(bool set){
//    ui.localPort  -> setEnabled(!set);
//    ui.extPort    -> setEnabled(!set);
//}

#ifdef false
void ServerDialog::toggleUPnP() {
    /* switch on the radioButton */
    bool settingChangeable = false;
    if (0 != ui.netModeComboBox->currentIndex()) {
        settingChangeable = true;
    }

    if (settingChangeable) {
        ui.dhtComboBox->setEnabled(true);

        ui.localAddress->setEnabled(false);
        ui.localPort  -> setEnabled(true);
        ui.extAddress -> setEnabled(false);
        ui.extPort    -> setEnabled(true);
    } else {
        ui.dhtComboBox->setEnabled(false);

        ui.localAddress->setEnabled(false);
        ui.localPort  -> setEnabled(false);
        ui.extAddress -> setEnabled(false);
        ui.extPort    -> setEnabled(false);
    }
}
#endif
