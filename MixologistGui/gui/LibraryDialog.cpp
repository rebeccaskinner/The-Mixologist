/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006, crypton
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


#include "gui/LibraryDialog.h"
#include "gui/LibraryModel.h"
#include "gui/MainWindow.h" //for mainwindow variable
#include "gui/PeersDialog.h"
#include "gui/OffLMOwnModel.h"
#include "interface/librarymixer-connect.h"
#include "interface/iface.h"
#include "interface/files.h"
#include "interface/settings.h"
#include <QDesktopServices>
#include <QMessageBox>

LibraryDialog::LibraryDialog(QWidget *parent)
    : QWidget(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    /* Setup Library Model. */
    libraryModel = new LibraryModel(ui.libraryList, this);
    ui.libraryList->setModel(libraryModel);

    connect(ui.addLibraryButton, SIGNAL(clicked()), this, SLOT(addLibraryClicked()));
    connect(ui.updateLibraryButton, SIGNAL(clicked()), this, SLOT(updateLibrary()));
    connect(librarymixerconnect, SIGNAL(downloadedLibrary()), this, SLOT(updatedLibrary()), Qt::QueuedConnection);

    /* Setup Off-LM Model. */
    OffLMOwnModel* ownModel = new OffLMOwnModel(ui.offLMList, this);
    ui.offLMList->setModel(ownModel);

    offLMHeaderSpacer = ui.offLMHeaderSpacer;

    QSettings settings(*mainSettings, QSettings::IniFormat, this);
    showAdvanced(settings.value("Gui/ShowAdvanced", DEFAULT_SHOW_ADVANCED).toBool());
}

void LibraryDialog::showAdvanced(bool enable) {
    ui.offLMList->setVisible(enable);
    ui.offLMListLabel->setVisible(enable);
    ui.offLMListIcon->setVisible(enable);
    if (enable) {
        /* We might end up removing it even if it isn't showing with this, but that's harmless.
           However, double-adding results in a segfault on quit when the offLMHeaderLayout tries to free the spacer twice. */
        ui.offLMHeaderLayout->removeItem(offLMHeaderSpacer);
        ui.offLMHeaderLayout->addSpacerItem(offLMHeaderSpacer);
    } else {
        ui.offLMHeaderLayout->removeItem(offLMHeaderSpacer);
    }
}

void LibraryDialog::addLibraryClicked() {
    QSettings settings(*startupSettings, QSettings::IniFormat);
    QString host = settings.value("MixologyServer", DEFAULT_MIXOLOGY_SERVER).toString();

    if (host.compare(DEFAULT_MIXOLOGY_SERVER, Qt::CaseInsensitive) == 0){
        host = DEFAULT_MIXOLOGY_SERVER_FIXED_VALUE;
    }

    QDesktopServices::openUrl(QUrl(host + "/library"));

    QTimer::singleShot(2000, this, SLOT(addLibraryClickedComplete()));
}

void LibraryDialog::addLibraryClickedComplete() {
    /* We don't just use the static QMessageBox::information box because, at least on Windows, it makes the annoying system beep when it pops up. */
    QMessageBox msgBox;
    msgBox.setWindowTitle("The Mixologist");
    msgBox.setText("<p>Hit ok when you are done making changes to your library to update the Mixologist.</p><p>(Or you can always hit Update Library in the upper-right later.)</p>");
    msgBox.exec();

    updateLibrary();
}

void LibraryDialog::updateLibrary() {
    ui.updateLibraryButton->setEnabled(false);
    /* In addition to re-downloading the library from LibraryMixer, we also want to recheck all files.
       This is really pretty inelegant architecture here, in that how LibraryMixer files and off-LibraryMixer are updated looks so different.
       librarymixerconnect->downloadLibrary already does that as a side-effect, so we just need to manually call for an update of the off-LibraryMixer files. */
    files->recheckOffLMFiles();
    if (librarymixerconnect->downloadLibrary() < 0) updatedLibrary();
}

void LibraryDialog::updatedLibrary() {
    ui.updateLibraryButton->setEnabled(true);
}
