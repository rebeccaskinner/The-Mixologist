/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006-2007, crypton
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

#include <gui/Toaster/OnlineToaster.h>
#include <gui/Toaster/QtToaster.h>
#include <gui/MainWindow.h>
#include <gui/PeersDialog.h>
#include <interface/notify.h> //for chat flag constants

OnlineToaster::OnlineToaster()
    : QObject(NULL) {

    _onlineToasterWidget = new QWidget(NULL);

    _ui = new Ui::OnlineToaster();
    _ui->setupUi(_onlineToasterWidget);

    connect(_ui->chatButton, SIGNAL(clicked()), SLOT(chatButtonSlot()));

    _toaster = new QtToaster(_onlineToasterWidget);
    _toaster->setTimeOnTop(5000);
}

OnlineToaster::~OnlineToaster() {
    delete _ui;
}

void OnlineToaster::setMessage(const QString &message) {
    _ui->FriendNameLabel->setText(peers->getPeerName(message.toInt()));
    librarymixer_id = message.toInt();
}

void OnlineToaster::setPixmap(const QPixmap &pixmap) {
    (void)pixmap;
    return;
}

void OnlineToaster::show() {
    _toaster->show();
}

void OnlineToaster::close() {
    _toaster->close();
}

void OnlineToaster::chatButtonSlot() {
    mainwindow->peersDialog->getOrCreateChat(librarymixer_id, true);
    close();
}
