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

#include "ratesstatus.h"

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

RatesStatus::RatesStatus(QWidget *parent)
    : QWidget(parent) {
    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setMargin(0);
    hbox->setSpacing(6);

    iconLabel = new QLabel( this );
    iconLabel->setPixmap(QPixmap::QPixmap(":/Images/Up0Down0.png"));
    // iconLabel doesn't change over time, so we didn't need a minimum size
    hbox->addWidget(iconLabel);

    statusRates = new QLabel( tr("<strong>Down:</strong> 0.00 (KB/s) | <strong>Up:</strong> 0.00 (KB/s) "), this );
    //statusPeers->setMinimumSize( statusPeers->frameSize().width() + 0, 0 );
    hbox->addWidget(statusRates);

    setLayout( hbox );

}

void RatesStatus::setRatesStatus(float downKb, float upKb) {
    std::ostringstream out;
    out << "<strong>" << tr("Down:").toStdString() << "</strong> " << std::setprecision(2) << std::fixed << downKb << " (KB/s) |  <strong>" << tr("Up:").toStdString() << "</strong> " << std::setprecision(2) << std::fixed <<  upKb << " (KB/s) ";

    statusRates->setText(QString::fromStdString(out.str()));

    if ( upKb > 0 && downKb <= 0  ) iconLabel->setPixmap(QPixmap::QPixmap(":/Images/Up1Down0.png"));
    else if ( upKb <= 0 && downKb > 0 ) iconLabel->setPixmap(QPixmap::QPixmap(":/Images/Up0Down1.png"));
    else if ( upKb > 0 && downKb > 0 ) iconLabel->setPixmap(QPixmap::QPixmap(":/Images/Up1Down1.png"));
    else iconLabel->setPixmap(QPixmap::QPixmap(":/Images/Up0Down0.png"));
}


