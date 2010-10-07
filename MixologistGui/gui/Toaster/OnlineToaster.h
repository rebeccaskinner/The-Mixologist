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


#ifndef ONLINETOASTER_H
#define ONLINETOASTER_H

#include "IQtToaster.h"
#include <QtCore/QObject>
#include <interface/peers.h>
#include "ui_OnlineToaster.h"
class QtToaster;

class QWidget;
class QString;
class QPixmap;

/**
 * Shows a toaster when friend is Online .
 *
 *
 */
class OnlineToaster : public QObject, public IQtToaster {
	Q_OBJECT
public:
	OnlineToaster();
	~OnlineToaster();
	void setMessage(const QString & message);
	void setPixmap(const QPixmap & pixmap);
	void show();


private slots:
	void chatButtonSlot();
	void close();

private:
        int librarymixer_id; //This stores the id of the friend associated with the notification.
	Ui::OnlineToaster * _ui;
	QWidget * _onlineToasterWidget;
	QtToaster * _toaster;
};

#endif
