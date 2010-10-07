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

#include <gui/LibraryHelper.h>
#include <gui/LibraryDialog.h>
#include <gui/Util/Helpers.h> //for the recursiveFileAdd on drop function
#include <interface/librarymixer-library.h>
#include <interface/peers.h>
#include <gui/MainWindow.h>
#include <QMimeData>
#include <QUrl>
#include <QDir>

void LibraryBox::setDisplay(std::list<LibraryMixerItem> items) {
    std::list<LibraryMixerItem>::iterator it;
    QList<QTreeWidgetItem *> listitems;

    clear();
    for (it = items.begin(); it != items.end(); it++) {
        QTreeWidgetItem *listitem = new QTreeWidgetItem(this);

        listitem -> setText(AUTHOR_COLUMN, it->author);
        listitem -> setText(TITLE_COLUMN, it->title);
        switch(it->itemState) {
            case ITEM_MATCHED_TO_CHAT:
                setResponseChat(listitem);
                break;
            case ITEM_MATCHED_TO_FILE:
                setResponseFiles(listitem, it->paths);
                break;
            case ITEM_MATCHED_TO_LEND:
                setResponseFiles(listitem, it->paths, true);
                break;
            case ITEM_MATCHED_TO_LENT:
                setResponseLent(listitem, it->lentTo);
                break;
            case ITEM_MATCH_NOT_FOUND:
                setResponseMissingFile(listitem);
                break;
            case ITEM_MATCHED_TO_MESSAGE:
                setResponseMessage(listitem, it->message);
                break;
            case ITEM_UNMATCHED:
            default:
                break;
        }
        listitem -> setText(ID_COLUMN, QString::number(it->id));
        listitem -> setText(BORROWER_COLUMN, QString::number(it->lentTo));

        listitems.append(listitem);
    }
    insertTopLevelItems(0, listitems);
    update();
    resizeColumnToContents(AUTHOR_COLUMN);
    resizeColumnToContents(TITLE_COLUMN);
    items.clear();
}

bool LibraryBox::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action) {
    (void) index;
    (void) action;
    if (parent == NULL) return false;
    QStringList paths;
    foreach(QUrl url, data->urls()) {
        if (url.scheme() != "file") return false;
        paths << recursiveFileAdd(url.toLocalFile());
    }
    setResponseFiles(parent, paths);
    LibraryMixerLibraryManager::setMatchFile(parent->text(ID_COLUMN).toInt(), paths, ITEM_MATCHED_TO_FILE);
    return true;
}

QStringList LibraryBox::mimeTypes () const {
    QStringList acceptTypes;
    acceptTypes.append("text/uri-list");
    return acceptTypes;
}

Qt::DropActions LibraryBox::supportedDropActions () const {
    return Qt::CopyAction | Qt::MoveAction;
}

void LibraryBox::setResponseChat(QTreeWidgetItem *item) {
    QFontInfo fontInfo = QWidget::fontInfo();
    item->setFont(STATUS_COLUMN, QFont(fontInfo.family(),
                                       fontInfo.pointSize(),
                                       fontInfo.weight(),
                                       true));
    item->setText(STATUS_COLUMN, "Open a chat window when requested");
}

void LibraryBox::setResponseFiles(QTreeWidgetItem *item, QStringList paths, bool lend) {
    QFontInfo fontInfo = QWidget::fontInfo();
    item->setFont(STATUS_COLUMN, QFont(fontInfo.family(),
                                       fontInfo.pointSize(),
                                       fontInfo.weight(),
                                       false));
    QString pathText("");
    for(int i = 0; i < paths.count(); i++) {
        if (lend) pathText.append(LEND_PREFIX);
        pathText.append(paths[i]);
        if (i != (paths.count() - 1)) pathText.append("\n");
    }
    item->setText(STATUS_COLUMN, pathText);
}

void LibraryBox::setResponseLent(QTreeWidgetItem *item, int friend_id) {
    QFontInfo fontInfo = QWidget::fontInfo();

    item->setFont(STATUS_COLUMN, QFont(fontInfo.family(),
                                       fontInfo.pointSize(),
                                       fontInfo.weight(),
                                       false));
    item->setText(STATUS_COLUMN, "Lent to " + peers->getPeerName(friend_id));
}

void LibraryBox::setResponseMissingFile(QTreeWidgetItem *item) {
    QFontInfo fontInfo = QWidget::fontInfo();

    item->setFont(STATUS_COLUMN, QFont(fontInfo.family(),
                                       fontInfo.pointSize(),
                                       fontInfo.weight(),
                                       true));
    item->setText(STATUS_COLUMN, QString("One or more matched files seems to have gone missing"));
}

void LibraryBox::setResponseMessage(QTreeWidgetItem *item, QString message) {
    QFontInfo fontInfo = QWidget::fontInfo();

    item->setFont(STATUS_COLUMN, QFont(fontInfo.family(),
                                       fontInfo.pointSize(),
                                       fontInfo.weight(),
                                       false));
    item->setText(STATUS_COLUMN, message);
}
