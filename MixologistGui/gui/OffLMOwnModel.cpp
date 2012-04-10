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

#include <gui/OffLMOwnModel.h>
#include <interface/peers.h>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QMenu>
#include <QDesktopServices>
#include <QTreeView>
#include <QStack>
#include <QHeaderView>
#include <QFileDialog>

#define IMAGE_OPEN                 ":/Images/Play.png"
#define IMAGE_CANCEL               ":/Images/Cancel.png"
#define IMAGE_ADD_FILE             ":/Images/File.png"
#define IMAGE_ADD_FOLDER           ":/Images/Folder.png"

OffLMOwnModel::OffLMOwnModel(QTreeView* view, QWidget *parent) :QAbstractItemModel(parent), ownItemView(view) {
    connect(ownItemView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(ownListContextMenu(QPoint)));
    /* This can't be done in the constructor because ownItemView isn't setup yet because LibraryDialog hasn't finished constructing yet.
       However, geometriesChanged seems to work, though it ends up being called not only after construction, but also when moving between pages and destruction. */
    connect(ownItemView->header(), SIGNAL(geometriesChanged()), this, SLOT(resizeHeader()));
    connect(files, SIGNAL(offLMOwnItemAboutToBeAdded(OffLMShareItem*)), this, SLOT(itemAboutToBeInserted(OffLMShareItem*)), Qt::DirectConnection);
    connect(files, SIGNAL(offLMOwnItemAdded()), this, SLOT(itemInserted()), Qt::QueuedConnection);
    connect(files, SIGNAL(offLMOwnItemChanged(OffLMShareItem*)), this, SLOT(itemChanged(OffLMShareItem*)), Qt::QueuedConnection);
    connect(files, SIGNAL(offLMOwnItemAboutToBeRemoved(OffLMShareItem*)), this, SLOT(itemAboutToBeRemoved(OffLMShareItem*)), Qt::DirectConnection);
    connect(files, SIGNAL(offLMOwnItemRemoved()), this, SLOT(itemRemoved()), Qt::QueuedConnection);
}

int OffLMOwnModel::columnCount(const QModelIndex &parent) const {
    return 2;
}

QVariant OffLMOwnModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();

    if (role != Qt::DisplayRole) return QVariant();

    OffLMShareItem* shareItem = static_cast<OffLMShareItem*>(index.internalPointer());

    if (OWN_LABEL_COLUMN == index.column()) {
        if (shareItem->isShare()){
            return shareItem->label() + " (" + shareItem->fullPath() + ")";
        } else {
            return shareItem->path();
        }
    } else if (OWN_STATUS_COLUMN == index.column()) {
        QString displayMessage = "";
        if (shareItem->isFile() && shareItem->hash().isEmpty()) {
           displayMessage = "(Prepping file) ";
        }
        if (shareItem->isShare()){
            switch (shareItem->shareMethod()){
            case OffLMShareItem::STATE_TO_SEND:
                displayMessage += "Send";
                break;
            case OffLMShareItem::STATE_TO_LEND:
                if (shareItem->lent() == 0)
                    displayMessage += "Lend";
                else
                    displayMessage += "Lent to " + peers->getPeerName(shareItem->lent());
                break;
            case OffLMShareItem::STATE_TO_SEND_MISSING:
                displayMessage += "Send (missing)";
                break;
            case OffLMShareItem::STATE_TO_LEND_MISSING:
                displayMessage += "Lend (missing)";
                break;
            }
        }
        return displayMessage;
    }
    return QVariant();
}

bool OffLMOwnModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (role == Qt::EditRole &&
        index.column() == OWN_LABEL_COLUMN &&
        !value.toString().isEmpty()) {
        OffLMShareItem* shareItem = static_cast<OffLMShareItem*>(index.internalPointer());

        if (shareItem->isShare()) {
            files->setOffLMShareLabel(shareItem, value.toString());
            return true;
        }
    }
    return false;
}

Qt::ItemFlags OffLMOwnModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;

    if (index.column() == OWN_LABEL_COLUMN) {
        OffLMShareItem* shareItem = static_cast<OffLMShareItem*>(index.internalPointer());
        if (shareItem->isShare()) flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QVariant OffLMOwnModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole){
        switch (section){
        case OWN_LABEL_COLUMN:
            return "Name";
        case OWN_STATUS_COLUMN:
            return "Response to request";
        }
    }

    return QVariant();
}

