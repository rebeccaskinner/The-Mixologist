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

#include "pqi/pqihandler.h"

#include "util/debug.h"

pqihandler::pqihandler() {
    QMutexLocker stack(&coreMtx);

    // setup minimal total+individual rates.
    maxIndivOut = 0.01;
    maxIndivIn = 0.01;
    maxTotalOut = 0.01;
    maxTotalIn = 0.01;
    return;
}

int pqihandler::tick() {
    int moreToTick = 0;

    {
        QMutexLocker stack(&coreMtx);

        foreach(PQInterface* currentInterface, connectionsToFriends.values()) {
            if (currentInterface->tick() > 0)
                moreToTick = 1;
        }

        // get the items, and queue them correctly
        if (0 < locked_GetItems()) {
            moreToTick = 1;
        }
    } /****** UNLOCK ******/

    updateRateCaps();
    return moreToTick;
}

bool pqihandler::AddPQI(PQInterface *pqi) {
    QMutexLocker stack(&coreMtx);

    if (pqi->LibraryMixerId() == 0) {
        log(LOG_ALERT, PQIHANDLERZONE, "pqihandler::AddPQI() No LibraryMixer ID supplied");
        return false;
    }

    if (connectionsToFriends.contains(pqi->LibraryMixerId())) {
        log(LOG_ALERT, PQIHANDLERZONE, "ERROR pqi already exists!");
        return false;
    }

    connectionsToFriends[pqi->LibraryMixerId()] = pqi;
    return true;
}

// generalised output
int pqihandler::HandleNetItem(NetItem *item) {
    QMutexLocker stack(&coreMtx);

    QMap<std::string, PQInterface *>::iterator it;
    log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "pqihandler::HandleNetItem()");

    if (!connectionsToFriends.contains(item->LibraryMixerId())) {
        log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "pqihandler::HandleNetItem() Invalid cert_id!");

        delete item;
        return -1;
    }

    connectionsToFriends[item->LibraryMixerId()]->SendItem(item);
    return 1;
}

int pqihandler::SendFileRequest(FileRequest *ns) {
    return HandleNetItem(ns);
}

int pqihandler::SendFileData(FileData *ns) {
    return HandleNetItem(ns);
}

int pqihandler::SendRawItem(RawItem *ns) {
    return HandleNetItem(ns);
}


// inputs. This is a very basic
// system that is completely biased and slow...
// someone please fix.

int pqihandler::locked_GetItems() {
    QMap<std::string, PQInterface *>::iterator it;

    NetItem *item;
    int count = 0;

    foreach (PQInterface* currentFriend, connectionsToFriends.values()) {
        while ((item = currentFriend->GetItem()) != NULL) {
            if (item->LibraryMixerId() != currentFriend->LibraryMixerId()) {
                log(LOG_ALERT, PQIHANDLERZONE, "ERROR IDs dont match!");
                item->LibraryMixerId(currentFriend->LibraryMixerId());
            }

            locked_SortnStoreItem(item);
            count++;
        }
    }

    return count;
}

void pqihandler::locked_SortnStoreItem(NetItem *item) {
    /* get class type / subtype out of the item */
    uint8_t vers    = item->PacketVersion();
    uint8_t cls     = item->PacketClass();
    uint8_t type    = item->PacketType();
    uint8_t subtype = item->PacketSubType();

    /* whole Version reserved for SERVICES/CACHES */
    if (vers == PKT_VERSION_SERVICE) {
        log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "SortnStore->Service");
        in_service.push_back(item);
        item = NULL;
        return;
    }

    if (vers != PKT_VERSION1) {
        log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "SortnStore->Invalid VERSION! Deleting!");
        delete item;
        item = NULL;
        return;
    }

    switch (cls) {
        case PKT_CLASS_BASE:
            switch (type) {
                case PKT_TYPE_FILE:
                    switch (subtype) {
                        case PKT_SUBTYPE_FI_REQUEST:
                            log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "SortnStore->File Request");
                            in_request.push_back(item);
                            item = NULL;
                            break;

                        case PKT_SUBTYPE_FI_DATA:
                            log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "SortnStore->File Data");
                            in_data.push_back(item);
                            item = NULL;
                            break;

                        default:
                            break; /* no match! */
                    }
                    break;

                default:
                    break;  /* no match! */
            }
            break;

        default:
            log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "SortnStore->Unknown");
            break;

    }

    if (item) {
        log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "SortnStore->Deleting Unsorted Item");
        delete item;
    }

    return;
}

FileRequest *pqihandler::GetFileRequest() {
    QMutexLocker stack(&coreMtx);

    if (in_request.size() != 0) {
        FileRequest *fi = dynamic_cast<FileRequest *>(in_request.front());
        if (!fi) {
            delete in_request.front();
        }
        in_request.pop_front();
        return fi;
    }
    return NULL;
}

FileData *pqihandler::GetFileData() {
    QMutexLocker stack(&coreMtx);

    if (in_data.size() != 0) {
        FileData *fi = dynamic_cast<FileData *>(in_data.front());
        if (!fi) {
            delete in_data.front();
        }
        in_data.pop_front();
        return fi;
    }
    return NULL;
}

