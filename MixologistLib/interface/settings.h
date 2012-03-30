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

#ifndef SETTINGS_H
#define SETTINGS_H

/*
  This file is a simple holder to be included by every file that needs to access the global settings.
*/

#include <QString>
#include <QSettings>

extern QString *startupSettings;
extern QString *mainSettings;
extern QString *savedTransfers;

const bool DEFAULT_START_MINIMIZED = false;
const bool DEFAULT_SHOW_ADVANCED = false;

const bool DEFAULT_ENABLE_OFF_LIBRARYMIXER_SHARING = false;

const bool DEFAULT_INCOMING_ASK = false;

const int DEFAULT_MAX_INDIVIDUAL_DOWNLOAD = 0;
const int DEFAULT_MAX_TOTAL_DOWNLOAD = 0;
const int DEFAULT_MAX_INDIVIDUAL_UPLOAD = 0;
const int DEFAULT_MAX_TOTAL_UPLOAD = 0;

const bool DEFAULT_NOTIFY_CONNECT = true;
const bool DEFAULT_NOTIFY_BAD_INTERNET = true;
const bool DEFAULT_NOTIFY_DOWNLOAD_DONE = true;

const bool DEFAULT_UPNP = true;
const int DEFAULT_PORT = -1;

const bool DEFAULT_TUTORIAL_DONE_INITIAL = false;
const bool DEFAULT_TUTORIAL_DONE_LIBRARY = false;
const bool DEFAULT_TUTORIAL_DONE_FRIENDS_LIBRARY = false;

#define DEFAULT_MIXOLOGY_SERVER "LibraryMixer"
#define DEFAULT_MIXOLOGY_SERVER_VALUE "https://librarymixer.heroku.com"
//This is a temporary hack, as it seems like the current QT version used (4.6) does not support SNI SSL used by LibraryMixer
#define DEFAULT_MIXOLOGY_SERVER_FIXED_VALUE "https://www.librarymixer.com"

#endif
