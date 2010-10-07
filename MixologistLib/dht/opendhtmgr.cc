/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-8, Robert Fernie
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

#include "dht/opendhtmgr.h"
#include "dht/opendht.h"
#include "util/threads.h" /* for pthreads headers */


class dhtSearchData {
public:
    OpenDHTMgr *mgr;
    DHTClient *client;
    std::string key;
};


class dhtPublishData {
public:
    OpenDHTMgr *mgr;
    DHTClient *client;
    std::string key;
    std::string value;
    uint32_t    ttl;
};

/* Thread routines */

extern "C" void *doDhtPublish(void *p) {
#ifdef OPENDHT_DEBUG
    std::cerr << "in doDhtPublish(void* p)" << std::endl ;
#endif
    dhtPublishData *data = (dhtPublishData *) p;
    if (data == NULL) {
        pthread_exit(NULL);
        return NULL;
    }

    /* publish it! */
    if (data->mgr != NULL && data->client != NULL)
        data->client->publishKey(data->key, data->value, data->ttl);

    delete data;
    pthread_exit(NULL);

    return NULL;
}


extern "C" void *doDhtSearch(void *p) {
    dhtSearchData *data = (dhtSearchData *) p;
    if ((!data) || (!data->mgr) || (!data->client)) {
        pthread_exit(NULL);

        return NULL;
    }

    /* search it! */
    std::list<std::string> values;

    if (data->client->searchKey(data->key, values)) {
        /* callback */
        std::list<std::string>::iterator it;
        for (it = values.begin(); it != values.end(); it++) {
            data->mgr->resultDHT(data->key, *it);
        }
    }

    delete data;
    pthread_exit(NULL);

    return NULL;
}




OpenDHTMgr::OpenDHTMgr(std::string ownId, pqiConnectCb *cb, QString configdir)
    :p3DhtMgr(ownId, cb), mConfigDir(configdir) {
    return;
}


/********** OVERLOADED FROM p3DhtMgr ***************/
bool    OpenDHTMgr::dhtInit() {
    QString configpath = mConfigDir;

    /* load up DHT gateways */
    mClient = new OpenDHTClient();
    //mClient = new DHTClientDummy();

    QString filename = configpath;
    if (configpath.size() > 0) {
        filename += "/";
    }
    filename += "ODHTservers.txt";

    /* check file date first */
    if (mClient -> checkServerFile(filename)) {
        return mClient -> loadServers(filename);
    } else if (!mClient -> loadServersFromWeb(filename)) {
        return mClient -> loadServers(filename);
    }
    return true;
}

bool    OpenDHTMgr::dhtShutdown() {
    /* do nothing */
    if (mClient) {
        delete mClient;
        mClient = NULL;
        return true;
    }

    return false;
}

bool    OpenDHTMgr::dhtActive() {
    /* do nothing */
    if ((mClient) && (mClient -> dhtActive())) {
        return true;
    }
    return false;
}

int     OpenDHTMgr::status(std::ostream &) {
    /* do nothing */
    return 1;
}


/* Blocking calls (only from thread) */
bool OpenDHTMgr::publishDHT(std::string key, std::string value, uint32_t ttl) {
    /* launch a publishThread */
    pthread_t tid;

#ifdef OPENDHT_DEBUG
    std::cerr << "in publishDHT(.......)" << std::endl ;
#endif

    dhtPublishData *pub = new dhtPublishData;
    pub->mgr = this;
    pub->client = mClient;
    pub->key = key;
    pub->value = value;
    pub->ttl = ttl;

    void *data = (void *) pub;
    pthread_create(&tid, 0, &doDhtPublish, data);
    pthread_detach(tid); /* so memory is reclaimed in linux */

    return true;
}

bool OpenDHTMgr::searchDHT(std::string key) {
    /* launch a publishThread */
    pthread_t tid;

    dhtSearchData *dht = new dhtSearchData;
    dht->mgr = this;
    dht->client = mClient;
    dht->key = key;

    void *data = (void *) dht;
    pthread_create(&tid, 0, &doDhtSearch, data);
    pthread_detach(tid); /* so memory is reclaimed in linux */

    return true;
}

/********** OVERLOADED FROM p3DhtMgr ***************/





