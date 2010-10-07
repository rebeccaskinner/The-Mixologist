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

#include "mixologistapplication.h"
#include <QFileOpenEvent>
#include <QUrl>

#if defined(Q_WS_WIN)
bool MixologistApplication::winEventFilter(MSG *msg, long *retVal) {
    if (msg->message == WM_QUERYENDSESSION ) {
        *retVal = 1;
        return true;
    }
    return false;
}
#endif

#if defined(Q_WS_MAC)
bool MixologistApplication::event(QEvent *event) {
    if (event->type() == QEvent::FileOpen) {
        emit messageReceived(static_cast<QFileOpenEvent *>(event)->url().toString());
        return true;
    }
    return QtSingleApplication::event(event);
}

#endif
