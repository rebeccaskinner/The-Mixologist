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

#if defined(WIN32)
#include <windows.h>
#endif
#include <QString>

#ifndef OSHELPERS_H
#define OSHELPERS_H

        /** Returns true if on the current platform, link association can be handled from in the Mixologist. */
        bool canHandleLinkAssociation();
        /** Set mixology: links to be associated with the Mixologist. True on success, false on failure */
        bool setMixologyLinksAssociated();
        /** Returns whether mixology: links are associated with the Mixologist. */
        bool getMixologyLinksAssociated();
        /** Returns true if on the current platform, start on boot can be handled from in the Mixologist. */
        bool canHandleRunOnBoot();
        /** Set whether to run on system boot. */
        void setRunOnBoot(bool run);
        /** Returns whether starts on system boot. */
        bool getRunOnBoot();

#if defined(WIN32)
        /** Returns value of keyName or empty QString if keyName doesn't exist */
        QString win32_registry_get_key_value(HKEY hkey, QString keyLocation, QString keyName);

        /** Creates and/or sets the key to the specified value */
        void win32_registry_set_key_value(HKEY hkey, QString keyLocation, QString keyName, QString keyValue);

        /** Removes the key from the registry if it exists */
        void win32_registry_remove_key(HKEY hkey, QString keyLocation, QString keyName);
#endif //WIN32

#endif
