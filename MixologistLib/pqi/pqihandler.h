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

#ifndef MRK_PQI_HANDLER_HEADER
#define MRK_PQI_HANDLER_HEADER

#include "pqi/pqi.h"

#include "util/threads.h"

#include <map>
#include <list>

/*
Aggregates the PQInterfaces (i.e. pqipersons, each of which corresponds to a peer/pqiloopback, for self),
creating a single interface from which to call to send or get NetItems.
Manages total bandwidth limits and enforces them on the PQInterfaces.
In practice, this is used in pqipersongrp as a class is extends.
*/
class pqihandler: public P3Interface {
public:
    pqihandler();
    bool AddPQI(PQInterface *mod);
    bool RemovePQI(PQInterface *mod);

    // file i/o
    virtual int SendFileRequest(FileRequest *ns);
    virtual int SendFileData(FileData *ns);
    virtual FileRequest *GetFileRequest();
    virtual FileData *GetFileData();

    // Rest of P3Interface
    /* In practice, this tick is called from pqipersongrp, which implemented pqihandler */
    virtual int tick();
    virtual int status();

    // Service Data Interface
    virtual int  SendRawItem(RawItem *);
    virtual RawItem *GetRawItem();

    // rate control.
    void setMaxIndivRate(bool in, float val);
    float getMaxIndivRate(bool in);
    void setMaxRate(bool in, float val);
    float getMaxRate(bool in);

protected:
    /* check to be overloaded by those that can
     * generates warnings otherwise
     */

    //Called by the SendX functions to find the correct pqi and have it send the item
    int HandleNetItem(NetItem *ns);

    //Called from tick, steps through the pqis and takes all the incoming items off of them, and then calling SortnStoreItem on the items
    int locked_GetItems();
    //Called from locked_GetItems, takes an incoming item and puts it on the appropriate incoming queue (i.e. service, file request, file data)
    void locked_SortnStoreItem(NetItem *item);

    MixMutex coreMtx;

    //Where all the aggregated PQInterfaces are held
    std::map<std::string, PQInterface *> pqis; //cert_ids / PQInterface
    //Incoming queues
    std::list<NetItem *> in_request, in_data, in_service;

private:

    //Called by tick(), steps through all pqis, and sets their transfer rate caps based on settings and overall activity.
    //Attempts to apportion bandwidth fairly among pqis.
    void updateRateCaps();
    //Called by UpdateRateCaps to handle either the downloading or uploading side of the rate caps
    void setRateCaps(bool downloading, float total_max_rate, float indiv_max_rate, float shared_max_rate, float used_bw, float extra_bw, int maxed, int num_pqis);

    float maxIndivIn;
    float maxIndivOut;
    float maxTotalIn;
    float maxTotalOut;

};

inline void pqihandler::setMaxIndivRate(bool in, float val) {
    MixStackMutex stack(coreMtx);
    if (in) maxIndivIn = val;
    else maxIndivOut = val;
}

inline float pqihandler::getMaxIndivRate(bool in) {
    MixStackMutex stack(coreMtx);
    if (in) return maxIndivIn;
    else return maxIndivOut;
}

inline void pqihandler::setMaxRate(bool in, float val) {
    MixStackMutex stack(coreMtx);
    if (in) maxTotalIn = val;
    else maxTotalOut = val;
}

inline float pqihandler::getMaxRate(bool in) {
    MixStackMutex stack(coreMtx);
    if (in) return maxTotalIn;
    else return maxTotalOut;
}

#endif // MRK_PQI_HANDLER_HEADER
