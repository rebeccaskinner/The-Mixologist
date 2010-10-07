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

#include <sstream>
#include "util/debug.h"
const int pqihandlerzone = 34283;

/****
#define DEBUG_TICK 1
#define NETITEM_DEBUG 1
****/

pqihandler::pqihandler(SecurityPolicy *Global) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

    // The global security....
    // if something is disabled here...
    // cannot be enabled by module.
    globsec = Global;

    {
        std::ostringstream out;
        out  << "New pqihandler()" << std::endl;
        out  << "Security Policy: " << secpolicy_print(globsec);
        out  << std::endl;
        pqioutput(PQL_DEBUG_BASIC, pqihandlerzone, out.str().c_str());
    }

    // setup minimal total+individual rates.
    rateIndiv_out = 0.01;
    rateIndiv_in = 0.01;
    rateMax_out = 0.01;
    rateMax_in = 0.01;
    return;
}

int pqihandler::tick() {
    int moreToTick = 0;

    {
        MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

        // tick all interfaces...
        std::map<std::string, SearchModule *>::iterator it;
        for (it = mods.begin(); it != mods.end(); it++) {
            if (0 < ((it -> second) -> pqi) -> tick()) {
#ifdef DEBUG_TICK
                std::cerr << "pqihandler::tick() moreToTick from mod()" << std::endl;
#endif
                moreToTick = 1;
            }
        }
        // get the items, and queue them correctly
        if (0 < locked_GetItems()) {
#ifdef DEBUG_TICK
            std::cerr << "pqihandler::tick() moreToTick from GetItems()" << std::endl;
#endif
            moreToTick = 1;
        }
    } /****** UNLOCK ******/

    UpdateRates();
    return moreToTick;
}


int pqihandler::status() {
    std::map<std::string, SearchModule *>::iterator it;
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

    {
        // for output
        std::ostringstream out;
        out  << "pqihandler::status() Active Modules:" << std::endl;

        // display all interfaces...
        for (it = mods.begin(); it != mods.end(); it++) {
            out << "\tModule [" << it -> first << "] Pointer <";
            out << (void *) ((it -> second) -> pqi) << ">" << std::endl;
        }

        pqioutput(PQL_DEBUG_BASIC, pqihandlerzone, out.str().c_str());

    } // end of output.


    // status all interfaces...
    for (it = mods.begin(); it != mods.end(); it++) {
        ((it -> second) -> pqi) -> status();
    }
    return 1;
}



bool    pqihandler::AddSearchModule(SearchModule *mod) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
    // if peerid used -> error.
    std::map<std::string, SearchModule *>::iterator it;
    if (mod->cert_id != mod->pqi->PeerId()) {
        // ERROR!
        std::ostringstream out;
        out << "ERROR cert_id != PeerId!" << std::endl;
        pqioutput(PQL_ALERT, pqihandlerzone, out.str().c_str());
        return false;
    }

    if (mod->cert_id == "") {
        // ERROR!
        std::ostringstream out;
        out << "ERROR cert_id == NULL" << std::endl;
        pqioutput(PQL_ALERT, pqihandlerzone, out.str().c_str());
        return false;
    }

    if (mods.find(mod->cert_id) != mods.end()) {
        // ERROR!
        std::ostringstream out;
        out << "ERROR Module already exists!" << std::endl;
        pqioutput(PQL_ALERT, pqihandlerzone, out.str().c_str());
        return false;
    }

    // check security.
    if (mod -> sp == NULL) {
        // create policy.
        mod -> sp = secpolicy_create();
    }

    // limit to what global security allows.
    secpolicy_limit(globsec, mod -> sp);

    // store.
    mods[mod->cert_id] = mod;
    return true;
}

bool    pqihandler::RemoveSearchModule(SearchModule *mod) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
    std::map<std::string, SearchModule *>::iterator it;
    for (it = mods.begin(); it != mods.end(); it++) {
        if (mod == it -> second) {
            mods.erase(it);
            return true;
        }
    }
    return false;
}

// generalised output
int pqihandler::HandleNetItem(NetItem *item) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

    std::map<std::string, SearchModule *>::iterator it;
    pqioutput(PQL_DEBUG_BASIC, pqihandlerzone,
              "pqihandler::HandleNetItem()");

    // find module.
    if ((it = mods.find(item->PeerId())) == mods.end()) {
        std::ostringstream out;
        out << "pqihandler::HandleNetItem() Invalid chan!";
        pqioutput(PQL_DEBUG_BASIC, pqihandlerzone, out.str().c_str());

        delete item;
        return -1;
    }

    // check security... is output allowed.
    if (0 < secpolicy_check((it -> second) -> sp, 0, PQI_OUTGOING)) {
        std::ostringstream out;
        out << "pqihandler::HandleNetItem() sending to chan:";
        out << it -> first << std::endl;
        pqioutput(PQL_DEBUG_BASIC, pqihandlerzone, out.str().c_str());

        // if yes send on item.
        ((it -> second) -> pqi) -> SendItem(item);
        return 1;
    } else {
        std::ostringstream out;
        out << "pqihandler::HandleNetItem()";
        out << " Sec not approved";
        pqioutput(PQL_DEBUG_BASIC, pqihandlerzone, out.str().c_str());

        delete item;
        return -1;
    }
}

