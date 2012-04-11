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

#include <gui/LibraryModel.h>
#include <gui/Util/Helpers.h> //for the recursiveFileAdd on drop function
#include <gui/MainWindow.h>
#include <gui/PeersDialog.h>
#include <interface/peers.h>
#include <interface/settings.h>
#include <QTreeView>
#include <QLabel>
#include <QMimeData>
#include <QUrl>
#include <QMenu>
#include <QMessageBox>
#include <QDesktopServices>
#include <QInputDialog>
#include <QFileDialog>
#include <QSettings>

#define IMAGE_CHAT                 ":/Images/Chat.png"
#define IMAGE_INFO                 ":/Images/Info.png"
#define IMAGE_MESSAGE              ":/Images/Message.png"
#define IMAGE_SEND_FILE            ":/Images/Send.png"
#define IMAGE_VIEW_ONLINE          ":/Images/LibraryMixer.png"

LibraryModel::LibraryModel(QTreeView* view, QWidget *parent)
    :QAbstractListModel(parent), ownItemView(view) {

    files->getLibrary(library);

    connect(files, SIGNAL(libraryItemInserted(uint)), this, SLOT(itemInserted(uint)), Qt::QueuedConnection);
    connect(files, SIGNAL(libraryItemRemoved(uint)), this, SLOT(itemRemoved(uint)), Qt::QueuedConnection);
    connect(files, SIGNAL(libraryStateChanged(uint)), this, SLOT(itemStateChanged(uint)), Qt::QueuedConnection);

    connect(ownItemView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customContextMenu(QPoint)));
    /* This can't be done in the constructor because ownItemView isn't setup yet because LibraryDialog hasn't finished constructing yet.
       However, geometriesChanged seems to work, thouhg it ends up being called not only after construction, but also when moving between pages and destruction. */
    connect(ownItemView->header(), SIGNAL(geometriesChanged()), this, SLOT(resizeColumns()));

}

int LibraryModel::columnCount(const QModelIndex &parent) const {
    return 3;
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();

    if (role == Qt::DisplayRole) {
        unsigned int item_id = library.keys()[index.row()];

        QString pathText;
        QStringList paths = library[item_id].paths;
        switch (index.column()){
        case LIBRARY_TITLE_COLUMN:
            return library[item_id].title;
        case LIBRARY_STATUS_COLUMN:
            switch (library[item_id].itemState){
            case LibraryMixerItem::MATCHED_TO_CHAT:
                return "Open a chat window when requested";
            case LibraryMixerItem::MATCHED_TO_MESSAGE:
                return library[item_id].message;
            case LibraryMixerItem::MATCHED_TO_LEND:
                pathText.append("Lend: ");
                for(int i = 0; i < paths.count(); i++) {
                    pathText.append(paths[i]);
                    if (i != (paths.count() - 1)) pathText.append("\n");
                }
                return pathText;
            case LibraryMixerItem::MATCHED_TO_LENT:
                return "Lent to " + peers->getPeerName(library[item_id].lentTo);
            case LibraryMixerItem::MATCHED_TO_FILE:
                for(int i = 0; i < paths.count(); i++) {
                    pathText.append(paths[i]);
                    if (i != (paths.count() - 1)) pathText.append("\n");
                }
                return pathText;
            case LibraryMixerItem::MATCH_NOT_FOUND:
                return "One or more matched files seems to have gone missing";
            case LibraryMixerItem::UNMATCHED:
            default:
                return "";
            }
        default:
            return QVariant();
        }
    }

    return QVariant();
}

Qt::ItemFlags LibraryModel::flags(const QModelIndex &/*index*/) const {
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole){
        switch (section){
        case LIBRARY_TITLE_COLUMN:
            return "Title";
        case LIBRARY_STATUS_COLUMN:
            return "Status";
        }
    }

    return QVariant();
}

int LibraryModel::rowCount(const QModelIndex &/*parent*/) const {
    return library.size();
}

