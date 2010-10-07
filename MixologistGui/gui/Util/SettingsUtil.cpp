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

#include <gui/Util/SettingsUtil.h>
#include <QWidget>
#include <QSettings>

void SettingsUtil::saveWidgetInformation(QWidget *widget, QString settingsFile) {
    QSettings settings(settingsFile, QSettings::IniFormat);
    settings.beginGroup("Geometry");
    settings.beginGroup(widget->objectName());

    settings.setValue("size", widget->size());
    settings.setValue("pos", widget->pos());

    settings.endGroup();
    settings.endGroup();
}


void SettingsUtil::loadWidgetInformation(QWidget *widget, QString settingsFile) {
    QSettings settings(settingsFile, QSettings::IniFormat);
    settings.beginGroup("Geometry");
    settings.beginGroup(widget->objectName());

    widget->resize(settings.value("size", widget->size()).toSize());
    widget->move(settings.value("pos", QPoint(200, 200)).toPoint());

    settings.endGroup();
    settings.endGroup();
}

