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

#include <QStringList>

#ifndef UTIL_HELPERS_H
#define UTIL_HELPERS_H


        /*Takes the QMimeData from a drop event, and parses out the paths of the drop.
          All files are added to a QStringList and returned.
          If the drop contains folders, recursively adds the files in those folders.*/
        QStringList recursiveFileAdd(const QString fileEntry);

        //Returns the rot13 encrypted result of input
        QString rot13(const QString & input);

        //Encrypt and decrypt files with source and destination referring to paths
        //bool encrypt(const QString source, const QString destination);
        //bool decrypt(const QString source, const QString destination);

#endif