bool LibraryModel::dropMimeData(const QMimeData *data, Qt::DropAction /*action*/, int row, int /*column*/, const QModelIndex &parent) {
    if (!parent.isValid() || parent.row() < 0) return false;

    contextItemId = library.keys()[parent.row()];

    QStringList paths;
    foreach(QUrl url, data->urls()) {
        if (url.scheme() != "file") return false;
        paths << recursiveFileAdd(url.toLocalFile());
    }

    files->setMatchFile(contextItemId, paths, LibraryMixerItem::MATCHED_TO_FILE);
    return true;
}

QStringList LibraryModel::mimeTypes () const {
    QStringList acceptTypes;
    acceptTypes.append("text/uri-list");
    return acceptTypes;
}

Qt::DropActions LibraryModel::supportedDropActions () const {
    return Qt::CopyAction | Qt::MoveAction;
}

void LibraryModel::customContextMenu(QPoint point) {
    QMenu contextMenu(ownItemView);
    QModelIndex index = ownItemView->indexAt(point);
    if (!index.isValid()) return;
    if (index.row() > rowCount()) return;

    contextItemId = library.keys()[index.row()];

    int status = library[contextItemId].itemState;
    if (status == LibraryMixerItem::MATCHED_TO_LENT) {
        QAction *chatBorrowerAct = new QAction(QIcon(IMAGE_CHAT), tr("Chat"), &contextMenu);
        connect(chatBorrowerAct, SIGNAL(triggered()), this, SLOT(chatBorrower()));
        contextMenu.addAction(chatBorrowerAct);
        contextMenu.addSeparator();
    }
    if (status != LibraryMixerItem::MATCHED_TO_MESSAGE) {
        QAction *matchToMessage = new QAction(QIcon(IMAGE_MESSAGE), tr("Auto send a message when requested"), &contextMenu);
        connect(matchToMessage, SIGNAL(triggered()), this, SLOT(setMatchToMessage()));
        contextMenu.addAction(matchToMessage);
    } else {
        QAction *matchToMessage = new QAction(QIcon(IMAGE_MESSAGE), tr("Change auto-response message"), &contextMenu);
        connect(matchToMessage, SIGNAL(triggered()), this, SLOT(setMatchToMessage()));
        contextMenu.addAction(matchToMessage);
    }
    if (status != LibraryMixerItem::MATCHED_TO_FILE &&
        status != LibraryMixerItem::MATCHED_TO_LEND) {
        QAction *matchToFiles = new QAction(QIcon(IMAGE_SEND_FILE), tr("Auto send one or more files when requested"), &contextMenu);
        connect(matchToFiles, SIGNAL(triggered()), this, SLOT(setMatchToFiles()));
        contextMenu.addAction(matchToFiles);

        QAction *matchToLend = new QAction(QIcon(IMAGE_SEND_FILE), tr("Auto lend one or more files when requested"), &contextMenu);
        connect(matchToLend, SIGNAL(triggered()), this, SLOT(setMatchToLend()));
        contextMenu.addAction(matchToLend);
    }
    if (status == LibraryMixerItem::MATCHED_TO_FILE) {
        QAction *filesToLend = new QAction(QIcon(IMAGE_SEND_FILE), tr("Change this to auto lend instead of auto send"), &contextMenu);
        connect(filesToLend, SIGNAL(triggered()), this, SLOT(setFilesToLend()));
        contextMenu.addAction(filesToLend);
    }
    if (status == LibraryMixerItem::MATCHED_TO_LEND) {
        QAction *filesToSend = new QAction(QIcon(IMAGE_SEND_FILE), tr("Change this to auto send instead of auto lend"), &contextMenu);
        connect(filesToSend, SIGNAL(triggered()), this, SLOT(setLendToFiles()));
        contextMenu.addAction(filesToSend);
    }
    if (status != LibraryMixerItem::MATCHED_TO_CHAT) {
        QAction *matchToChat = new QAction(QIcon(IMAGE_CHAT), tr("Change to open a chat window when requested"), &contextMenu);
        connect(matchToChat, SIGNAL(triggered()), this, SLOT(setMatchToChat()));
        contextMenu.addAction(matchToChat);
    }

    contextMenu.addSeparator();

    QAction *help = new QAction(QIcon(IMAGE_INFO), tr("Huh? What does this all mean?"), &contextMenu);
    connect(help, SIGNAL(triggered()), this, SLOT(showHelp()));
    contextMenu.addAction(help);

    contextMenu.addSeparator();

    QAction *openOnline = new QAction(QIcon(IMAGE_VIEW_ONLINE), tr("See more details online"), &contextMenu);
    connect(openOnline, SIGNAL(triggered()), this, SLOT(openOnline()));
    contextMenu.addAction(openOnline);

    contextMenu.exec(ownItemView->mapToGlobal(point));
}

