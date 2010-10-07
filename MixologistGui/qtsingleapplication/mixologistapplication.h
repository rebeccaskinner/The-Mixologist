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

#include "qtsingleapplication.h"
#if defined(Q_WS_WIN)
#include <qt_windows.h>
#endif

class MixologistApplication : public QtSingleApplication
{
public:
    MixologistApplication(int &argc, char **argv)
        : QtSingleApplication(argc, argv){}

    /*By reimplementing the winEventFilter, we can catch Windows
      shutdown signals, and shutdown the Mixologist.*/
#if defined(Q_WS_WIN)
    bool winEventFilter(MSG * msg, long * retVal);
#endif
    /*By reimplementing the event function, we can catch OS X
      url open events.*/
#if defined(Q_WS_MAC)
    bool event(QEvent *event);
#endif
};
