/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006-2007, crypton
 *  Copyright 2006, Matt Edman, Justin Hipple
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


#include <QSettings>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <gui/Preferences/TransfersPrefDialog.h>
#include <gui/MainWindow.h> //for settings files
#include <gui/NetworkDialog.h>
#include "gui/Util/OSHelpers.h"
#include <interface/init.h>
#include <interface/iface.h>
#include <interface/files.h>
#include <interface/settings.h>

#define SPINNER_MAX 999999

TransfersPrefDialog::TransfersPrefDialog(QWidget *parent)
    : ConfigPage(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    connect(ui.downloadsButton, SIGNAL(clicked(bool)), this , SLOT(browseDownloadsDirectory()));
    connect(ui.partialsButton, SIGNAL(clicked(bool)), this , SLOT(browsePartialsDirectory()));

    QSettings settings(*mainSettings, QSettings::IniFormat, this);

    ui.incomingAsk->setChecked(settings.value("Transfers/IncomingAsk", DEFAULT_INCOMING_ASK).toBool());

    ui.totalDownloadRate->setValue(settings.value("Transfers/MaxTotalDownloadRate", DEFAULT_MAX_TOTAL_DOWNLOAD).toInt());
    ui.totalUploadRate->setValue(settings.value("Transfers/MaxTotalUploadRate", DEFAULT_MAX_TOTAL_UPLOAD).toInt());
    ui.indivDownloadRate->setValue(settings.value("Transfers/MaxIndividualDownloadRate", DEFAULT_MAX_INDIVIDUAL_DOWNLOAD).toInt());
    ui.indivUploadRate->setValue(settings.value("Transfers/MaxIndividualUploadRate", DEFAULT_MAX_INDIVIDUAL_UPLOAD).toInt());

    // It makes no sense to set the total transfer rate lower than the individual rate.
    // Avoid this by setting the upper limit for individual rate to total transfer rate now,
    // and every time the user changes the total transfer rate.
    setMaxIndivDownloadRate(settings.value("Transfers/MaxTotalDownloadRate", DEFAULT_MAX_TOTAL_DOWNLOAD).toInt());
    setMaxIndivUploadRate(settings.value("Transfers/MaxTotalUploadRate", DEFAULT_MAX_TOTAL_UPLOAD).toInt());
    QObject::connect(ui.totalDownloadRate, SIGNAL(valueChanged(int)), this, SLOT(setMaxIndivDownloadRate(int)));
    QObject::connect(ui.totalUploadRate, SIGNAL(valueChanged(int)), this, SLOT(setMaxIndivUploadRate(int)));

    ui.downloadsDir->setText(files->getDownloadDirectory());
    ui.partialsDir->setText(files->getPartialsDirectory());
}

bool TransfersPrefDialog::save() {
    QSettings settings(*mainSettings, QSettings::IniFormat, this);

    files->setDownloadDirectory(ui.downloadsDir->text());
    files->setPartialsDirectory(ui.partialsDir->text());

    settings.setValue("Transfers/IncomingAsk", ui.incomingAsk->isChecked());
    settings.setValue("Transfers/MaxTotalDownloadRate", ui.totalDownloadRate->value());
    settings.setValue("Transfers/MaxTotalUploadRate", ui.totalUploadRate->value());
    settings.setValue("Transfers/MaxIndividualDownloadRate", ui.indivDownloadRate->value());
    settings.setValue("Transfers/MaxIndividualUploadRate", ui.indivUploadRate->value());
    control->ReloadTransferRates();

    return true;
}

void TransfersPrefDialog::setMaxIndivDownloadRate(int maximum) {
    if (maximum == 0) ui.indivDownloadRate->setMaximum(SPINNER_MAX);
    else ui.indivDownloadRate->setMaximum(maximum);
}
void TransfersPrefDialog::setMaxIndivUploadRate(int maximum) {
    if (maximum == 0) ui.indivUploadRate->setMaximum(SPINNER_MAX);
    else ui.indivUploadRate->setMaximum(maximum);
}

void TransfersPrefDialog::browseDownloadsDirectory() {
    QString qdir = QFileDialog::getExistingDirectory(this, tr("Set Downloads Folder"), ui.downloadsDir->text(),
                   QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    qdir = QDir::toNativeSeparators(qdir);
    if (!qdir.isEmpty()) ui.downloadsDir->setText(qdir);
}

void TransfersPrefDialog::browsePartialsDirectory() {
    QString qdir = QFileDialog::getExistingDirectory(this, tr("Set Temporary File Folder"), ui.partialsDir->text(),
                   QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    qdir = QDir::toNativeSeparators(qdir);
    if (!qdir.isEmpty()) ui.partialsDir->setText(qdir);
}
