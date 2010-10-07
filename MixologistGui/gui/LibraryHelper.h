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

#ifndef _LIBRARYHELPER_H
#define _LIBRARYHELPER_H

#include <QObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <interface/librarymixer-library.h>

#define TITLE_COLUMN 0
#define AUTHOR_COLUMN 1
#define STATUS_COLUMN 2
#define ID_COLUMN 3
#define BORROWER_COLUMN 4

const QString LEND_PREFIX("Lend: ");

class LibraryMixerItem;

class LibraryBox : public QTreeWidget{
    Q_OBJECT

    public:
        LibraryBox(QWidget *parent = 0) :QTreeWidget(parent) {}

        //Sets the display of items
        //For items that have an empty path, if they are also marked ITEM_MATCHED_TO_CHAT in itemstate,
        //calls setResponseText with "" to return the appropriate message
        void setDisplay(std::list<LibraryMixerItem> items);

        //Handles dropping of files onto rows to match paths
        virtual bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
        QStringList mimeTypes() const;
        Qt::DropActions supportedDropActions () const;

        /*Sets the response column to chat.*/
        void setResponseChat(QTreeWidgetItem* item);
        /*Sets the response column to files.*/
        void setResponseFiles(QTreeWidgetItem* item, QStringList paths, bool lend = false);
        /*Sets the response column to who borrowed the item.*/
        void setResponseLent(QTreeWidgetItem* item, int friend_id);
        /*Sets the response column to missing file.*/
        void setResponseMissingFile(QTreeWidgetItem* item);
        /*Sets the response column to the message.*/
        void setResponseMessage(QTreeWidgetItem* item, QString message);
};
#endif