QModelIndex OffLMOwnModel::index(int row, int column, const QModelIndex &parent) const {
    if (row < 0 || column < 0) return QModelIndex();

    OffLMShareItem* parentItem;

    //An invalid parent indicates the view is querying a root row
    if (!parent.isValid()) {
        parentItem = files->getOwnOffLMRoot();
    //Otherwise indicates the view is querying an item held in a folder
    } else {
        parentItem = static_cast<OffLMShareItem*>(parent.internalPointer());
    }

    OffLMShareItem* item = parentItem->child(row);

    if (item) return createIndex(row, column, item);
    else return QModelIndex();
}

QModelIndex OffLMOwnModel::parent(const QModelIndex &index) const {
    if (!index.isValid()) return QModelIndex();

    OffLMShareItem *childItem = static_cast<OffLMShareItem*>(index.internalPointer());
    OffLMShareItem *parentItem = childItem->parent();

    if (!parentItem || parentItem == files->getOwnOffLMRoot()) return QModelIndex();

     return createIndex(parentItem->order(), 0, parentItem);
}

int OffLMOwnModel::rowCount(const QModelIndex &parent) const {
    if (parent.column() > 0) return 0;

    OffLMShareItem* parentItem;

    //An invalid parent indicates the view is querying a root row
    if (!parent.isValid()) {
        parentItem = files->getOwnOffLMRoot();
    //Otherwise indicates the view is querying an item held in a folder
    } else {
        parentItem = static_cast<OffLMShareItem*>(parent.internalPointer());
    }

    return parentItem->childCount();
}

bool OffLMOwnModel::dropMimeData(const QMimeData *data, Qt::DropAction /*action*/, int /*row*/, int /*column*/, const QModelIndex& /*parent*/) {
    QStringList paths;
    foreach (QUrl url, data->urls()) {
        if (url.scheme() == "file") paths.append(url.toLocalFile());
    }

    if (paths.count() > 1) {
        int choice = QMessageBox::question(NULL, "Multiple items dropped", "Do you want to add all " + QString::number(paths.count()) + " items you dropped?", QMessageBox::Yes, QMessageBox::No);
        if (choice == QMessageBox::No){
            return false;
        }
    }

    foreach (QString path, paths) {
        files->addOffLMShare(path);
    }

    return true;
}

QStringList OffLMOwnModel::mimeTypes () const {
    QStringList acceptTypes;
    acceptTypes.append("text/uri-list");
    return acceptTypes;
}

Qt::DropActions OffLMOwnModel::supportedDropActions () const {
    return Qt::CopyAction | Qt::MoveAction;
}

void OffLMOwnModel::ownListContextMenu(QPoint point){
    QMenu contextMenu(ownItemView);
    QModelIndex index = ownItemView->indexAt(point);
    if (!index.isValid()) {
        QAction *addFilesAct = new QAction(QIcon(IMAGE_ADD_FILE), tr("Share a file"), &contextMenu);
        connect(addFilesAct, SIGNAL(triggered()), this, SLOT(addFileDialog()));
        contextMenu.addAction(addFilesAct);

        QAction *addFoldersAct = new QAction(QIcon(IMAGE_ADD_FOLDER), tr("Share a folder"), &contextMenu);
        connect(addFoldersAct, SIGNAL(triggered()), this, SLOT(addFolderDialog()));
        contextMenu.addAction(addFoldersAct);
    } else {
        contextItem = static_cast<OffLMShareItem*>(ownItemView->indexAt(point).internalPointer());

        QAction *openAct = new QAction(QIcon(IMAGE_OPEN), tr("Open"), &contextMenu);
        connect(openAct, SIGNAL(triggered()), this, SLOT(openItem()));
        contextMenu.addAction(openAct);

        if (contextItem->isShare()) {
            QAction *statusAct = new QAction(tr("Change Send/Lend"), &contextMenu);
            connect(statusAct, SIGNAL(triggered()), this, SLOT(setStatus()));
            contextMenu.addAction(statusAct);

            QAction *removeAct = new QAction(QIcon(IMAGE_CANCEL), tr("Remove"), &contextMenu);
            connect(removeAct, SIGNAL(triggered()), this, SLOT(removeOwn()));
            contextMenu.addAction(removeAct);
        }
    }

    contextMenu.exec(ownItemView->mapToGlobal(point));
}