void LibraryModel::setMatchToMessage() {
    bool ok;
    QString defaultText = "";

    if (library.contains(contextItemId)) {
        if (library[contextItemId].itemState == LibraryMixerItem::MATCHED_TO_MESSAGE) defaultText = library[contextItemId].message;

        QString text = QInputDialog::getText(ownItemView, tr("Auto Response"), tr("Enter your auto response message:"), QLineEdit::Normal, defaultText, &ok);
        if (ok) {
            files->setMatchMessage(contextItemId, text);
        }
    }
}

void LibraryModel::setMatchToFiles() {
    QStringList paths = QFileDialog::getOpenFileNames(ownItemView, "It's usually easier to just drag and drop, but you can also do it this way");
    if (!paths.empty()) {
        files->setMatchFile(contextItemId, paths, LibraryMixerItem::MATCHED_TO_FILE);
    }
}

void LibraryModel::setMatchToLend() {
    QStringList paths = QFileDialog::getOpenFileNames(ownItemView, "");
    if (!paths.empty()) {
        files->setMatchFile(contextItemId, paths, LibraryMixerItem::MATCHED_TO_LEND);
    }
}

void LibraryModel::setMatchToChat() {
    files->setMatchChat(contextItemId);
}

void LibraryModel::setFilesToLend() {
    files->setMatchFile(contextItemId, QStringList(), LibraryMixerItem::MATCHED_TO_LEND);
}

void LibraryModel::setLendToFiles() {
    files->setMatchFile(contextItemId, QStringList(), LibraryMixerItem::MATCHED_TO_FILE);
}

void LibraryModel::showHelp() {
    QMessageBox helpBox(ownItemView);
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

void LibraryModel::chatBorrower() {
    if (library.contains(contextItemId))
        mainwindow->peersDialog->getOrCreateChat(library[contextItemId].lentTo, true);
}

void LibraryModel::openOnline() {
    QSettings settings(*startupSettings, QSettings::IniFormat);
    QString host = settings.value("MixologyServer", DEFAULT_MIXOLOGY_SERVER).toString();

    if (host.compare(DEFAULT_MIXOLOGY_SERVER, Qt::CaseInsensitive) == 0){
        host = DEFAULT_MIXOLOGY_SERVER_FIXED_VALUE;
    }

    QDesktopServices::openUrl(QUrl(host + "/redirect/item/" + QString::number(contextItemId)));
}

void LibraryModel::resizeColumns() {
    ownItemView->resizeColumnToContents(LIBRARY_TITLE_COLUMN);
    ownItemView->resizeColumnToContents(LIBRARY_STATUS_COLUMN);
}

void LibraryModel::itemInserted(unsigned int item_id) {
    LibraryMixerItem item;
    if (!files->getLibraryMixerItem(item_id, item)) return;

    /* Find where the item will fall in the order. */
    int currentIndex;
    for (currentIndex = 0; currentIndex < library.keys().size(); currentIndex ++) {
        if (library.keys()[currentIndex] < item_id) break;
    }

    beginInsertRows(QModelIndex(), currentIndex, currentIndex);
    library.insert(item_id, item);
    endInsertRows();
    resizeColumns();
}

void LibraryModel::itemRemoved(unsigned int item_id) {
    int index = library.keys().indexOf(item_id);

    if (index == -1) return;

    beginRemoveRows(QModelIndex(), index, index);
    library.remove(item_id);
    endRemoveRows();
}

void LibraryModel::itemStateChanged(unsigned int item_id) {
    LibraryMixerItem item;
    if (!files->getLibraryMixerItem(item_id, item)) return;

    library[item_id] = item;
    int row = library.keys().indexOf(item_id);
    emit dataChanged(createIndex(row, LIBRARY_TITLE_COLUMN), createIndex(row, LIBRARY_STATUS_COLUMN));
    resizeColumns();
}
