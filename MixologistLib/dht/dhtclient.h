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


#ifndef GENERIC_DHT_CLIENT_H
#define GENERIC_DHT_CLIENT_H

#include <inttypes.h>
#include <string>
#include <list>
#include <QString>

class DHTClient {
public:

    /* initialise from file */
    virtual bool checkServerFile(QString filename) = 0;
    virtual bool loadServers(QString filename) = 0;
    virtual bool loadServersFromWeb(QString storename) = 0;
    virtual bool loadServers(std::istream &) = 0;

    /* check that its working */
    virtual bool dhtActive() = 0;

    /* publish / search */
    virtual bool publishKey(std::string key, std::string value, uint32_t ttl) = 0;
    virtual bool searchKey(std::string key, std::list<std::string> &values) = 0;

};


class DHTClientDummy: public DHTClient {
public:

    /* initialise from file */
    virtual bool checkServerFile(QString) {
        return false;
    }
    virtual bool loadServers(QString) {
        return true;
    }
    virtual bool loadServersFromWeb(QString) {
        return true;
    }
    virtual bool loadServers(std::istream &) {
        return true;
    }

    /* check that its working */
    virtual bool dhtActive() {
        return true;
    }

    /* publish / search */
    virtual bool publishKey(std::string, std::string, uint32_t ) {
        return true;
    }
    virtual bool searchKey(std::string , std::list<std::string> &)   {
        return true;
    }

};


#endif

