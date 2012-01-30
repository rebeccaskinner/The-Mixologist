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

#include <gui/OffLMFriendModel.h>
#include <gui/MainWindow.h>
#include <gui/PeersDialog.h>
#include <gui/TransfersDialog.h>
#include <interface/peers.h>
#include <QMenu>
#include <QTreeView>
#include <QDateTime>
#include <QMessageBox>
#include <QHeaderView>

#define IMAGE_CHAT                 ":/Images/Chat.png"
#define IMAGE_DOWNLOAD             ":/Images/Download.png"

OffLMFriendModel::OffLMFriendModel(QTreeView* view, QWidget *parent) :QAbstractItemModel(parent), friendItemView(view) {
    connect(files, SIGNAL(offLMFriendAboutToBeAdded(int)), this, SLOT(friendAboutToBeAdded(int)), Qt::DirectConnection);
    connect(files, SIGNAL(offLMFriendAboutToBeRemoved(int)), this, SLOT(friendAboutToBeRemoved(int)), Qt::DirectConnection);
    connect(files, SIGNAL(offLMFriendAdded()), this, SLOT(friendAdded()));
    connect(files, SIGNAL(offLMFriendRemoved()), this, SLOT(friendRemoved()));
    connect(friendItemView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(friendListContextMenu(QPoint)));
    /* This can't be done in the constructor because ownItemView isn't setup yet because LibraryDialog hasn't finished constructing yet.
       However, geometriesChanged seems to work, thouhg it ends up being called not only after construction, but also when moving between pages and destruction. */
    connect(friendItemView->header(), SIGNAL(geometriesChanged()), this, SLOT(resizeHeader()));
    connect(friendItemView->header(), SIGNAL(geometriesChanged()), this, SLOT(setDefaultExpansion()));
}

void OffLMFriendModel::setContainingSearchModel(OffLMSearchModel *model) {
    containingSearchModel = model;
}

int OffLMFriendModel::columnCount(const QModelIndex &parent) const {
    return 4;
}

QVariant OffLMFriendModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();

    if (role != Qt::DisplayRole) return QVariant();

    OffLMShareItem* shareItem = static_cast<OffLMShareItem*>(index.internalPointer());

    switch (index.column()){
    case LABEL_COLUMN:
        if (shareItem->isRootItem()){
            return peers->getPeerName(shareItem->friendId());
        } else if (shareItem->isShare()){
            return shareItem->label();
        } else {
            return shareItem->path();
        }
    case STATUS_COLUMN:
        if (shareItem->isShare()){
            switch (shareItem->shareMethod()){
            case OffLMShareItem::STATE_TO_SEND:
                return "Send";
            case OffLMShareItem::STATE_TO_LEND:
                if (shareItem->lent() == 0)
                    return "Lend";
                else
                    return "Lent out";
            /* No need to handle missing states, they are stripped before being sent to us by the sending friend.
            case OffLMShareItem::STATE_TO_SEND_MISSING:
            case OffLMShareItem::STATE_TO_LEND_MISSING:
                return "Files missing"; */
            default:
                return "";
            }
        } else return QVariant();
    case SIZE_COLUMN:
        if (shareItem->isFile()){
            double size = shareItem->size();
            if (size < 1024) {
                return "<1 KB";
            } else if (size < 1048576) {
                size /= 1024;
                return QString::number(size, 'f', 0) + " KB";
            } else if (size < 1073741824) {
                size /= 1048576;
                return QString::number(size, 'f', 1) + " MB";
            } else {
                size /= 1073741824;
                return QString::number(size, 'f', 1) + " GB";
            }
        } else return QVariant();
    case UPDATED_COLUMN:
        /* If we have only a share we use the share updated timestamp, if we have only a file we use the file modified timestamp.
           If we have a share that is a file, we use the newer timestamp. */
        if (shareItem->isShare() && shareItem->isFile()){
            if (QDateTime::fromTime_t(shareItem->updated()) > QDateTime::fromTime_t(shareItem->modified()))
                return QDateTime::fromTime_t(shareItem->updated()).toString(Qt::SystemLocaleShortDate);
            else
                return QDateTime::fromTime_t(shareItem->modified()).toString(Qt::SystemLocaleShortDate);
        } else if (shareItem->isShare()) {
            return QDateTime::fromTime_t(shareItem->updated()).toString(Qt::SystemLocaleShortDate);
        } else if (shareItem->isFile()) {
            return QDateTime::fromTime_t(shareItem->modified()).toString(Qt::SystemLocaleShortDate);
        }
        return QVariant();
    default:
        return QVariant();
    }
}

QVariant OffLMFriendModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole){
        switch (section){
        case LABEL_COLUMN:
            return "Name";
        case STATUS_COLUMN:
            return "Response to request";
        case SIZE_COLUMN:
            return "Size";
        case UPDATED_COLUMN:
            return "Updated";
        }
    }

    return QVariant();
}

