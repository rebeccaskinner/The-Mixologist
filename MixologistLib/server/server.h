/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-6, Robert Fernie
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

#ifndef MRK_P3_INTERFACE_H
#define MRK_P3_INTERFACE_H

#include "interface/iface.h"
#include "util/threads.h"

/*
The main thread that does most of the work, and implements the Control interface in iface.h that provides high-level control over MixologistLib.
*/

class p3ConnectMgr;
class AuthMgr;
class pqipersongrp;
class pqiNetAssistFirewall;
class p3DhtMgr;
class p3ChatService;
class ftServer;

class Server: public Control, public MixThread {
public:
    Server(){return;}
    virtual ~Server(){return;};

    virtual bool StartupMixologist();
    virtual bool ShutdownMixologist();

    virtual void ReloadTransferRates();

    /* This is the main MixologistLib loop, handles the ticking */
    virtual void run();

    /* locking stuff */
    void lockCore() {
        coreMutex.lock();
    }

    void unlockCore() {
        coreMutex.unlock();
    }

private:

    /* mutex */
    MixMutex coreMutex;

    /* General Internal Helper Functions (Must be Locked)
    */
    double getCurrentTS();

public:

    //These important public variables hold pointers to the single instances of each of the below
    pqipersongrp *pqih;
    p3DhtMgr  *mDhtMgr;

    friend class Init;
};

#endif
