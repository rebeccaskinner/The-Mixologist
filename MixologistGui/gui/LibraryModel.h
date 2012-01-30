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

#ifndef _LIBRARYMODEL_H
#define _LIBRARYMODEL_H

#include "interface/files.h"
#include <QObject>
#include <QAbstractListModel>
#include <QPoint>

#define LIBRARY_TITLE_COLUMN 0
#define LIBRARY_STATUS_COLUMN 1

class QTreeView;
class QLabel;

class LibraryModel : public QAbstractListModel{
    Q_OBJECT

public:
    LibraryModel(QTreeView* view, QWidget* parent = 0);

    /* Basic QAbstractItemModel functions. */
    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    /* Drop files/folders into QAbstractItemModel functions. */
    virtual bool dropMimeData (const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    QStringList mimeTypes() const;
    Qt::DropActions supportedDropActions () const;

public slots:
    /* The following slots are used by the files interface to inform of changes to the model. */
    void itemAboutToBeInserted(int row);
    void itemInserted();
    void itemAboutToBeRemoved(int row);
    void itemRemoved();
    void itemStateChanged(int row);

private slots:
    /* Create the context popup menu and it's submenus. */
    void customContextMenu(QPoint point);

    /* Resizes the columns to their contents. */
    void resizeColumns();

    //Opens a text dialog for the user to set an auto response message
    void setMatchToMessage();
    //Opens a file dialog to set files
    void setMatchToFiles();
    //Opens a file dialog to set files to lend
    void setMatchToLend();
    //Sets an item to chat, and never bother again about auto match
    void setMatchToChat();
    //Toggle an item set to file to lend
    void setFilesToLend();
    //Toggle an item set to lend to file
    void setLendToFiles();
    //Shows the help window
    void showHelp();
    //Chat with the borrower of a lent item
    void chatBorrower();
    //Opens the item in a browser
    void openOnline();

private:
    /* Info for keeping track of context menu actions. */
    int contextItemId;

    QTreeView* ownItemView;
};
#endif