void OffLMOwnModel::removeOwn(){
    QString message("Are you sure that you want to remove:\n");
    message.append(contextItem->label());
    if ((QMessageBox::question(ownItemView, tr("Remove"), message,
                               QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes)) == QMessageBox::Yes) {
        if (!files->removeOffLMShare(contextItem)){
            QMessageBox::about(ownItemView, "Uh-oh!", "A problem came up while trying to remove this!");
        }
    }
}

void OffLMOwnModel::openItem(){
    QDesktopServices::openUrl(QUrl("file:///" + contextItem->fullPath(), QUrl::TolerantMode));
}

void OffLMOwnModel::setStatus(){
    OffLMShareItem::shareMethodState oldState = contextItem->shareMethod();
    OffLMShareItem::shareMethodState newState = (oldState == OffLMShareItem::STATE_TO_SEND) ?
                                                OffLMShareItem::STATE_TO_LEND :
                                                OffLMShareItem::STATE_TO_SEND;
    if (!files->setOffLMShareMethod(contextItem, newState)){
        QMessageBox::about(ownItemView, "Uh-oh!", "A problem came up while trying to change this!");
    }
}

void OffLMOwnModel::itemAboutToBeInserted(OffLMShareItem* parentItem) {
    QModelIndex index = findIndexByItem(parentItem);
    beginInsertRows(index, parentItem->childCount(), parentItem->childCount());
}

void OffLMOwnModel::itemInserted() {
    endInsertRows();
}

void OffLMOwnModel::itemAboutToBeRemoved(OffLMShareItem* item) {
    QModelIndex index = findIndexByItem(item);
    beginRemoveRows(index.parent(), item->order(), item->order());
}

void OffLMOwnModel::itemRemoved() {
    endRemoveRows();
}

void OffLMOwnModel::itemChanged(OffLMShareItem *item) {
    QModelIndex parentIndex = findIndexByItem(item);

    /* Once we have the parent, we can generate the indexes we need for updating changedItem's view. */
    QModelIndex leftIndex = index(item->order(), OWN_LABEL_COLUMN, parentIndex);
    QModelIndex rightIndex = index(item->order(), OWN_STATUS_COLUMN, parentIndex);
    emit dataChanged(leftIndex, rightIndex);
}

void OffLMOwnModel::resizeHeader() {
    /* Set the status column to be big enough to hold its contents, but reduceable in size,
       and the label column to take whatever is left. */
    ownItemView->header()->setResizeMode(OWN_STATUS_COLUMN, QHeaderView::Interactive);
    ownItemView->resizeColumnToContents(OWN_STATUS_COLUMN);
    ownItemView->header()->setResizeMode(OWN_LABEL_COLUMN, QHeaderView::Stretch);
}

void OffLMOwnModel::addFileDialog() {
    QStringList paths = QFileDialog::getOpenFileNames(ownItemView, "It's usually easier to just drag and drop, but you can also do it this way");
    if (!paths.empty()) {
        foreach (QString path, paths) {
            files->addOffLMShare(path);
        }
    }
}

void OffLMOwnModel::addFolderDialog() {
    QString path = QFileDialog::getExistingDirectory(ownItemView, "It's usually easier to just drag and drop, but you can also do it this way");
    if (!path.isEmpty()) files->addOffLMShare(path);
}

void OffLMOwnModel::addShares(QStringList paths) {
    foreach (QString path, paths) {
        files->addOffLMShare(path);
    }
}

QModelIndex OffLMOwnModel::findIndexByItem(OffLMShareItem *item) {
    /* We begin by constructing a stack of all of the row numbers of changedItem's ancestors, which does not include changedItem itself. */
    QStack<int> orderStack;
    OffLMShareItem *currentItem = item;
    while (!currentItem->isRootItem()) {
        orderStack.push(currentItem->order());
        currentItem = currentItem->parent();
    }

    /* Then we use the stack to start from the bottom to get the index of changedItem's parent. */
    int currentRow;
    QModelIndex parentIndex;
    while (!orderStack.isEmpty()) {
        currentRow = orderStack.pop();
        parentIndex = index(currentRow, 0, parentIndex);
    }

    return parentIndex;
}
