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

#ifndef _NETWORKDIALOG_H
#define _NETWORKDIALOG_H

#include <QFileDialog>
#include <QtGui>

#include "ui_NetworkDialog.h"

class NetworkDialog : public QWidget {
	Q_OBJECT

public:
	/** Default Constructor */
	NetworkDialog(QWidget *parent = 0);

public slots:
	void insertConnect();
        void setLogInfo(QString info);

private slots:

	void displayInfoLogMenu(const QPoint& point);
	void on_actionSelect_All_triggered();
	void on_actionCopy_triggered();
	void on_actionClear_Log_triggered();

private:


	QTreeWidgetItem *getCurrentNeighbour();

	QTreeWidget *connecttreeWidget;

	class NetworkView *networkview;

	/** Qt Designer generated object */
	Ui::NetworkDialog ui;
};

#endif

