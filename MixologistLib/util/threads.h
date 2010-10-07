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

#ifndef INTERFACE_THREADS_H
#define INTERFACE_THREADS_H

#include <pthread.h>
#include <inttypes.h>

/* Interface Thread Wrappers */

class MixMutex {
public:

    MixMutex() {
        pthread_mutex_init(&realMutex, NULL);
    }
    ~MixMutex() {
        pthread_mutex_destroy(&realMutex);
    }
    void    lock() {
        pthread_mutex_lock(&realMutex);
    }
    void    unlock() {
        pthread_mutex_unlock(&realMutex);
    }
    bool    trylock() {
        return (0 == pthread_mutex_trylock(&realMutex));
    }

private:
    pthread_mutex_t  realMutex;
};

class MixStackMutex {
public:

    MixStackMutex(MixMutex &mtx): mMtx(mtx) {
        mMtx.lock();
    }
    ~MixStackMutex() {
        mMtx.unlock();
    }

private:
    MixMutex &mMtx;
};

class MixThread;

/* to create a thread! */
pthread_t  createThread(MixThread &thread);

class MixThread {
public:
    MixThread() {
        return;
    }
    virtual ~MixThread() {
        return;
    }

    virtual void start() {
        createThread(*this);
    }
    virtual void run() = 0; /* called once the thread is started */

    pthread_t mTid;
    MixMutex   mMutex;
};


class MixQueueThread: public MixThread {
public:

    MixQueueThread(uint32_t min, uint32_t max, double relaxFactor );
    virtual ~MixQueueThread() {
        return;
    }

    virtual void run();

protected:

    virtual bool workQueued() = 0;
    virtual bool doWork() = 0;

private:
    uint32_t mMinSleep; /* ms */
    uint32_t mMaxSleep; /* ms */
    uint32_t mLastSleep; /* ms */
    time_t   mLastWork;  /* secs */
    float    mRelaxFactor;
};


#endif
