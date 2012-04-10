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

#ifndef _OFFLMFRIENDMODEL_H
#define _OFFLMFRIENDMODEL_H

#include "interface/files.h"
#include <QObject>
#include <QAbstractItemModel>
#include <QPoint>
#include <QSortFilterProxyModel>
#include <QTimer>

#define LABEL_COLUMN 0
#define SIZE_COLUMN 1
#define UPDATED_COLUMN 2
#define STATUS_COLUMN 3

class QTreeView;
class OffLMSearchModel;
class QLineEdit;

class OffLMFriendModel : public QAbstractItemModel{
    Q_OBJECT

public:
    OffLMFriendModel(QTreeView* view, QWidget* parent = 0);

    /* Necessary step to finish initializing the OffLMFriendModel.
       Can't be done in constructor because both OffLMFriendModel and OffLMSearchModel need pointers to each other. */
    void setContainingSearchModel(OffLMSearchModel* model);

    /* Basic QAbstractItemModel functions. */
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

public slots:
    /* Create the context popup menu and it's submenus. */
    void friendListContextMenu(QPoint point);
    /* Initiates a chat with the friend who is represented by contextItem. */
    void chatFriend();
    /* Requests the item represented by contextItem. */
    void requestItem();

    /* The following slots are used by the files interface to inform of changes to the model. */
    void friendAdded(unsigned int friend_id);
    void friendRemoved(unsigned int friend_id);

private slots:
    /* Sets the column widths of the view to defaults. */
    void resizeHeader();

    /* Expands all of the first level items (friends' root shares) */
    void setDefaultExpansion();

private:
    /* The data for the model .*/
    QHash<unsigned int, OffLMShareItem*> friendRoots;

    /* We use this to keep track of the order of display, the display orders match the order in the list. */
    QList<unsigned int> friendDisplayOrder;

    /* Info for keeping track of context menu actions. */
    OffLMShareItem* contextItem;

    QTreeView* friendItemView;

    /* Needed in order  to be able to act upon the view.
       All of the indexes generated in this model are redirected through a parallel set of indexes in the search model.
       Without the ability to call the containingSearchModel to translate, we wouldn't be able to pass the view commands. */
    OffLMSearchModel* containingSearchModel;

    friend class OffLMSearchModel;
};

class OffLMSearchModel : public QSortFilterProxyModel{
    Q_OBJECT
public:
    OffLMSearchModel(QLineEdit* searchForm, QWidget* parent = 0);

public slots:
    /* Executes the search and expands the rows according to the search terms entered. */
    void executeSearch();

    /* Invokes the searchDelayTimer */
    void searchTermsChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

private:
    /* Returns the text from all ancestors concatenated into a single string.
       Needed so that we can filter descendants based on text that is in their ancestors. */
    QString ancestorText(const QModelIndex &index) const;

    /* Returns true if any descendants will be accepted.
       ancestorText is the summed text from all ancestors, so that for example if a username is matched from the friend root,
       and the title is matched from the current file, this will register as true. */
    bool anyDescendantsAccepted(const QModelIndex &index, QString ancestorText) const;

    /* Returns true if the given string will be accepted. */
    bool stringAccepted(const QString &subject) const;

    /* The field in which the user types their search queries. */
    QLineEdit* searchForm;

    /* Search performance isn't wonderful.
       This timer is used to add a delay onto the search while the user is entering it so that it does not slow down typing the query
       due to the search executing while the user is typing. */
    QTimer* searchDelayTimer;

    friend class OffLMFriendModel;
};

#endif
