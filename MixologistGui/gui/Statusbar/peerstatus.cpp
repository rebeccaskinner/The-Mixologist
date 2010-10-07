/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2009 RetroShare Team
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

#include "peerstatus.h"

#include <QtGui>
#include <QString>

#include <QLayout>
#include <QLabel>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

#include "interface/iface.h"
#include "interface/peers.h"

#include <sstream>
#include <iomanip>

PeerStatus::PeerStatus(QWidget *parent)
    : QWidget(parent) {
    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setMargin(0);
    hbox->setSpacing(6);

    iconLabel = new QLabel( this );
    iconLabel->setPixmap(QPixmap::QPixmap(":/Images/StatusOffline.png"));
    // iconLabel doesn't change over time, so we didn't need a minimum size
    hbox->addWidget(iconLabel);

    statusPeers = new QLabel( tr("Online: 0  | Friends: 0 "), this );
    hbox->addWidget(statusPeers);

    setLayout( hbox );

}

void PeerStatus::setPeerStatus(int online, int total) {
    std::ostringstream out2;
    out2 << "<span style=\"color:#008080\"><strong>" << tr("Online:").toStdString() << " </strong></span>" << online << " | <span style=\"color:#000080\"><strong>" << tr("Friends:").toStdString() << " </strong></span>" << total << " ";

    statusPeers -> setText(QString::fromStdString(out2.str()));

    if (online > 0) iconLabel->setPixmap(QPixmap::QPixmap(":/Images/StatusConnected.png"));
    else iconLabel->setPixmap(QPixmap::QPixmap(":/Images/StatusOffline.png"));
}


