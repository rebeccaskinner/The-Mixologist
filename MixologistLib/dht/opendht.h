/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-2008, Robert Fernie
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

#ifndef OPEN_DHT_CLIENT_H
#define OPEN_DHT_CLIENT_H

#include "pqi/pqinetwork.h"

#include <inttypes.h>
#include <string>
#include <list>
#include <map>

#include <QString>
#include <QMutex>

class dhtServer {
public:

    std::string host;
    uint16_t    port;
    uint16_t    failed;
    time_t      ts;
    struct sockaddr_in addr;
};


/* Connects to openDHT servers, and stores and retrieves key value pairs from them. */
class OpenDHTClient {
public:

    /* Publishes a key value paid into the open DHT network. */
    bool publishKey(std::string key, std::string value, uint32_t ttl);

    /* Retrieves the values associated with a key from the open DHT network. */
    bool searchKey(std::string key, std::list<std::string> &values);

    /* Fns accessing data */
    /* Checks to see if the stored file is valid and fresh enough to use. */
    bool checkServerFile(QString filename);
    /* Reads in the istream from the file and passes it to loadServers. */
    bool loadServers(QString filename);
    /* Downloads the file and passes it to loadServers. */
    bool loadServersFromWeb(QString storefname);
    /* Reads in the DHT servers. */
    bool loadServers(std::istream &);

    /* True if connected to the open DHT server network. */
    bool dhtActive();

private:
    bool    getServer(std::string &host, uint16_t &port, struct sockaddr_in &addr);
    bool    setServerIp(std::string host, struct sockaddr_in addr);
    void    setServerFailed(std::string host);

private:

    /* generic send msg */
    bool openDHT_sendMessage(std::string msg, std::string &response);
    /* Connects to www.opendht.org to get the server list. */
    bool openDHT_getDHTList(std::string &response);

    mutable QMutex dhtMutex;

    /* Map by domain name of dhtSevers. */
    std::map<std::string, dhtServer> mServers;
    uint32_t mDHTFailCount;

};


#endif


