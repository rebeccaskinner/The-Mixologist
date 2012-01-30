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

#include "gui/FriendsLibraryDialog.h"
#include "gui/LibraryFriendModel.h"
#include "gui/OffLMFriendModel.h"
#include <interface/iface.h>
#include <interface/librarymixer-connect.h>
#include <interface/settings.h>
#include <QSortFilterProxyModel>

/** Constructor */
FriendsLibraryDialog::FriendsLibraryDialog(QWidget *parent)
    : QWidget(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    connect(ui.updateLibraryButton, SIGNAL(clicked()), this, SLOT(updateLibrary()));
    connect(librarymixerconnect, SIGNAL(downloadedFriendsLibrary()), this, SLOT(updatedLibrary()));

    /* Setup LibraryFriendModel */
    LibraryFriendModel* libraryFriendModel = new LibraryFriendModel(ui.libraryList, this);
    LibrarySearchModel* librarySearchModel = new LibrarySearchModel(ui.searchBar, this);
    librarySearchModel->setSourceModel(libraryFriendModel);
    ui.libraryList->setModel(librarySearchModel);

    /* Setup OffLMFriendModel */
    QSettings settings(*mainSettings, QSettings::IniFormat);
    if (settings.value("Gui/EnableOffLibraryMixer", DEFAULT_ENABLE_OFF_LIBRARYMIXER_SHARING).toBool()) {
        OffLMFriendModel* friendModel = new OffLMFriendModel(ui.offLMList, this);
        OffLMSearchModel* searchableFriendModel = new OffLMSearchModel(ui.searchBar, this);
        searchableFriendModel->setSourceModel(friendModel);
        friendModel->setContainingSearchModel(searchableFriendModel);
        ui.offLMList->setModel(searchableFriendModel);
    } else {
        ui.offLMList->setVisible(false);
        ui.offLMListLabel->setVisible(false);
        ui.offLMListIcon->setVisible(false);
        ui.offLMHeaderLayout->removeItem(ui.offLMHeaderSpacer);
    }
}

void FriendsLibraryDialog::updateLibrary() {
    ui.updateLibraryButton->setEnabled(false);
    if (librarymixerconnect->downloadFriendsLibrary() < 0) updatedLibrary();
}

void FriendsLibraryDialog::updatedLibrary() {
    ui.updateLibraryButton->setEnabled(true);
}
