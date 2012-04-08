/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2008, Robert Fernie
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

#include <time.h>
#include <ft/fttransfermodule.h>
#include <ft/ftserver.h>
#include <pqi/friendsConnectivityManager.h>
#include <interface/peers.h>
#include <interface/files.h>
#include <util/debug.h>

//The amount of time we're targeting to be able to complete one rtt measurement cycle in
const int32_t FT_TM_STD_RTT = 9; //9 seconds
//The minimum amount of time that can pass in one rtt measurement cycle that we'll recognize
const int32_t FT_TM_FAST_RTT = 1; //1 second

ftTransferModule::ftTransferModule(unsigned int initial_friend_id, uint64_t size, const QString &hash)
    :actualRate(0) {
    QMutexLocker stack(&tfMtx);

    QString temporaryLocation =  files->getPartialsDirectory() + QDir::separator() + hash;
    mFileCreator = new ftFileCreator(temporaryLocation, size, hash);

    peerInfo initialPeer(initial_friend_id);
    if (friendsConnectivityManager->isOnline(initial_friend_id)) {
        initialPeer.state = peerInfo::PQIPEER_ONLINE_IDLE;
    } else {
        initialPeer.state = peerInfo::PQIPEER_NOT_ONLINE;
    }

    mFileSources.insert(initialPeer.librarymixer_id, initialPeer);

    /* If we're already done on completion (i.e. resuming a multifile transfer where some files are done)
       then flag this as completed.
       Otherwise, set this to waiting so that the ftController can start it when appropriate. */
    if (mFileCreator->finished()) mTransferStatus = FILE_COMPLETE;
    else mTransferStatus = FILE_WAITING;
}

ftTransferModule::~ftTransferModule() {
    delete mFileCreator;
}

int ftTransferModule::tick() {
    /* Cache the current status so we don't need to hold the mutex overly long. */
    fileTransferStatus currentStatus;
    {
        QMutexLocker stack(&tfMtx);
        currentStatus = mTransferStatus;
    }

    switch (currentStatus) {
    case FILE_DOWNLOADING:
        {
            QMutexLocker stack(&tfMtx);
            actualRate = 0;
            foreach (unsigned int librarymixer_id, mFileSources.keys()) {
                locked_tickPeerTransfer(mFileSources[librarymixer_id]);
                actualRate += mFileSources[librarymixer_id].actualRate;
            }

            mFileCreator->tick();
        }
        break;
    case FILE_COMPLETE:
        return 1;
    default:
        break;
    }

    return 0;
}

void ftTransferModule::friendConnected(unsigned int friend_id) {
    QMutexLocker stack(&tfMtx);

    if (!mFileSources.contains(friend_id)) return;

    mFileSources[friend_id].state = peerInfo::PQIPEER_ONLINE_IDLE;
}

void ftTransferModule::friendDisconnected(unsigned int friend_id) {
    QMutexLocker stack(&tfMtx);

    if (!mFileSources.contains(friend_id)) return;

    mFileSources[friend_id].state = peerInfo::PQIPEER_NOT_ONLINE;

    /* Now that we're disconnected, don't expect to hear back on any of our pending requests. */
    mFileCreator->invalidateChunksRequestedFrom(friend_id);

    /* If we've been disconnected, set the module to restart the transfer rate next time. */
    mFileSources[friend_id].fastStart = true;
}

ftTransferModule::fileTransferStatus ftTransferModule::transferStatus() const {
    QMutexLocker stack(&tfMtx);
    return mTransferStatus;
}

void ftTransferModule::transferStatus(fileTransferStatus newStatus) {
    QMutexLocker stack(&tfMtx);
    mTransferStatus = newStatus;
}

bool ftTransferModule::getFileSources(QList<unsigned int> &sourceIds) {
    QMutexLocker stack(&tfMtx);
    foreach (unsigned int friend_id, mFileSources.keys()) {
        sourceIds.push_back(friend_id);
    }
    return true;
}

bool ftTransferModule::getPeerState(unsigned int librarymixer_id, uint32_t &state, uint32_t &tfRate) {
    QMutexLocker stack(&tfMtx);

    if (!mFileSources.contains(librarymixer_id)) return false;

    state = mFileSources[librarymixer_id].state;
    tfRate = (uint32_t) mFileSources[librarymixer_id].actualRate;

    return true;
}

bool ftTransferModule::recvFileData(unsigned int librarymixer_id, uint64_t offset, uint32_t chunk_size, void *data) {
    {
        QMutexLocker stack(&tfMtx);

        if (!mFileSources.contains(librarymixer_id)) {
            free(data);
            return false;
        }

        locked_recvDataUpdateStats(mFileSources[librarymixer_id], offset, chunk_size);
    }

    mFileCreator->addFileData(librarymixer_id, offset, chunk_size, data);

    return true;
}