QModelIndex OffLMFriendModel::index(int row, int column, const QModelIndex &parent) const {
    if (row < 0 || column < 0) return QModelIndex();

    OffLMShareItem* item;
    //An invalid parent indicates the view is querying a friend's root
    if (!parent.isValid()) {
        item = files->getFriendOffLMShares(row);
    //Otherwise indicates the view is querying a share/file/folder
    } else {
        item = static_cast<OffLMShareItem*>(parent.internalPointer())->child(row);
    }

    if (item) return createIndex(row, column, item);
    else return QModelIndex();
}

QModelIndex OffLMFriendModel::parent(const QModelIndex &index) const {
    if (!index.isValid()) return QModelIndex();

    OffLMShareItem *childItem = static_cast<OffLMShareItem*>(index.internalPointer());
    OffLMShareItem *parentItem = childItem->parent();

    if (!parentItem) return QModelIndex();

     return createIndex(parentItem->order(), 0, parentItem);
}

int OffLMFriendModel::rowCount(const QModelIndex &parent) const {
    if (parent.column() > 0) return 0;

    //An invalid parent indicates the view is querying a root row
    if (!parent.isValid()) {
        return files->getOffLMShareFriendCount();
    //Otherwise indicates the view is querying an item held in a folder
    } else {
        return static_cast<OffLMShareItem*>(parent.internalPointer())->childCount();
    }
}

void OffLMFriendModel::friendListContextMenu(QPoint point){
    /* We cannot directly use the pointer to the OffLMShareItem, because the QSortFilterProxyModel (OffLMSearchModel)
       uses the internal pointers itself, so we must first convert the index into OffLMFriendModel's own index. */
    QModelIndex searchIndex = friendItemView->indexAt(point);
    if (!searchIndex.isValid()) return;
    QModelIndex index = ((OffLMSearchModel*)searchIndex.model())->mapToSource(searchIndex);
    if (!index.isValid()) return;

    contextItem = static_cast<OffLMShareItem*>(index.internalPointer());

    QMenu contextMenu(friendItemView);
    bool friendOnline = peers->isOnline(contextItem->friendId());
    if (contextItem->isRootItem()) {
        QAction *chatAct = new QAction(QIcon(IMAGE_CHAT), tr("Chat"), &contextMenu);
        connect(chatAct, SIGNAL(triggered()), this, SLOT(chatFriend()));
        contextMenu.addAction(chatAct);
        if (!friendOnline){
            chatAct->setEnabled(false);
            chatAct->setText(tr("Chat (friend offline)"));
        }
    /* If the item is shared for sending, then any file, folder, or share will work.
       However, if the item is for lending, then only root shares can be downloaded. */
    } else if ((contextItem->shareMethod() == OffLMShareItem::STATE_TO_SEND &&
        (contextItem->isShare() || contextItem->isFile() || contextItem->isFolder())) ||
        (contextItem->shareMethod() == OffLMShareItem::STATE_TO_LEND &&
        (contextItem->isShare()))){
        QAction *requestAct = new QAction(QIcon(IMAGE_DOWNLOAD), tr("Download"), &contextMenu);
        connect(requestAct, SIGNAL(triggered()), this, SLOT(requestItem()));
        contextMenu.addAction(requestAct);
        if (!friendOnline) {
            requestAct->setText(tr("Download when friend online"));
        }
    }
    contextMenu.exec(friendItemView->mapToGlobal(point));
}


void OffLMFriendModel::chatFriend(){
    if (contextItem->friendId() != 0)
        mainwindow->peersDialog->getOrCreateChat(contextItem->friendId(), true);
}

