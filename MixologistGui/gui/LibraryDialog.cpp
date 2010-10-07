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


#include "LibraryDialog.h"
#include "gui/MainWindow.h" //for mainwindow variable
#include "gui/PeersDialog.h"
#include "interface/librarymixer-connect.h"
#include "interface/librarymixer-library.h"
#include "interface/iface.h"
#include "interface/files.h"

#include <QTimer>
#include <QMenu>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>

#define IMAGE_OPEN                 ":/Images/Play.png"

/** Constructor */
LibraryDialog::LibraryDialog(QWidget *parent)
    : QWidget(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    connect(ui.unmatchedList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(unmatchedContextMenu(QPoint)));
    connect(ui.matchedList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(matchedContextMenu(QPoint)));
    connect(ui.updateLibraryButton, SIGNAL( clicked()), this, SLOT(updateLibrary()));
    connect(librarymixerconnect, SIGNAL(downloadedLibrary()), this, SLOT(updatedLibrary()));

    ui.matchedList->sortItems(0, Qt::AscendingOrder);
    ui.unmatchedList->sortItems(0, Qt::AscendingOrder);
}

void LibraryDialog::updateLibrary() {
    QMovie *movie = new QMovie(":/Images/AnimatedLoading.gif");
    ui.updateLibraryLabel->setMovie(movie);
    movie->start();
    movie->setSpeed(100); // 2x speed
    ui.updateLibraryLabel->setToolTip("Downloading library") ;
    ui.updateLibraryButton->setEnabled(false);
    if (librarymixerconnect->downloadLibrary() < 0) updatedLibrary();
}

void LibraryDialog::updatedLibrary() {
    ui.updateLibraryLabel->clear();
    ui.updateLibraryButton->setEnabled(true);
    insertLibrary();
}

void LibraryDialog::setMatchToMessage() {
    bool ok;
    QString text = QInputDialog::getText(this, tr("Auto Response"), tr("Enter your auto response message:"), QLineEdit::Normal, "", &ok);
    if (ok) {
        LibraryMixerLibraryManager::setMatchMessage(contextItem->text(ID_COLUMN).toInt(), text);
    }
}

void LibraryDialog::setMatchToFiles() {
    QStringList paths = QFileDialog::getOpenFileNames(this, "It's usually easier to just drag and drop, but you can also do it this way");
    if (!paths.empty()) {
        LibraryMixerLibraryManager::setMatchFile(contextItem->text(ID_COLUMN).toInt(), paths, ITEM_MATCHED_TO_FILE);
    }
}

void LibraryDialog::setMatchToLend() {
    QStringList paths = QFileDialog::getOpenFileNames(this, "");
    if (!paths.empty()) {
        LibraryMixerLibraryManager::setMatchFile(contextItem->text(ID_COLUMN).toInt(), paths, ITEM_MATCHED_TO_LEND);
    }
}

void LibraryDialog::setMatchToChat() {
    contextBox->setResponseChat(contextItem);
    LibraryMixerLibraryManager::setMatchChat(contextItem->text(ID_COLUMN).toInt());
}

void LibraryDialog::setFilesToLend() {
    LibraryMixerLibraryManager::setMatchFile(contextItem->text(ID_COLUMN).toInt(), QStringList(), ITEM_MATCHED_TO_LEND);
}

void LibraryDialog::setLendToFiles() {
    LibraryMixerLibraryManager::setMatchFile(contextItem->text(ID_COLUMN).toInt(), QStringList(), ITEM_MATCHED_TO_FILE);
}

void LibraryDialog::showHelp() {
    QMessageBox helpBox(this);
    helpBox.setText("Setting automatic responses");
    QString info = "<p>By default, when your friends ask for stuff in your library on the Mixologist, it'll pop up a chat window so you can talk it over. As an optional alternative, you can set automatic responses for each thing in your library.</p>";
    info += "<p>When a friend makes a request, you can set the Mixologist to:</p>";
    info += "<ul><li>Automatically send a message you choose over chat - <b>right click and choose auto send a message.</b></li>";
    info += "<li>Automatically send one or more files - <b>drag the files onto the title</b> of what you're matching the files to.</li>";
    info += "<li>Automatically lend files - <b>right click and choose auto lend</b>. After the file or files are sent to your friend, they will be removed from your computer until your friend sends them back to you. When your friends send them back, they will automatically be put back in their original spots.</li>";
    info += "<li>Always open a chat window and never ask you to set an automatic response again - <b>right click and choose chat window.</b></li></ul>";
    info += "<p>Or, you can just do nothing, and leave things without an automatic response. If you do, when your friends ask you for something, you'll be given the chance to set the permanent response for all future requests for that thing from friends.</p>";
    helpBox.setInformativeText(info);
    helpBox.setTextFormat(Qt::RichText);
    helpBox.exec();
}

void LibraryDialog::chatBorrower() {
    mainwindow->peersDialog->getOrCreateChat(contextItem->text(BORROWER_COLUMN).toInt(), true);
}

void LibraryDialog::openFile() {
    QString target = contextItem->text(STATUS_COLUMN);
    if (target.startsWith(LEND_PREFIX)) target.remove(0, LEND_PREFIX.length());
    QDesktopServices::openUrl(QUrl("file:///" + target, QUrl::TolerantMode));
}