//Start by requesting chunks of 25k. In practice this works out to a little over 5KB/s
const double FT_TM_FAST_START_RATE = 25600;
//Max number of times to try a source before marking it unavailable
const uint32_t FT_TM_MAX_RESETS = 5;
//Minimum mchunk size. In practice this works out to about 1/8KB/s
const uint32_t FT_TM_MINIMUM_CHUNK = 128;
//Amount of time to wait on a request before considering it dead and attempting a new one
const uint32_t FT_TM_REQUEST_TIMEOUT = 5; //5 seconds
//Amount of time between receiving before marking source as idle
const uint32_t FT_TM_DOWNLOAD_TIMEOUT = 10; //10 seconds

bool ftTransferModule::locked_tickPeerTransfer(peerInfo &currentPeer) {

    time_t currentTime = time(NULL);
    int ageReceiveTime = currentTime - currentPeer.lastReceiveTime;
    int ageRequestTime = currentTime - currentPeer.lastRequestTime;

    /* if offline - ignore */
    if (currentPeer.state == peerInfo::PQIPEER_NOT_ONLINE) return false;

    /* If we haven't made a new request in a long time.
       This can be either because of connection failure or because we were too aggressive in the amount we requested
       and it couldn't be completed in FT_TM_REQUEST_TIMEOUT */
    if (ageRequestTime > (int) (FT_TM_REQUEST_TIMEOUT * (currentPeer.numResets + 1))) {
        log(LOG_DEBUG_ALERT, FTTRANSFERMODULEZONE, "ftTransferModule::locked_tickPeerTransfer() request timeout");

#ifdef false
        //Multi-source stuff
        /* 2) Some of the peers might not have the file...care must be taken avoid deadlock.
        *
        * A edge case which used to fail badly.
        *  Small 1K file (one chunk), with 3 sources (A,B,C). A doesn't have file.
        *  (a) request data from A. B & C pause because no more data needed.
        *  (b) all timeout, chunk is back in the needed pool...then back to request again (a) and repeat.
        *  (c) all timeout x 5 and are disabled...no transfer, while B&C had it the whole time.
        *
        * To solve this we introduced an element of randomness to resets on timeout. */
        if (info.numResets > 1) { /* 3rd timeout */
            /* 90% chance of return false...
             * will mean variations in which peer
             * starts first. hopefully stop deadlocks.
             */
            if (qrand() % 10 != 0) return false;
        }
#endif
        /* reset, treat as if we received the last request so we can send a new request */
        currentPeer.numResets++;
        currentPeer.state = peerInfo::PQIPEER_DOWNLOADING;
        currentPeer.lastReceiveTime = currentTime;
        ageReceiveTime = 0;

#ifdef false
        /* We could potentially reach here because our friend doesn't have the requested file anymore.
           This will disable the file.
           For now, this has been turned off, we need transfer reliability more than bandwidth efficiency.
           We should have a no such file packet response to bad requests instead of this. */
        if (info.numResets >= FT_TM_MAX_RESETS) {
            log(LOG_DEBUG_ALERT, FTTRANSFERMODULEZONE, "ftTransferModule::locked_tickPeerTransfer() max resets reached");
            info.state = PQIPEER_NOT_ONLINE;
            return false;
        }
#endif
    }

    /* if we haven't received any data in a long time */
    if (ageReceiveTime > (int) FT_TM_DOWNLOAD_TIMEOUT) {
        log(LOG_DEBUG_ALERT, FTTRANSFERMODULEZONE, "ftTransferModule::locked_tickPeerTransfer() receive timeout");
        currentPeer.state = peerInfo::PQIPEER_ONLINE_IDLE;
        return false;
    }

    /* update rate, weighing 90% previous number and 10% current number */
    currentPeer.actualRate = currentPeer.actualRate * 0.5 + currentPeer.pastTickTransferred * 0.5;
    currentPeer.pastTickTransferred = 0;

    /* If more time has already passed in this rtt period than FT_TM_STD_RTT (our target time)
     * then halt any further rate increases for this period, since we were already too aggressive. */
    if ((currentPeer.rttActive) && ((currentTime - currentPeer.rttStart) > FT_TM_STD_RTT)) {
        if (currentPeer.mRateChange > 0) {
            log(LOG_DEBUG_ALERT, FTTRANSFERMODULEZONE, "ftTransferModule::locked_tickPeerTransfer() rate increases halted");
            currentPeer.mRateChange = 0;
        }
    }

    /* Calculate amount of data to request */
    uint32_t requestSize;
    if (currentPeer.fastStart) {
        requestSize = FT_TM_FAST_START_RATE;
        currentPeer.fastStart = false;
    } else {
        requestSize = currentPeer.actualRate * (1.0 + currentPeer.mRateChange);
    }
    if (requestSize < FT_TM_MINIMUM_CHUNK) {
        requestSize = FT_TM_MINIMUM_CHUNK;
        log(LOG_DEBUG_ALERT, FTTRANSFERMODULEZONE, "ftTransferModule::locked_tickPeerTransfer() minimum speed hit");
    }

    {
        QString toLog = "ftTransferModule::locked_tickPeerTransfer()";
        toLog += " actualRate: " + QString::number(actualRate);
        toLog += " desired next_req: " + QString::number(requestSize);
        log(LOG_DEBUG_BASIC, FTTRANSFERMODULEZONE, toLog);
    }

    /* do request */
    currentPeer.lastRequestTime = currentTime;
    uint64_t requestOffset = 0;
    if (mFileCreator->allocateRemainingChunk(currentPeer.librarymixer_id, requestOffset, requestSize) == false) {
        mTransferStatus = FILE_COMPLETE;
    } else {
        if (requestSize > 0) {
            currentPeer.state = peerInfo::PQIPEER_DOWNLOADING;
            {
                QString toLog = "ftTransferModule::locked_tickPeerTransfer() requesting data";
                toLog += (" hash: " + mFileCreator->getHash());
                toLog.append(" requestOffset: " + QString::number(requestOffset));
                toLog.append(" requestSize: " + QString::number(requestSize));
                log(LOG_DEBUG_ALERT, FTTRANSFERMODULEZONE, toLog);
            }
            ftserver->sendDataRequest(currentPeer.librarymixer_id, mFileCreator->getHash(), mFileCreator->getFileSize(), requestOffset, requestSize);

            /* if it's time to start next rtt measurement period */
            if (!currentPeer.rttActive) {
                currentPeer.rttStart = currentTime;
                currentPeer.rttActive = true;
                currentPeer.rttOffset = requestOffset + requestSize;
            }
        } else {
            log(LOG_DEBUG_ALERT, FTTRANSFERMODULEZONE, "ftTransferModule::locked_tickPeerTransfer() waiting for a chunk to become available for a new request");
        }
    }

    return true;
}

