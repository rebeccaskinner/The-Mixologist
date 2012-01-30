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

#include <gui/LibraryFriendModel.h>
#include <gui/MainWindow.h>
#include <gui/PeersDialog.h>
#include <gui/TransfersDialog.h>
#include "interface/files.h"
#include "interface/peers.h"

#include <QMenu>
#include <QTreeView>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>

#define IMAGE_CHAT                 ":/Images/Chat.png"
#define IMAGE_DOWNLOAD             ":/Images/Download.png"
#define IMAGE_VIEW_ONLINE          ":/Images/LibraryMixer.png"

LibraryFriendModel::LibraryFriendModel(QTreeView* view, QWidget *parent)
    :QAbstractListModel(parent), ownItemView(view) {
    itemList = files->getFriendLibrary();

    connect(files, SIGNAL(friendLibraryItemAboutToBeInserted(int)), this, SLOT(itemAboutToBeInserted(int)), Qt::DirectConnection);
    connect(files, SIGNAL(friendLibraryItemAboutToBeRemoved(int)), this, SLOT(itemAboutToBeRemoved(int)), Qt::DirectConnection);
    connect(files, SIGNAL(friendLibraryItemInserted()), this, SLOT(itemInserted()));
    connect(files, SIGNAL(friendLibraryItemRemoved()), this, SLOT(itemRemoved()));
    connect(files, SIGNAL(friendLibraryStateChanged(int)), this, SLOT(itemStateChanged(int)));

    connect(ownItemView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customContextMenu(QPoint)));
    /* This can't be done in the constructor because ownItemView isn't setup yet because LibraryDialog hasn't finished constructing yet.
       However, geometriesChanged seems to work, thouhg it ends up being called not only after construction, but also when moving between pages and destruction. */
    connect(ownItemView->header(), SIGNAL(geometriesChanged()), this, SLOT(resizeColumns()));

}

int LibraryFriendModel::columnCount(const QModelIndex &parent) const {
    return 2;
}

QVariant LibraryFriendModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();

    if (role == Qt::DisplayRole) {

        FriendLibraryMixerItem* item = itemList->values().at(index.row());

        switch (index.column()){
        case FRIEND_LIBRARY_TITLE_COLUMN:
            return item->title();
        case FRIEND_LIBRARY_FRIEND_COLUMN:
            return peers->getPeerName(item->friend_id());
        default:
            return QVariant();
        }
    }

    return QVariant();
}

QVariant LibraryFriendModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole){
        switch (section){
        case FRIEND_LIBRARY_TITLE_COLUMN:
            return "Title";
        case FRIEND_LIBRARY_FRIEND_COLUMN:
            return "Friend";
        }
    }

    return QVariant();
}

int LibraryFriendModel::rowCount(const QModelIndex &/*parent*/) const {
    return itemList->count();
}

void LibraryFriendModel::customContextMenu(QPoint point) {
    QMenu contextMenu(ownItemView);
    QModelIndex index = ownItemView->indexAt(point);
    if (!index.isValid()) return;
    if (index.row() > rowCount()) return;
    const FriendLibraryMixerItem* contextItem = itemList->values().at(index.row());
    contextItemId = contextItem->item_id();
    bool friendOnline = peers->isOnline(contextItem->friend_id());

    QAction *requestAct = new QAction(QIcon(IMAGE_DOWNLOAD), tr("Ask for it"), &contextMenu);
    connect(requestAct, SIGNAL(triggered()), this, SLOT(requestItem()));
    contextMenu.addAction(requestAct);
    if (!friendOnline) {
        requestAct->setText(tr("Ask for it when friend online"));
    }

    contextMenu.addSeparator();

    QAction *chatAct = new QAction(QIcon(IMAGE_CHAT), tr("Chat"), &contextMenu);
    connect(chatAct, SIGNAL(triggered()), this, SLOT(chatFriend()));
    contextMenu.addAction(chatAct);
    if (!friendOnline){
        chatAct->setEnabled(false);
        chatAct->setText(tr("Chat (friend offline)"));
    }

    contextMenu.addSeparator();

    QAction *openOnline = new QAction(QIcon(IMAGE_VIEW_ONLINE), tr("See more details online"), &contextMenu);
    connect(openOnline, SIGNAL(triggered()), this, SLOT(openOnline()));
    contextMenu.addAction(openOnline);

    contextMenu.exec(ownItemView->mapToGlobal(point));
}

void LibraryFriendModel::requestItem() {
    FriendLibraryMixerItem* requestedItem = itemList->value(contextItemId);
    files->LibraryMixerRequest(requestedItem->friend_id(), requestedItem->item_id(), requestedItem->title());

    /* Change display to transfers dialog to provide visual feedback to user of action. */
    mainwindow->switchToDialog(mainwindow->transfersDialog);
}

void LibraryFriendModel::chatFriend() {
    mainwindow->peersDialog->getOrCreateChat(itemList->value(contextItemId)->friend_id(), true);
}

void LibraryFriendModel::openOnline() {
    QSettings settings(*startupSettings, QSettings::IniFormat);
    QString host = settings.value("MixologyServer", DEFAULT_MIXOLOGY_SERVER).toString();

    if (host.compare(DEFAULT_MIXOLOGY_SERVER, Qt::CaseInsensitive) == 0){
        host = DEFAULT_MIXOLOGY_SERVER_VALUE;
    }

    QDesktopServices::openUrl(QUrl(host + "/redirect/item/" + QString::number(contextItemId)));
}

void LibraryFriendModel::resizeColumns() {
    ownItemView->resizeColumnToContents(FRIEND_LIBRARY_TITLE_COLUMN);
    ownItemView->resizeColumnToContents(FRIEND_LIBRARY_FRIEND_COLUMN);
}

void LibraryFriendModel::itemAboutToBeInserted(int row) {
    beginInsertRows(QModelIndex(), row, row);
}

void LibraryFriendModel::itemInserted() {
    endInsertRows();
    resizeColumns();
}

void LibraryFriendModel::itemAboutToBeRemoved(int row) {
    beginRemoveRows(QModelIndex(), row, row);
}

void LibraryFriendModel::itemRemoved() {
    endRemoveRows();
}

void LibraryFriendModel::itemStateChanged(int row) {
    emit dataChanged(createIndex(row, FRIEND_LIBRARY_TITLE_COLUMN), createIndex(row, FRIEND_LIBRARY_FRIEND_COLUMN));
}

LibrarySearchModel::LibrarySearchModel(QLineEdit* searchForm, QWidget *parent)
    :QSortFilterProxyModel(parent), searchForm(searchForm) {
    /* Not needed for regexp search.
       setFilterCaseSensitivity(Qt::CaseInsensitive); */
    setDynamicSortFilter(true);

    connect(searchForm, SIGNAL(textChanged(QString)), this, SLOT(executeSearch()));
}

bool LibrarySearchModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex titleIndex = sourceModel()->index(sourceRow, FRIEND_LIBRARY_TITLE_COLUMN, sourceParent);
    QModelIndex friendIndex = sourceModel()->index(sourceRow, FRIEND_LIBRARY_FRIEND_COLUMN, sourceParent);
    QString toFilter = sourceModel()->data(titleIndex).toString();
    toFilter = toFilter + " " + sourceModel()->data(friendIndex).toString();
    QString filter = filterRegExp().pattern();
    QStringList filterElements = filter.split(" ");
    foreach (QString element, filterElements) {
        if (!toFilter.contains(element, Qt::CaseInsensitive)) return false;
    }
    return true;
}

void LibrarySearchModel::executeSearch(){
    QString filter = searchForm->text();
    setFilterFixedString(filter);
}
