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

#include <gui/Util/Helpers.h>
#include <QDir>
#include <iostream>

QStringList recursiveFileAdd(const QString fileEntry) {
    QStringList paths;
    /* First test if the drop is a directory.  If it is add all files.  Otherwise it must be a file, so add it. */
    if (fileEntry == "." || fileEntry == ".." || fileEntry == "") return paths;
    QDir directory(fileEntry);
    if (directory.exists()) {
        foreach (QFileInfo subFile, directory.entryInfoList()) {
            if (subFile.fileName() == "." || subFile.fileName() == "..") continue;
            paths << recursiveFileAdd(subFile.canonicalFilePath());
        }
    } else paths << fileEntry;

    return paths;
}

QString rot13(const QString &input) {
    std::string str = input.toStdString();
    std::string ret = str;

    int i=0;

    while (str[i] != '\0') {
        if (str[i] >= 'a' && str[i] <= 'z')
            ret[i] = (str[i] - 'a' + 13) % 26 + 'a';
        else if (str[i] >= 'A' && str[i] <= 'Z')
            ret[i] = (str[i] - 'A' + 13) % 26 + 'A';
        i++;
    }
    return ret.c_str();
}
