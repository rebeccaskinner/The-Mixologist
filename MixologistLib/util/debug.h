/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-8, Robert Fernie
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

#ifndef LOG_DEBUG_H
#define LOG_DEBUG_H

#include <QString>

/*
 * All logging at or below LOG_DEBUG_ALERT should begin with the containing class and function.
 * All logging at or above LOG_WARNING should not include those, but rather be in plain text form suitable for end-users.
 */
#define LOG_NONE        -1
#define LOG_ALERT        1
#define LOG_ERROR        3
#define LOG_WARNING      5 //Default log level
#define LOG_DEBUG_ALERT  6
#define LOG_DEBUG_BASIC  8
#define LOG_DEBUG_ALL   10

int setOutputLevel(int lvl);
int setZoneLevel(int lvl, int zone);
int log(unsigned int lvl, int zone, QString msg);

/* Retaining old #DEFINES and functions for backward compatibility */

#define pqioutput log

#define PQL_ALERT   LOG_ALERT
#define PQL_ERROR   LOG_ERROR
#define PQL_WARNING     LOG_WARNING
#define PQL_DEBUG_ALERT LOG_DEBUG_ALERT
#define PQL_DEBUG_BASIC LOG_DEBUG_BASIC
#define PQL_DEBUG_ALL   LOG_DEBUG_ALL

/* Zone constants for various files. */
#define PQISTREAMERZONE 8221
#define MIXOLOGYSERVICEZONE 12409
#define FTTRANSFERMODULEZONE 29384
#define FTCONTROLLERZONE 29422
#define FTDATADEMULTIPLEXZONE 29592
#define FTFILEPROVIDERZONE 29800
#define FTFILECREATORZONE 29801
#define PQIHANDLERZONE 34283
#define PQISSLZONE 37714
#define AUTHMGRZONE 38383
#define UPNPHANDLERZONE 99283
#endif
