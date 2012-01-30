/****************************************************************
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 ****************************************************************/

#ifndef VERSION_HEADER
#define VERSION_HEADER

#include <QString>

#define VERSION Q_UINT64_C(2012013015)

namespace VersionUtil {
    QString inline convert_to_display_version(qlonglong inputVersion) {
        int indexOfTime = QString::number(inputVersion).length() - 2;
        int indexOfDate = QString::number(inputVersion).length() - 4;
        int indexOfMonth = QString::number(inputVersion).length() - 6;
        QString versionString = QString::number(inputVersion);
        versionString.insert(indexOfTime, ":");
        versionString.insert(indexOfDate, "/");
        versionString.insert(indexOfMonth, "/");
        return versionString;
    }

    QString inline display_version() {return convert_to_display_version(VERSION);}
}

#endif
