/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-8, Robert Fernie
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

#ifndef MRK_P3_DHT_MANAGER_HEADER
#define MRK_P3_DHT_MANAGER_HEADER

/* Interface class for DHT data */

#include <string>
#include <map>
#include "pqi/pqinetwork.h"
#include "dht/opendht.h"

#include "pqi/pqimonitor.h"

#include <QThread>
#include <QMutex>

/* All other #defs are in .cc */
#define DHT_ADDR_INVALID        0xff
#define DHT_ADDR_TCP            0x01
#define DHT_ADDR_UDP            0x02


/* for DHT peer STATE */
#define DHT_PEER_OFF            0 /* Peer is not to be contacted with notifyPeer at least */
#define DHT_PEER_INIT           1
#define DHT_PEER_SEARCH         2
#define DHT_PEER_FOUND          3

/* for DHT peer STATE (ownEntry) */
#define DHT_PEER_ADDR_KNOWN     4
#define DHT_PEER_PUBLISHED      5

/* Interface with Real DHT Implementation */
#define DHT_MODE_SEARCH         1
#define DHT_MODE_PUBLISH        1
#define DHT_MODE_NOTIFY         2
#define DHT_MODE_BOOTSTRAP  3


/* TIMEOUTS: Reference Values are set here... */

#define DHT_SEARCH_PERIOD       1800 /* PeerKeys: if we haven't found them: 30 min */
#define DHT_CHECK_PERIOD        1800 /* PeerKeys: re-lookup peer: 30 min */
#define DHT_PUBLISH_PERIOD      1800 /* OwnKey: 30 min */
#define DHT_NOTIFY_PERIOD       300  /* 5 min - Notify Check period */

/* TTLs for DHTs posts */
#define DHT_TTL_PUBLISH         (DHT_PUBLISH_PERIOD + 120)  // for a little overlap.
#define DHT_TTL_NOTIFY          (DHT_NOTIFY_PERIOD  + 60)   // for time to find it...
#define DHT_TTL_BOOTSTRAP       (DHT_PUBLISH_PERIOD)        // To start with.

class dhtPeerEntry {
public:
    dhtPeerEntry();

    std::string id;
    uint32_t state;
    time_t lastTS;

    uint32_t notifyPending;
    time_t   notifyTS;

    struct sockaddr_in laddr, raddr;
    uint32_t type;  /* ADDR_TYPE as defined above */

    std::string hash1; /* SHA1 Hash of id */
    std::string hash2; /* SHA1 Hash of reverse Id */
};

class p3DhtMgr: public QThread {
    /*
     */
public:
    p3DhtMgr(std::string id, pqiConnectCb *cb);

    /********** External DHT Interface ************************
     * These Functions are the external interface
     * for the DHT, and must be non-blocking and return quickly
     */

    /* OVERLOADED From pqiNetAssistConnect. */

    virtual void enable(bool on);

    virtual bool getEnabled(); /* on */
    virtual bool getActive();  /* actually working */

    virtual void    setBootstrapAllowed(bool on);
    virtual bool    getBootstrapAllowed();

    /* set key data */
    virtual bool    setExternalInterface(struct sockaddr_in laddr,
                                         struct sockaddr_in raddr, uint32_t type);

    /* add / remove peers */
    virtual bool    findPeer(std::string id);
    virtual bool    dropPeer(std::string id);

    /* post DHT key saying we should connect (callback when done) */
    virtual bool    notifyPeer(std::string id);

    /* extract current peer status */
    virtual bool    getPeerStatus(std::string id,
                                  struct sockaddr_in &laddr, struct sockaddr_in &raddr,
                                  uint32_t &type, uint32_t &mode);

    /* stun */
    virtual bool    enableStun(bool on);
    virtual bool    addStun(std::string id);
    //doneStun();

    /********** Higher Level DHT Work Functions ************************
     * These functions translate from the strings/addresss to
     * key/value pairs.
     */
public:

    /* results from DHT proper */
    virtual bool dhtResultNotify(std::string id);
    virtual bool dhtResultSearch(std::string id,
                                 struct sockaddr_in &laddr, struct sockaddr_in &raddr,
                                 uint32_t type, std::string sign);

    virtual bool dhtResultBootstrap(std::string idhash);

protected:

    /* can block briefly (called only from thread) */
    virtual bool dhtPublish(std::string id,
                            struct sockaddr_in &laddr,
                            struct sockaddr_in &raddr,
                            uint32_t type, std::string sign);

    /* publish notification (publish Our Id)
     * We publish the connection attempt with key equal to peers hash,
     * using our own hash as the value. */
    virtual bool dhtNotify(std::string peerid, std::string ownId,
                           std::string sign);

    virtual bool dhtSearch(std::string id, uint32_t mode);

    virtual bool dhtBootstrap(std::string storehash, std::string ownIdHash,
                              std::string sign); /* to publish bootstrap */



    /********** Actual DHT Work Functions ************************
     * These involve a very simple LOW-LEVEL interface ...
     *
     * publish
     * search
     * result
     *
     */

public:

    /* Feedback callback (handled here) */
    virtual bool resultDHT(std::string key, std::string value);

protected:

    /* Creates the openDHTClient and has it read in the openDHT server list. */
    virtual bool    dhtInit();
    /* Kills the openDHTClient. */
    virtual bool    dhtShutdown();
    /* True if connected to the open DHT server network. */
    virtual bool    dhtActive();
    virtual int     status(std::ostream &out);

    /* Publishes a key value paid into the open DHT network. */
    virtual bool publishDHT(std::string key, std::string value, uint32_t ttl);

    /* Retrieves the values associated with a key from the open DHT network. */
    virtual bool searchDHT(std::string key);



    /********** Internal DHT Threading ************************
     *
     */

public:

    virtual void run();

private:

    /* search scheduling */
    void    checkDHTStatus();
    int     checkStunState();
    int     checkStunState_Active(); /* when in active state */
    int     doStun();
    int     checkPeerDHTKeys();
    int     checkOwnDHTKeys();
    int     checkNotifyDHT();

    void    clearDhtData();

    std::string  mPeerId;
    pqiConnectCb *mConnCb;

    /* IP Bootstrap */
    bool    getDhtBootstrapList();
    std::string BootstrapId(uint32_t bin);
    std::string randomBootstrapId();

    /* other feedback through callback */
    // use pqiNetAssistConnect.. version pqiConnectCb *connCb;

    /* protected by Mutex */
    mutable QMutex dhtMtx;

    bool mDhtOn; /* User desired state */
    /* True when there are orders from higher-up for the DHT manager to do something. */
    bool mDhtModifications;

    dhtPeerEntry ownEntry;
    time_t ownNotifyTS;
    std::map<std::string, dhtPeerEntry> peers;

    /* List of friend ids that have been added. */
    std::list<std::string> stunIds;

    /* Whether we need to send out stun messages. */
    bool mStunRequired;

    /* The current state of the mDht. */
    uint32_t mDhtState;

    time_t   mDhtActiveTS;

    bool   mBootstrapAllowed;
    time_t mLastBootstrapListTS;

    OpenDHTClient *mClient;
};


#endif // MRK_P3_DHT_MANAGER_HEADER


