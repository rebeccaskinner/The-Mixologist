/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2009 RetroShare Team
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

#ifndef RATESSTATUS_H
#define RATESSTATUS_H
#include "gui/MainWindow.h"


#include <QWidget>


class RatesStatus : public QWidget {
	Q_OBJECT
public:
	RatesStatus(QWidget *parent = 0);
        void setRatesStatus(float downKb, float upKb);

private:
	class QLabel *iconLabel, *statusRates;
};

#endif