void OffLMFriendModel::requestItem(){
    QString title;
    if (contextItem->isShare()) title = contextItem->label();
    else title = contextItem->path();

    unsigned int friend_id = contextItem->friendId();

    QStringList paths;
    QStringList hashes;
    QList<qlonglong> filesizes;
    contextItem->getRecursiveFileInfo(paths, hashes, filesizes);

    if (contextItem->shareMethod() == OffLMShareItem::STATE_TO_SEND) {
        files->downloadFiles(friend_id, title, paths, hashes, filesizes);
    } else if (contextItem->shareMethod() == OffLMShareItem::STATE_TO_LEND) {
        QString friend_name = peers->getPeerName(friend_id);
        QString message = friend_name +
                          " has marked " +
                          title +
                          " as available to be lent to friends, but wants it back at some point. Continue?";
        if (QMessageBox::question(friendItemView, title, message, QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            files->borrowFiles(friend_id, title, paths, hashes, filesizes, FILE_HINTS_OFF_LM, contextItem->label());
        } else return;
    }

    /* Change display to transfers dialog to provide visual feedback to user of action. */
    mainwindow->switchToDialog(mainwindow->transfersDialog);
}

void OffLMFriendModel::friendAboutToBeAdded(int row) {
    beginInsertRows(QModelIndex(), row, row);
    lastAddedRow = row;
}

void OffLMFriendModel::friendAdded() {
    endInsertRows();

    friendItemView->setExpanded(containingSearchModel->mapFromSource(index(lastAddedRow, 0, QModelIndex())), true);

    resizeHeader();
}

void OffLMFriendModel::friendAboutToBeRemoved(int row) {
    beginRemoveRows(QModelIndex(), row, row);
}

void OffLMFriendModel::friendRemoved() {
    endRemoveRows();
}

void OffLMFriendModel::resizeHeader() {
    /* Set the status column to be big enough to hold its contents, but reduceable in size,
       and the label column to take whatever is left. */
    friendItemView->header()->setResizeMode(STATUS_COLUMN, QHeaderView::Interactive);
    friendItemView->header()->setResizeMode(SIZE_COLUMN, QHeaderView::Interactive);
    friendItemView->header()->setResizeMode(UPDATED_COLUMN, QHeaderView::Interactive);
    friendItemView->resizeColumnToContents(STATUS_COLUMN);
    /* We don't use resizeColumnToContents here because on the initial post-construction call, there are no contents yet,
       and our header titles are really narrow, resulting in a column width too narrow for most contents.
       These values are arbitrary, and just eye-balled to generally be large enough for conventional fields. */
    friendItemView->header()->resizeSection(SIZE_COLUMN, 60);
    friendItemView->header()->resizeSection(UPDATED_COLUMN, 135);
    friendItemView->header()->setResizeMode(LABEL_COLUMN, QHeaderView::Stretch);
}

void OffLMFriendModel::setDefaultExpansion() {
  for (int i = 0; i <= rowCount(QModelIndex()); i++)
      friendItemView->setExpanded(containingSearchModel->mapFromSource(index(i, 0, QModelIndex())), true);
}

OffLMSearchModel::OffLMSearchModel(QLineEdit* searchForm, QWidget *parent)
    :QSortFilterProxyModel(parent), searchForm(searchForm) {
    /* Not needed for regexp search.
       setFilterCaseSensitivity(Qt::CaseInsensitive); */
    setDynamicSortFilter(true);

    searchDelayTimer = new QTimer(this);
    searchDelayTimer->setSingleShot(true);

    connect(searchForm, SIGNAL(textChanged(QString)), this, SLOT(searchTermsChanged()));
    connect(searchDelayTimer, SIGNAL(timeout()), this, SLOT(executeSearch()));
}

bool OffLMSearchModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    bool accept = false;
    QModelIndex index = sourceModel()->index(sourceRow, LABEL_COLUMN, sourceParent);
    QString currentData = sourceModel()->data(index).toString();
    if (stringAccepted(currentData)) accept = true;
    else {
        currentData = currentData + " " + ancestorText(index);
        if (stringAccepted(currentData)) accept = true;
        else {
            if (anyDescendantsAccepted(index, currentData)) accept = true;
        }
    }
    /* This would be a good approach to expanding the relevant items on search, calling on each
       item exactly once. However, it doesn't work and also acts buggy, causing the expansion
       triangle graphics in QT to disappear.
       Instead, we are implementing this using the expand slot on the OffLMSearchModel. */
    //static_cast<OffLMFriendModel*>(sourceModel())->friendItemView->setExpanded(index, accept);
    return accept;
}

QString OffLMSearchModel::ancestorText(const QModelIndex &index) const {
    if (index.parent().isValid()){
        return sourceModel()->data(index.parent()).toString() + " " + ancestorText(index.parent());
    }
    return "";
}

bool OffLMSearchModel::anyDescendantsAccepted(const QModelIndex &index, QString ancestorText) const {
    if (sourceModel()->hasChildren(index)){
        int children = sourceModel()->rowCount(index);
        for (int i = 0; i < children; i++){
            QModelIndex childIndex = sourceModel()->index(i, LABEL_COLUMN, index);
            QString currentData = sourceModel()->data(childIndex).toString() + " " + ancestorText;
            if (stringAccepted(currentData)) {
                return true;
            }
            if (anyDescendantsAccepted(childIndex, currentData)) return true;
        }
    }
    return false;
}

bool OffLMSearchModel::stringAccepted(const QString &subject) const {
    QString filter = filterRegExp().pattern();
    QStringList filterElements = filter.split(" ");
    foreach (QString element, filterElements) {
        if (!subject.contains(element, Qt::CaseInsensitive)) return false;
    }
    return true;
}

#define SEARCH_DELAY 250 //quarter second

void OffLMSearchModel::searchTermsChanged() {
    searchDelayTimer->start(SEARCH_DELAY);
}

void OffLMSearchModel::executeSearch(){
    QString filter = searchForm->text();

    setFilterFixedString(filter);

    if (filter.isEmpty()) static_cast<OffLMFriendModel*>(sourceModel())->setDefaultExpansion();
    else static_cast<OffLMFriendModel*>(sourceModel())->friendItemView->expandAll();
}