void LibraryDialog::insertLibrary() {
    ui.unmatchedList->setDisplay(LibraryMixerLibraryManager::getUnmatched());
    ui.matchedList->setDisplay(LibraryMixerLibraryManager::getMatched());

    if (ui.unmatchedList->topLevelItemCount() == 0) ui.unmatchedGroup->setVisible(false);
    else ui.unmatchedGroup->setVisible(true);
}

void LibraryDialog::unmatchedContextMenu(QPoint point) {
    QMenu contextMenu(this);
    contextItem = ui.unmatchedList->itemAt(point);
    if (contextItem == NULL) return;
    contextBox = ui.unmatchedList;

    QAction *matchToMessage = new QAction(tr("Auto send a message when requested"), &contextMenu);
    connect(matchToMessage, SIGNAL(triggered()), this, SLOT(setMatchToMessage()));
    contextMenu.addAction(matchToMessage);

    QAction *matchToFiles = new QAction(tr("Auto send one or more files when requested"), &contextMenu);
    connect(matchToFiles, SIGNAL(triggered()), this, SLOT(setMatchToFiles()));
    contextMenu.addAction(matchToFiles);

    QAction *matchToLend = new QAction(tr("Auto lend one or more files when requested"), &contextMenu);
    connect(matchToLend, SIGNAL(triggered()), this, SLOT(setMatchToLend()));
    contextMenu.addAction(matchToLend);

    contextMenu.addSeparator();
    QAction *matchToChat = new QAction(tr("Always open a chat window and never ask to set an automatic response again"), &contextMenu);
    connect(matchToChat, SIGNAL(triggered()), this, SLOT(setMatchToChat()));
    contextMenu.addAction(matchToChat);

    contextMenu.addSeparator();

    QAction *help = new QAction(tr("Huh? What does this all mean?"), &contextMenu);
    connect(help, SIGNAL(triggered()), this, SLOT(showHelp()));
    contextMenu.addAction(help);

    contextMenu.exec(ui.unmatchedList->mapToGlobal(point));
}

void LibraryDialog::matchedContextMenu(QPoint point) {
    QMenu contextMenu(this);
    contextItem = ui.matchedList->itemAt(point);
    if (contextItem == NULL) return;
    contextBox = ui.matchedList;

    int status = LibraryMixerLibraryManager::getLibraryMixerItemStatus(contextItem->text(ID_COLUMN).toInt());
    if (status == ITEM_MATCHED_TO_LENT) {
        QAction *chatBorrowerAct = new QAction(tr("Chat"), &contextMenu);
        connect(chatBorrowerAct, SIGNAL(triggered()), this, SLOT(chatBorrower()));
        contextMenu.addAction(chatBorrowerAct);
        contextMenu.addSeparator();
    }
    if (status == ITEM_MATCHED_TO_FILE ||
            status == ITEM_MATCHED_TO_LEND) {
        QAction *openAct = new QAction(QIcon(IMAGE_OPEN), tr("Open"), &contextMenu);
        connect(openAct, SIGNAL(triggered()), this, SLOT(openFile()));
        contextMenu.addAction(openAct);
        contextMenu.addSeparator();
    }
    if (status != ITEM_MATCHED_TO_MESSAGE) {
        QAction *matchToMessage = new QAction(tr("Auto send a message when requested"), &contextMenu);
        connect(matchToMessage, SIGNAL(triggered()), this, SLOT(setMatchToMessage()));
        contextMenu.addAction(matchToMessage);
    }
    if (status != ITEM_MATCHED_TO_FILE &&
            status != ITEM_MATCHED_TO_LEND) {
        QAction *matchToFiles = new QAction(tr("Auto send one or more files when requested"), &contextMenu);
        connect(matchToFiles, SIGNAL(triggered()), this, SLOT(setMatchToFiles()));
        contextMenu.addAction(matchToFiles);

        QAction *matchToLend = new QAction(tr("Auto lend one or more files when requested"), &contextMenu);
        connect(matchToLend, SIGNAL(triggered()), this, SLOT(setMatchToLend()));
        contextMenu.addAction(matchToLend);
    }
    if (status == ITEM_MATCHED_TO_FILE) {
        QAction *filesToLend = new QAction(tr("Change this to auto lend instead of auto send"), &contextMenu);
        connect(filesToLend, SIGNAL(triggered()), this, SLOT(setFilesToLend()));
        contextMenu.addAction(filesToLend);
    }
    if (status == ITEM_MATCHED_TO_LEND) {
        QAction *filesToSend = new QAction(tr("Change this to auto send instead of auto lend"), &contextMenu);
        connect(filesToSend, SIGNAL(triggered()), this, SLOT(setLendToFiles()));
        contextMenu.addAction(filesToSend);
    }
    if (status != ITEM_MATCHED_TO_CHAT) {
        QAction *matchToChat = new QAction(tr("Change to open a chat window when requested"), &contextMenu);
        connect(matchToChat, SIGNAL(triggered()), this, SLOT(setMatchToChat()));
        contextMenu.addAction(matchToChat);
        contextMenu.addSeparator();
    }

    QAction *help = new QAction(tr("Huh? What does this all mean?"), &contextMenu);
    connect(help, SIGNAL(triggered()), this, SLOT(showHelp()));
    contextMenu.addAction(help);

    contextMenu.exec(ui.matchedList->mapToGlobal(point));
}
