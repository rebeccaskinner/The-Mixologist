/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-7, Robert Fernie
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


#include "interface/iface.h"
#include "util/dir.h"
#include "util/threads.h" //for the MixMutex

Iface::Iface(NotifyBase &callback)
    :cb(callback) {
    ifaceMutex = new MixMutex();
    return;

}
Iface::~Iface() {
    delete ifaceMutex;
    return;
}

/* set to true */
bool    Iface::setChanged(DataFlags set) {
    if ((int) set < (int) NumOfFlags) {
        /* go for it */
        mChanged[(int) set ] = true;
        return true;
    }
    return false;
}


/* leaves it */
bool    Iface::getChanged(DataFlags set) {
    if ((int) set < (int) NumOfFlags) {
        /* go for it */
        return mChanged[(int) set ];
    }
    return false;
}

/* resets it */
bool    Iface::hasChanged(DataFlags set) {
    if ((int) set < (int) NumOfFlags) {
        /* go for it */
        if (mChanged[(int) set ]) {
            mChanged[(int) set ] = false;
            return true;
        }
    }
    return false;
}

void    Iface::lockData() {
    return ifaceMutex->lock();
}
void    Iface::unlockData() {
    return ifaceMutex->unlock();
}
