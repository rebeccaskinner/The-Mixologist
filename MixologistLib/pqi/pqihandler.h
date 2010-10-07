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
#include "pqi/pqisecurity.h"

#include "util/threads.h"

#include <map>
#include <list>

class SearchModule {
public:
    std::string cert_id;
    PQInterface *pqi;
    SecurityPolicy *sp;
};

/*
Combines a number of PQInterface (i.e. pqipersons, each of which corresponds to a peer)
into a manageable aggregate.
Manages total bandwidth limits and enforces them on the PQInterfaces.
The PQInterfaces are stored as SearchModule, which are a pairing of
the PQInterface and a SecurityPolicy.
*/
class pqihandler: public P3Interface {
public:
    pqihandler(SecurityPolicy *Global);
    bool    AddSearchModule(SearchModule *mod);
    bool    RemoveSearchModule(SearchModule *mod);

    // file i/o
    virtual int     SendFileRequest(FileRequest *ns);
    virtual int     SendFileData(FileData *ns);
    virtual FileRequest    *GetFileRequest();
    virtual FileData   *GetFileData();

    // Rest of P3Interface
    virtual int     tick();
    virtual int     status();

    // Service Data Interface
    virtual int     SendRawItem(RawItem *);
    virtual RawItem *GetRawItem();

    // rate control.
    void    setMaxIndivRate(bool in, float val);
    float   getMaxIndivRate(bool in);
    void    setMaxRate(bool in, float val);
    float   getMaxRate(bool in);

    void    getCurrentRates(float &in, float &out);


protected:
    /* check to be overloaded by those that can
     * generates warnings otherwise
     */

    int HandleNetItem(NetItem *ns);

    int locked_GetItems();
    void    locked_SortnStoreItem(NetItem *item);

    MixMutex coreMtx; /* MUTEX */

    std::map<std::string, SearchModule *> mods; //cert_ids / Searchmodules(container for a PQInterface and SecurityPolicy
    SecurityPolicy *globsec;

    // Temporary storage...
    std::list<NetItem *> in_result, in_search,
        in_request, in_data, in_service;

private:

    // Called by tick(), steps through all mods, and sets their transfer rate caps based on settings and overall activity.
    void UpdateRates();
    void    locked_StoreCurrentRates(float in, float out);

    float rateIndiv_in;
    float rateIndiv_out;
    float rateMax_in;
    float rateMax_out;

    float rateTotal_in;
    float rateTotal_out;
};

inline void pqihandler::setMaxIndivRate(bool in, float val) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
    if (in)
        rateIndiv_in = val;
    else
        rateIndiv_out = val;
    return;
}

inline float pqihandler::getMaxIndivRate(bool in) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
    if (in)
        return rateIndiv_in;
    else
        return rateIndiv_out;
}

inline void pqihandler::setMaxRate(bool in, float val) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
    if (in)
        rateMax_in = val;
    else
        rateMax_out = val;
    return;
}

inline float pqihandler::getMaxRate(bool in) {
    MixStackMutex stack(coreMtx); /**************** LOCKED MUTEX ****************/
    if (in)
        return rateMax_in;
    else
        return rateMax_out;
}

#endif // MRK_PQI_HANDLER_HEADER
