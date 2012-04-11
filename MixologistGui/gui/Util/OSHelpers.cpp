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

#include <gui/Util/OSHelpers.h>
#include <QDir>
#include <QCoreApplication>

#if defined(Q_WS_MAC)
#endif

#if defined(WIN32)
#define STARTUP_REG_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define MIXOLOGIST_STARTUP_REG_KEY "Mixologist"
#define MIXOLOGY_LINK_REG_KEY "mixology"
#define MIXOLOGY_LINK_COMMAND_REG_KEY "mixology\\shell\\open\\command"
#endif

bool canHandleLinkAssociation() {
#if defined(Q_WS_WIN)
    return true;
#else
    return false;
#endif
}

bool setMixologyLinksAssociated() {
#if defined(WIN32)
    win32_registry_set_key_value(HKEY_CLASSES_ROOT, MIXOLOGY_LINK_REG_KEY, "", "URL:mixology Protocol");
    win32_registry_set_key_value(HKEY_CLASSES_ROOT, MIXOLOGY_LINK_REG_KEY, "URL Protocol", "");
    win32_registry_set_key_value(HKEY_CLASSES_ROOT, MIXOLOGY_LINK_COMMAND_REG_KEY, "",
                                 QString("\"" + QDir::convertSeparators(QCoreApplication::applicationFilePath()) +
                                         "\"" + " \"%1\""));
    return getMixologyLinksAssociated();
#else
    /* Platforms other than windows aren't supported yet */
    return true;
#endif
}

/** Returns whether mixology: links are associated with the Mixologist. */
bool getMixologyLinksAssociated() {
#if defined(WIN32)
    return (win32_registry_get_key_value(HKEY_CLASSES_ROOT, MIXOLOGY_LINK_COMMAND_REG_KEY, "") ==
            QString("\"" + QDir::convertSeparators(QCoreApplication::applicationFilePath()) + "\"" + " \"%1\""));
#else
    /* Platforms other than windows aren't supported yet */
    return true;
#endif
}

bool canHandleRunOnBoot() {
#if defined(Q_WS_WIN)
    return true;
#else
    return false;
#endif
}

void setRunOnBoot(bool run) {
#if defined(WIN32)
    if (run) {
        win32_registry_set_key_value(HKEY_CURRENT_USER, STARTUP_REG_KEY, MIXOLOGIST_STARTUP_REG_KEY,
                                     QString("\"" +
                                             QDir::convertSeparators(QCoreApplication::applicationFilePath())) +
                                     "\"" + " -startminimized");
    } else {
        win32_registry_remove_key(HKEY_CURRENT_USER, STARTUP_REG_KEY, MIXOLOGIST_STARTUP_REG_KEY);
    }
#elif defined(Q_WS_MAC)
    (void) run;
    return;
#else

    /* Platforms other than windows aren't supported yet */
    (void) run;
    return;
#endif
}

bool getRunOnBoot() {
#if defined(WIN32)
    if (!win32_registry_get_key_value(HKEY_CURRENT_USER, STARTUP_REG_KEY, MIXOLOGIST_STARTUP_REG_KEY).isEmpty()) {
        return true;
    } else {
        return false;
    }
#elif defined(Q_WS_MAC)
    return false;
#else
    /* Other platforms aren't supported yet */
    return false;
#endif
}

#if defined(WIN32)

/** Returns the value in keyName at keyLocation.
 *  Returns an empty QString if the keyName doesn't exist */
QString win32_registry_get_key_value(HKEY hkey, QString keyLocation, QString keyName) {
#ifdef WIN32
    HKEY hkResult;
    char data[255] = {0};
    DWORD size = sizeof(data);

    /* Open the key for reading (opens new key if it doesn't exist) */
    if (RegOpenKeyExA(hkey,
                      qPrintable(keyLocation),
                      0L, KEY_READ, &hkResult) == ERROR_SUCCESS) {

        /* Key exists, so read the value into data */
        RegQueryValueExA(hkResult, qPrintable(keyName),
                         NULL, NULL, (LPBYTE)data, &size);
    }

    /* Close anything that was opened */
    RegCloseKey(hkResult);

    return QString(data);
#else
    return QString();
#endif
}

/** Creates and/or sets the key to the specified value */
void win32_registry_set_key_value(HKEY hkey, QString keyLocation, QString keyName, QString keyValue) {
#ifdef WIN32
    HKEY hkResult;

    /* Open the key for writing (opens new key if it doesn't exist */
    if (RegOpenKeyExA(hkey,
                      qPrintable(keyLocation),
                      0, KEY_WRITE, &hkResult) != ERROR_SUCCESS) {

        /* Key didn't exist, so write the newly opened key */
        RegCreateKeyExA(hkey,
                        qPrintable(keyLocation),
                        0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
                        &hkResult, NULL);
    }

    /* Save the value in the key */
    RegSetValueExA(hkResult, qPrintable(keyName), 0, REG_SZ,
                   (BYTE *)qPrintable(keyValue),
                   (DWORD)keyValue.length() + 1); // include null terminator

    /* Close the key */
    RegCloseKey(hkResult);
#endif
}

/** Removes the key from the registry if it exists */
void win32_registry_remove_key(HKEY hkey, QString keyLocation, QString keyName) {
#ifdef WIN32
    HKEY hkResult;

    /* Open the key for writing (opens new key if it doesn't exist */
    if (RegOpenKeyExA(hkey,
                      qPrintable(keyLocation),
                      0, KEY_SET_VALUE, &hkResult) == ERROR_SUCCESS) {

        /* Key exists so delete it */
        RegDeleteValueA(hkResult, qPrintable(keyName));
    }

    /* Close anything that was opened */
    RegCloseKey(hkResult);
#endif
}
#endif
