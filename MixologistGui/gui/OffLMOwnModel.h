/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
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

#ifndef _OFFLMOWNMODEL_H
#define _OFFLMOWNMODEL_H

#include "interface/files.h"
#include <QObject>
#include <QAbstractItemModel>
#include <QPoint>

#define OWN_LABEL_COLUMN 0
#define OWN_STATUS_COLUMN 1

class QTreeView;

class OffLMOwnModel : public QAbstractItemModel{
    Q_OBJECT

public:
    OffLMOwnModel(QTreeView* view, QWidget* parent = 0);

    QVariant data(const QModelIndex &index, int role) const;
    bool setData (const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    /* Drop files/folders into QAbstractItemModel functions. */
    virtual bool dropMimeData (const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    QStringList mimeTypes() const;
    Qt::DropActions supportedDropActions () const;

public slots:
    /* Create the context popup menu and it's submenus. */
    void ownListContextMenu(QPoint point);
    /* Removes own item from share list. */
    void removeOwn();
    /* Opens own item with desktop services. */
    void openItem();
    /* Sets the copy/lend status of one of own items. */
    void setStatus();

    /* The following slots are used by the files interface to inform of changes to the model. */
    /* Note that itemAboutToBeInserted is in terms of the item that will hold the new row, and not in terms of the new row itself. */
    void itemAboutToBeInserted(OffLMShareItem* parentItem);
    void itemInserted();
    void itemAboutToBeRemoved(OffLMShareItem* item);
    void itemRemoved();
    void itemChanged(OffLMShareItem* item);

private slots:
    /* Sets the column widths of the view to defaults. */
    void resizeHeader();

    /* Opens a dialog for users to add a file share. */
    void addFileDialog();

    /* Opens a dialog for users to add a folder share. */
    void addFolderDialog();

private:
    /* Adds the specified paths as shares.
       If only one path is provided, then will display an error on duplicates.
       Otherwise, we purposefully silently ignore duplicates to avoid inundating the user with errors. */
    void addShares(QStringList paths);

    QModelIndex findIndexByItem(OffLMShareItem* item);

    /* Info for keeping track of context menu actions. */
    OffLMShareItem* contextItem;

    QTreeView* ownItemView;
};
#endif