const double FT_TM_MAX_INCREASE = 1.00; //Doubling in speed
const double FT_TM_MIN_INCREASE = -0.10; //Dropping to 90% speed

void ftTransferModule::locked_recvDataUpdateStats(peerInfo &currentPeer, uint64_t startingByte, uint32_t chunk_size) {
    time_t currentTime = time(NULL);
    currentPeer.lastReceiveTime = currentTime;
    currentPeer.numResets = 0;
    currentPeer.state = peerInfo::PQIPEER_DOWNLOADING;
    currentPeer.pastTickTransferred += chunk_size;

    //If we have completed our rtt measurement cycle
    if ((currentPeer.rttActive) && (currentPeer.rttOffset == startingByte + chunk_size)) {

        int32_t rtt = time(NULL) - currentPeer.rttStart;

        /*
         * We will change the rate proportionally to the amount we differ from FT_TM_STD_RTT (9 seconds).
         * FT_TM_FAST_RTT = 1 sec. mRateChange =  1.00
         * FT_TM_STD_RTT = 9 sec. mRateChange =  0
         * i.e. 11 sec. mRateChange = -0.25
         */

        currentPeer.mRateChange = FT_TM_MAX_INCREASE *
                           (FT_TM_STD_RTT - rtt) /
                           (FT_TM_STD_RTT - FT_TM_FAST_RTT);

        if (currentPeer.mRateChange > FT_TM_MAX_INCREASE)
            currentPeer.mRateChange = FT_TM_MAX_INCREASE;

        if (currentPeer.mRateChange < FT_TM_MIN_INCREASE)
            currentPeer.mRateChange = FT_TM_MIN_INCREASE;

        currentPeer.rtt = rtt;
        currentPeer.rttActive = false;

        {
            QString toLog = "ftTransferModule::locked_recvDataUpdateStats() rtt calculation complete";
            toLog += " Updated Rate based on RTT: " + QString::number(rtt);
            toLog += " Rate: " + QString::number(currentPeer.mRateChange);
            log(LOG_DEBUG_BASIC, FTTRANSFERMODULEZONE, toLog);
        }

    }
}

