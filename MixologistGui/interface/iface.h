/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2008, Robert Fernie.
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

#ifndef GUI_INTERFACE_H
#define GUI_INTERFACE_H

#include "types.h"

#include <map>

#include <QObject>
#include <QString>

class Control;
class Init;

extern Control *control;

/*
The interface for controlling MixologistLib at a high-level.
*/
class Control {
public:

    Control(){}

    virtual ~Control() {}

    /* Starts all of MixologistLib's threads. */
    virtual bool StartupMixologist() = 0;
    /* Shuts down connections. */
    virtual bool ShutdownMixologist() = 0;

    /* Reloads transfer limits set in settings.ini by the user. */
    virtual void ReloadTransferRates() = 0;

    /* Used so that the GUI can inform the library of what version the client is, which is shared with friends on connect.
       setVersion is not thread-safe but is only being called once on startup before any reading could happen, and then by the status service.
       If any other thread is going to access these variables, make sure to update and make this thread-safe.
       The clientName is the name of that client, and can be anything, but for the Mixologist is simply Mixologist.
       The clientVersion is the version number with that client.
       The latestKnownVersion indicates that the user has indicated he should not be informed about new versions equal to or lower than that version. */
    virtual void setVersion(const QString &clientName, qulonglong clientVersion, qulonglong latestKnownVersion) = 0;
    virtual QString clientName() = 0;
    virtual qulonglong clientVersion() = 0;
    virtual qulonglong latestKnownVersion() = 0;
};


#endif //GUI_INTERFACE_H