RawItem *pqihandler::GetRawItem() {
    QMutexLocker stack(&coreMtx);

    if (in_service.size() != 0) {
        RawItem *fi = dynamic_cast<RawItem *>(in_service.front());
        if (!fi) {
            delete in_service.front();
        }
        in_service.pop_front();
        return fi;
    }
    return NULL;
}

static const float MIN_RATE = 0.01; // 10 B/s

// internal fn to send updates
void pqihandler::updateRateCaps() {
    QMap<std::string, PQInterface *>::iterator it;
    int numberFriends = connectionsToFriends.size();

    //Total bandwidth available
    float total_max_rate_in = getMaxRate(true);
    float total_max_rate_out = getMaxRate(false);

    //Max bandwidth per pqi
    float indiv_max_rate_in = getMaxIndivRate(true);
    float indiv_max_rate_out = getMaxIndivRate(false);

    //The average bandwidth available if max total bandwidth cap was equally shared among pqi
    float shared_max_rate_in = total_max_rate_in/numberFriends;
    float shared_max_rate_out = total_max_rate_out/numberFriends;

    //Total of bandwidth being used
    float used_bw_in = 0;
    float used_bw_out = 0;

    //The total amount of all pqi's bandwidth use over the avg_rate
    float extra_bw_in = 0;
    float extra_bw_out = 0;

    //The number of transfers that are bumping off the individual rate limiter and over the avg_rate
    //Used so we know when not to bother trying to increase the speed anymore (when all pqi are maxed)
    int maxed_in = 0;
    int maxed_out = 0;

    /* Lock once rates have been retrieved */
    QMutexLocker stack(&coreMtx);

    foreach (PQInterface *currentFriend, connectionsToFriends.values()) {
        //Actual rates being used by this pqi
        float pqi_rate_in = currentFriend->getRate(true);
        float pqi_rate_out = currentFriend->getRate(false);

        used_bw_in += pqi_rate_in;
        used_bw_out += pqi_rate_out;

        if (pqi_rate_in > shared_max_rate_in) {
            if (currentFriend->getMaxRate(true) == indiv_max_rate_in) {
                maxed_in++;
            }
            extra_bw_in +=  pqi_rate_in - shared_max_rate_in;
        }
        if (pqi_rate_out > shared_max_rate_out) {
            if (currentFriend->getMaxRate(false) == indiv_max_rate_out) {
                maxed_out++;
            }
            extra_bw_out +=  pqi_rate_out - shared_max_rate_out;
        }
    }

    setRateCaps(true, total_max_rate_in, indiv_max_rate_in, shared_max_rate_in, used_bw_in, extra_bw_in, maxed_in, numberFriends);
    setRateCaps(false, total_max_rate_out, indiv_max_rate_out, shared_max_rate_out, used_bw_out, extra_bw_out, maxed_out, numberFriends);
}

void pqihandler::setRateCaps(bool downloading, float total_max_rate, float indiv_max_rate, float shared_max_rate,
                             float used_bw, float extra_bw, int maxed, int numberFriends) {
    QMap<std::string, PQInterface *>::iterator it;
    //If there is no limit, just set all pqi to individual max
    if (total_max_rate == 0){
        foreach (PQInterface *currentFriend, connectionsToFriends.values()) {
            currentFriend->setMaxRate(downloading, indiv_max_rate);
        }
    }
    //If already over the cap, target those that are above average to be slowed down
    else if (used_bw > total_max_rate) {
        float fchg = (used_bw - total_max_rate) / (float) extra_bw;
        foreach (PQInterface *currentFriend, connectionsToFriends.values()) {
            float pqi_rate = currentFriend->getRate(downloading);
            float new_max = shared_max_rate;
            if (pqi_rate > shared_max_rate) {
                new_max = shared_max_rate + (1 - fchg) *
                          (pqi_rate - shared_max_rate);
            }
            if (new_max > indiv_max_rate) {
                new_max = indiv_max_rate;
            }
            currentFriend->setMaxRate(downloading, new_max);
            log(LOG_DEBUG_ALERT, PQIHANDLERZONE, "setRateCaps() Slowed down a pqi for fairness - " + QString::number(currentFriend->LibraryMixerId()));
        }
    }
    //If not maxed already and using less than 95%, increase limit
    else if ((maxed != numberFriends) && (used_bw < 0.95 * total_max_rate)) {
        float percent_available = (total_max_rate - used_bw) / total_max_rate;
        foreach (PQInterface * currentFriend, connectionsToFriends.values()) {
            float pqi_rate = currentFriend->getRate(downloading);
            float pqi_set_max = currentFriend->getMaxRate(downloading);

            if (pqi_set_max != indiv_max_rate) {
                float new_max = pqi_set_max;
                if (pqi_set_max < shared_max_rate) {
                    new_max = shared_max_rate * (1 + percent_available);
                    log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "setRateCaps() Set a pqi speed cap based on shared max rate - " + QString::number(currentFriend->LibraryMixerId()));
                } else if (pqi_rate > 0.5 * pqi_set_max) {
                    new_max =  pqi_set_max * (1 + percent_available);
                    log(LOG_DEBUG_BASIC, PQIHANDLERZONE, "setRateCaps() Set a pqi speed cap based on its old max rate - " + QString::number(currentFriend->LibraryMixerId()));
                }
                if (new_max > indiv_max_rate) {
                    new_max = indiv_max_rate;
                }
                currentFriend->setMaxRate(downloading, new_max);
            }
        }
    }
}
