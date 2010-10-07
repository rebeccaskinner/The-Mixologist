/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2006-2007, crypton
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


#ifndef QTTOASTER_H
#define QTTOASTER_H

#include <QtCore/QObject>

class QWidget;
class QTimer;
class QFrame;

/**
 * This class codes the algorithm that show/hide the toaster.
 */
class QtToaster : public QObject {
	Q_OBJECT
public:

        QtToaster(QWidget * toaster);

        //Sets the time the toaster is on top in milliseconds
	void setTimeOnTop(unsigned time);
	void show();
	void close();

private slots:
	void changeToasterPosition();

private:
	QWidget * _toaster;
	QTimer * _timer;
	bool _show;
	unsigned _timeOnTop;
};

#endif	//QTTOASTER_H
