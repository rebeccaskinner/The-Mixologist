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


#include "threads.h"
#include <unistd.h>    /* for usleep() */

/*******
 * #define DEBUG_THREADS 1
 *******/

#ifdef DEBUG_THREADS
#include <iostream>
#endif

extern "C" void *mixthread_init(void *p) {
    MixThread *thread = (MixThread *) p;
    if (!thread) {
        return 0;
    }
    thread -> run();
    return 0;
}


pthread_t  createThread(MixThread &thread) {
    pthread_t tid;
    void  *data = (void *) (&thread);

    thread.mMutex.lock();
    {
        pthread_create(&tid, 0, &mixthread_init, data);
        thread.mTid = tid;
    }
    thread.mMutex.unlock();

    return tid;

}


MixQueueThread::MixQueueThread(uint32_t min, uint32_t max, double relaxFactor )
    :mMinSleep(min), mMaxSleep(max), mRelaxFactor(relaxFactor) {
    mLastSleep = (uint32_t)mMinSleep ;
}

void MixQueueThread::run() {
    while (1) {
        bool doneWork = false;
        while (workQueued() && doWork()) {
            doneWork = true;
        }
        time_t now = time(NULL);
        if (doneWork) {
            mLastWork = now;
            mLastSleep = (uint32_t) (mMinSleep + (mLastSleep - mMinSleep) / 2.0);
#ifdef DEBUG_THREADS
            std::cerr << "MixQueueThread::run() done work: sleeping for: " << mLastSleep;
            std::cerr << " ms";
            std::cerr << std::endl;
#endif

        } else {
            uint32_t deltaT = now - mLastWork;
            double frac = deltaT / mRelaxFactor;

            mLastSleep += (uint32_t)
                          ((mMaxSleep-mMinSleep) * (frac + 0.05));
            if (mLastSleep > mMaxSleep) {
                mLastSleep = mMaxSleep;
            }
#ifdef DEBUG_THREADS
            std::cerr << "MixQueueThread::run() no work: sleeping for: " << mLastSleep;
            std::cerr << " ms";
            std::cerr << std::endl;
#endif
        }
#ifdef WIN32
        Sleep(mLastSleep);
#else
        usleep(1000 * mLastSleep);
#endif
    }
}