int     pqihandler::SendFileRequest(FileRequest *ns) {
    return HandleNetItem(ns);
}

int     pqihandler::SendFileData(FileData *ns) {
    return HandleNetItem(ns);
}

int     pqihandler::SendRawItem(RawItem *ns) {
    pqioutput(PQL_DEBUG_BASIC, pqihandlerzone,
              "pqihandler::SendRawItem()");
    return HandleNetItem(ns);
}


// inputs. This is a very basic
// system that is completely biased and slow...
// someone please fix.

int pqihandler::locked_GetItems() {
    std::map<std::string, SearchModule *>::iterator it;

    NetItem *item;
    int count = 0;

    // loop through modules....
    for (it = mods.begin(); it != mods.end(); it++) {
        SearchModule *mod = (it -> second);

        // check security... is output allowed.
        if (0 < secpolicy_check((it -> second) -> sp,
                                0, PQI_INCOMING)) { // PQI_ITEM_TYPE_ITEM, PQI_INCOMING))
            // if yes... attempt to read.
            while ((item = (mod -> pqi) -> GetItem()) != NULL) {
#ifdef NETITEM_DEBUG
                std::ostringstream out;
                out << "pqihandler::GetItems() Incoming Item ";
                out << " from: " << mod -> pqi << std::endl;
                item -> print(out);

                pqioutput(PQL_DEBUG_BASIC,
                          pqihandlerzone, out.str());
#endif

                if (item->PeerId() != (mod->pqi)->PeerId()) {
                    /* ERROR */
                    pqioutput(PQL_ALERT,
                              pqihandlerzone, "ERROR PeerIds dont match!");
                    item->PeerId(mod->pqi->PeerId());
                }

                locked_SortnStoreItem(item);
                count++;
            }
        } else {
            // not allowed to recieve from here....
            while ((item = (mod -> pqi) -> GetItem()) != NULL) {
                std::ostringstream out;
                out << "pqihandler::GetItems() Incoming Item ";
                out << " from: " << mod -> pqi << std::endl;
                item -> print(out);
                out << std::endl;
                out << "Item Not Allowed (Sec Pol). deleting!";
                out << std::endl;

                pqioutput(PQL_DEBUG_BASIC,
                          pqihandlerzone, out.str().c_str());

                delete item;
            }
        }
    }
    return count;
}




void pqihandler::locked_SortnStoreItem(NetItem *item) {
    /* get class type / subtype out of the item */
    uint8_t vers    = item -> PacketVersion();
    uint8_t cls     = item -> PacketClass();
    uint8_t type    = item -> PacketType();
    uint8_t subtype = item -> PacketSubType();

    /* whole Version reserved for SERVICES/CACHES */
    if (vers == PKT_VERSION_SERVICE) {
        pqioutput(PQL_DEBUG_BASIC, pqihandlerzone,
                  "SortnStore -> Service");
        in_service.push_back(item);
        item = NULL;
        return;
    }

    if (vers != PKT_VERSION1) {
        pqioutput(PQL_DEBUG_BASIC, pqihandlerzone,
                  "SortnStore -> Invalid VERSION! Deleting!");
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
                            pqioutput(PQL_DEBUG_BASIC, pqihandlerzone,
                                      "SortnStore -> File Request");
                            in_request.push_back(item);
                            item = NULL;
                            break;

                        case PKT_SUBTYPE_FI_DATA:
                            pqioutput(PQL_DEBUG_BASIC, pqihandlerzone,
                                      "SortnStore -> File Data");
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
            pqioutput(PQL_DEBUG_BASIC, pqihandlerzone,
                      "SortnStore -> Unknown");
            break;

    }

    if (item) {
        pqioutput(PQL_DEBUG_BASIC, pqihandlerzone,
                  "SortnStore -> Deleting Unsorted Item");
        delete item;
    }

    return;
}

