/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006, crypton
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


#include <QFile>
#include <QFileInfo>

#include "NetworkDialog.h"
#include "interface/iface.h"
#include "interface/peers.h"
#include <sstream>

#include <QTimer>
#include <QTime>
#include <QContextMenuEvent>
#include <QMenu>
#include <QCursor>
#include <QPoint>
#include <QMouseEvent>
#include <QPixmap>
#include <QHeaderView>

/** Constructor */
NetworkDialog::NetworkDialog(QWidget *parent)
    : QWidget(parent) {
    /* Invoke the Qt Designer generated object setup routine */
    ui.setupUi(this);

    ui.infoLog->setReadOnly(true);
    connect(ui.infoLog, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(displayInfoLogMenu(const QPoint &)));

    ui.connecttreeWidget->resizeColumnToContents(0);
    ui.connecttreeWidget->resizeColumnToContents(1);
    ui.connecttreeWidget->resizeColumnToContents(2);
    ui.connecttreeWidget->resizeColumnToContents(3);
    ui.connecttreeWidget->resizeColumnToContents(4);
    ui.connecttreeWidget->resizeColumnToContents(5);

    // Set Log infos
    setLogInfo(tr("Mixologist ready to mix it up!"));

    connect(ui.refreshButton, SIGNAL(clicked()), this, SLOT(insertConnect()));
}

/* get the list of Neighbours from the Interface.  */
void NetworkDialog::insertConnect() {
    if (!peers) {
        return;
    }

    std::list<int> friends;
    std::list<int>::iterator it;
    peers->getFriendList(friends);

    /* get a link to the table */
    QTreeWidget *connectWidget = ui.connecttreeWidget;
    QTreeWidgetItem *oldSelect = getCurrentNeighbour();
    QTreeWidgetItem *newSelect = NULL;
    unsigned int oldId = 0;
    if (oldSelect) {
        oldId = (oldSelect->text(5)).toInt();
    }

    QList<QTreeWidgetItem *> items;
    for (it = friends.begin(); it != friends.end(); it++) {
        PeerDetails detail;
        if (!peers->getPeerDetails(*it, detail)) {
            continue; /* BAD */
        }

        /* make a widget per friend */
        QTreeWidgetItem *item = new QTreeWidgetItem((QTreeWidget *)0);

        /* add all the labels */

        /* (0) Name */
        item->setText(0, detail.name);

        /* (1) Status  */
        if (detail.state == PEER_STATE_CONNECTED)
            item->setText(1, "Connected");
        else if (detail.state == PEER_STATE_TRYING)
            item->setText(1, "Trying");
        else if (detail.state == PEER_STATE_WAITING_FOR_RETRY)
            item->setText(1, "Waiting to retry");
        else
            item->setText(1, "Offline");

        /* (2) Last Connect */
        {
            // Show anouncement if a friend never was connected.
            if (detail.state == PEER_STATE_CONNECTED)
                item->setText(2, "Now");
            else if (detail.lastConnect==0 )
                item->setText(2, "");
            else {
                // Dont Show a timestamp in RS calculate the day
                QDateTime datum = QDateTime::fromTime_t(detail.lastConnect);
                // out << datum.toString(Qt::LocalDate);
                QString stime = datum.toString(Qt::LocalDate);
                item->setText(2, stime);
            }
        }

        /* (3) Peer Address */
        {
            std::ostringstream out;
            out << detail.extAddr << ":";
            out << detail.extPort;
            item->setText(3, QString::fromStdString(out.str()));
        }
        {
            std::ostringstream out;
            out << detail.localAddr << ":";
            out << detail.localPort;
            item->setText(4, QString::fromStdString(out.str()));
        }

        /* (4) LibraryMixer ID */
        item->setText(5, QString::number(detail.librarymixer_id));

        if ((oldSelect) && (oldId == detail.librarymixer_id)) {
            newSelect = item;
        }

        /* add to the list */
        items.append(item);
    }

    // add self to network.
    PeerDetails detail ;
    if (peers->getPeerDetails(peers->getOwnLibraryMixerId(),detail)) {
        QTreeWidgetItem *self_item = new QTreeWidgetItem((QTreeWidget *)0);

        self_item->setText(0, peers->getOwnName());
        self_item->setText(1, "This is you!");
        //skip last connect
        {
            std::ostringstream out;
            out << detail.extAddr << ":" << detail.extPort;
            self_item->setText(3, QString::fromStdString(out.str()));
        }
        {
            std::ostringstream out;
            out << detail.localAddr << ":" << detail.localPort;
            self_item->setText(4, QString::fromStdString(out.str()));
        }
        self_item->setText(5, QString::number(detail.librarymixer_id));

        items.append(self_item);
    }

    /* remove old items */
    connectWidget->clear();

    /* add the items in! */
    connectWidget->insertTopLevelItems(0, items);
    if (newSelect) {
        connectWidget->setCurrentItem(newSelect);
    }

    connectWidget->update(); /* update display */

    ui.connecttreeWidget->resizeColumnToContents(0);
    ui.connecttreeWidget->resizeColumnToContents(1);
    ui.connecttreeWidget->resizeColumnToContents(2);
    ui.connecttreeWidget->resizeColumnToContents(3);
    ui.connecttreeWidget->resizeColumnToContents(4);
    ui.connecttreeWidget->resizeColumnToContents(5);
}

QTreeWidgetItem *NetworkDialog::getCurrentNeighbour() {
    /* get the current, and extract the Id */

    /* get a link to the table */
    QTreeWidget *connectWidget = ui.connecttreeWidget;
    QTreeWidgetItem *item = connectWidget->currentItem();
    if (!item) return NULL;
    return item;
}

void NetworkDialog::setLogInfo(QString info) {
    QColor color=QApplication::palette().color(QPalette::WindowText);
    static unsigned int nbLines = 0;
    ++nbLines;
    // Check log size, clear it if too big
    if (nbLines > 2000) {
        ui.infoLog->clear();
        nbLines = 1;
    }
    ui.infoLog->append(QString::fromUtf8("<font color='grey'>") +
                       QTime::currentTime().toString(QString::fromUtf8("hh:mm:ss")) +
                       QString::fromUtf8("</font> - <font color='") + color.name() +
                       QString::fromUtf8("'><i>") + info + QString::fromUtf8("</i></font>"));
}

/* Utility Fns */
void NetworkDialog::displayInfoLogMenu(const QPoint &point) {
    QMouseEvent *mevent = new QMouseEvent( QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton, Qt::NoModifier );
    QMenu contextMenu(this);
    contextMenu.addAction(ui.actionSelect_All);
    contextMenu.addAction(ui.actionCopy);
    contextMenu.addSeparator();
    contextMenu.addAction(ui.actionClear_Log);
    contextMenu.exec(mevent->globalPos());
}

void NetworkDialog::on_actionSelect_All_triggered() {
    ui.infoLog->selectAll();
}

void NetworkDialog::on_actionCopy_triggered() {
    ui.infoLog->copy();
}

void NetworkDialog::on_actionClear_Log_triggered() {
    ui.infoLog->clear();
}
