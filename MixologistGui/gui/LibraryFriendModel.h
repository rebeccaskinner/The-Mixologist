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

#ifndef _LIBRARYFRIENDMODEL_H
#define _LIBRARYFRIENDMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <QPoint>
#include <QSortFilterProxyModel>
#include <QLineEdit>

#define FRIEND_LIBRARY_TITLE_COLUMN 0
#define FRIEND_LIBRARY_FRIEND_COLUMN 1

class QTreeView;
class FriendLibraryMixerItem;

class LibraryFriendModel : public QAbstractListModel{
    Q_OBJECT

public:
    LibraryFriendModel(QTreeView* view, QWidget* parent = 0);

    /* Basic QAbstractItemModel functions. */
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

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

    /* Request the clicked item. */
    void requestItem();

    /* Chat with the clicked friend. */
    void chatFriend();

    //Opens the item in a browser
    void openOnline();

private:
    QMap<unsigned int, FriendLibraryMixerItem*>* itemList;

    /* Info for keeping track of context menu actions. */
    unsigned int contextItemId;

    QTreeView* ownItemView;
};

class LibrarySearchModel : public QSortFilterProxyModel{
    Q_OBJECT
public:
    LibrarySearchModel(QLineEdit* searchForm, QWidget* parent = 0);

public slots:
    /* Executes the search and expands the rows according to the search terms entered. */
    void executeSearch();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

private:
    /* The field in which the user types their search queries. */
    QLineEdit* searchForm;

    /* Search performance isn't wonderful.
       This timer is used to add a delay onto the search while the user is entering it so that it does not slow down typing the query
       due to the search executing while the user is typing. */
    QTimer* searchDelayTimer;

    friend class OffLMFriendModel;
};

#endif