FileRequest *pqihandler::GetFileRequest() {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

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
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

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
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

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
void pqihandler::UpdateRates() {
    std::map<std::string, SearchModule *>::iterator it;
    int num_sm = mods.size();

    //Total bandwidth available
    float avail_in = getMaxRate(true);
    float avail_out = getMaxRate(false);

    //Max bandwidth per module
    float indiv_in = getMaxIndivRate(true);
    float indiv_out = getMaxIndivRate(false);

    //The average bandwidth available if all bandwidth was equally shared among modules
    float avg_rate_in = avail_in/num_sm;
    float avg_rate_out = avail_out/num_sm;

    //Total of bandwidth being used
    float used_bw_in = 0;
    float used_bw_out = 0;

    //The total amount of all module's bandwidth use over the avg_rate
    float extra_bw_in = 0;
    float extra_bw_out = 0;

    //The number of transfers that are bumping off the individual rate limiter and over the avg_rate
    //Used so we know when not to bother trying to increase the speed anymore (when all modules are maxed)
    int maxed_in = 0;
    int maxed_out = 0;

    /* Lock once rates have been retrieved */
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

    //Loop through modules to gather data on speeds
    for (it = mods.begin(); it != mods.end(); it++) {
        SearchModule *mod = (it -> second);
        //Actual rates being used by this module
        float crate_in = mod -> pqi -> getRate(true);
        float crate_out = mod -> pqi -> getRate(false);

        used_bw_in += crate_in;
        used_bw_out += crate_out;

        if (crate_in > avg_rate_in) {
            if (mod -> pqi -> getMaxRate(true) == indiv_in) {
                maxed_in++;
            }
            extra_bw_in +=  crate_in - avg_rate_in;
        }
        if (crate_out > avg_rate_out) {
            if (mod -> pqi -> getMaxRate(false) == indiv_out) {
                maxed_out++;
            }
            extra_bw_out +=  crate_out - avg_rate_out;
        }
    }

    locked_StoreCurrentRates(used_bw_in, used_bw_out);

    //If there is no limit, just set all modules to individual max
    if (avail_in == 0){
        for (it = mods.begin(); it != mods.end(); it++) {
            it -> second -> pqi -> setMaxRate(true, indiv_in);
        }
    //If already over the cap, target those that are above average to be slowed down
    } else if (used_bw_in > avail_in) {
        float fchg = (used_bw_in - avail_in) / (float) extra_bw_in;
        for (it = mods.begin(); it != mods.end(); it++) {
            SearchModule *mod = (it -> second);
            float crate_in = mod -> pqi -> getRate(true);
            float new_max = avg_rate_in;
            if (crate_in > avg_rate_in) {
                new_max = avg_rate_in + (1 - fchg) *
                          (crate_in - avg_rate_in);
            }
            if (new_max > indiv_in) {
                new_max = indiv_in;
            }
            mod -> pqi -> setMaxRate(true, new_max);
        }
    //If not maxxed already and using less than 95%, increase limit
    } else if ((maxed_in != num_sm) && (used_bw_in < 0.95 * avail_in)) {
        float fchg = (avail_in - used_bw_in) / avail_in;
        for (it = mods.begin(); it != mods.end(); it++) {
            SearchModule *mod = (it -> second);
            float crate_in = mod -> pqi -> getRate(true);
            float max_in = mod -> pqi -> getMaxRate(true);

            if (max_in == indiv_in) {
                // do nothing...
            } else {
                float new_max = max_in;
                if (max_in < avg_rate_in) {
                    new_max = avg_rate_in * (1 + fchg);
                } else if (crate_in > 0.5 * max_in) {
                    new_max =  max_in * (1 + fchg);
                }
                if (new_max > indiv_in) {
                    new_max = indiv_in;
                }
                mod -> pqi -> setMaxRate(true, new_max);
            }
        }

    }

    //If there is no limit, just set all modules to individual max
    if (avail_out == 0){
        for (it = mods.begin(); it != mods.end(); it++) {
            it -> second -> pqi -> setMaxRate(false, indiv_out);
        }
    //If already over the cap, target those that are above average to be slowed down
    } else if (used_bw_out > avail_out) {
        float fchg = (used_bw_out - avail_out) / (float) extra_bw_out;
        for (it = mods.begin(); it != mods.end(); it++) {
            SearchModule *mod = (it -> second);
            float crate_out = mod -> pqi -> getRate(false);
            float new_max = avg_rate_out;
            if (crate_out > avg_rate_out) {
                new_max = avg_rate_out + (1 - fchg) *
                          (crate_out - avg_rate_out);
            }
            if (new_max > indiv_out) {
                new_max = indiv_out;
            }
            mod -> pqi -> setMaxRate(false, new_max);
        }
    //If not maxxed already and using less than 95%, increase limit
    } else if ((maxed_out != num_sm) && (used_bw_out < 0.95 * avail_out)) {
        float fchg = (avail_out - used_bw_out) / avail_out;
        for (it = mods.begin(); it != mods.end(); it++) {
            SearchModule *mod = (it -> second);
            float crate_out = mod -> pqi -> getRate(false);
            float max_out = mod -> pqi -> getMaxRate(false);

            if (max_out == indiv_out) {
                // do nothing...
            } else {
                float new_max = max_out;
                if (max_out < avg_rate_out) {
                    new_max = avg_rate_out * (1 + fchg);
                } else if (crate_out > 0.5 * max_out) {
                    new_max =  max_out * (1 + fchg);
                }
                if (new_max > indiv_out) {
                    new_max = indiv_out;
                }
                mod -> pqi -> setMaxRate(false, new_max);
            }
        }
    }
}

void    pqihandler::getCurrentRates(float &in, float &out) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/

    in = rateTotal_in;
    out = rateTotal_out;
}

void    pqihandler::locked_StoreCurrentRates(float in, float out) {
    rateTotal_in = in;
    rateTotal_out = out;
}


