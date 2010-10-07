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

#include <string>
#include <sstream>

#include "opendhtstr.h"
#include "b64.h"

std::string createHttpHeader(std::string host, uint16_t port, std::string agent, uint32_t length) {
    std::ostringstream req;

    req << "POST / HTTP/1.0\r\n";
    req << "Host: " << host << ":" << port << "\r\n";
    req << "User-Agent: " << agent << "\r\n";
    req << "Content-Type: text/xml\r\n";
    req << "Content-Length: " << length << "\r\n";
    req << "\r\n";

    return req.str();
};

std::string createHttpHeaderGET(std::string host, uint16_t port, std::string page, std::string agent, uint32_t) {
    std::ostringstream req;

    req << "GET /" << page << " HTTP/1.0\r\n";
    req << "Host: " << host << ":" << port << "\r\n";
    req << "User-Agent: " << agent << "\r\n";
    //req << "Content-Type: text/xml\r\n";
    //req << "Content-Length: " << length << "\r\n";
    req << "\r\n";

    return req.str();
};

std::string createOpenDHT_put(std::string key, std::string value, uint32_t ttl, std::string client) {
    std::ostringstream req;

    req << "<?xml version=\"1.0\"?>" << std::endl;
    req << "<methodCall>" << std::endl;
    req << "\t<methodName>put</methodName>" << std::endl;
    req << "\t<params>" << std::endl;

    req << "\t\t<param><value><base64>";
    req << convertToBase64(key);
    req << "</base64></value></param>" << std::endl;

    req << "\t\t<param><value><base64>";
    req << convertToBase64(value);
    req << "</base64></value></param>" << std::endl;

    req << "\t\t<param><value><int>";
    req << ttl;
    req << "</int></value></param>" << std::endl;

    req << "\t\t<param><value><string>";
    req << client;
    req << "</string></value></param>" << std::endl;

    req << "\t</params>" << std::endl;
    req << "</methodCall>" << std::endl;

    return req.str();
}


std::string createOpenDHT_get(std::string key, uint32_t maxresponse, std::string client) {
    std::ostringstream req;

    req << "<?xml version=\"1.0\"?>" << std::endl;
    req << "<methodCall>" << std::endl;
    req << "\t<methodName>get</methodName>" << std::endl;
    req << "\t<params>" << std::endl;

    /* key */
    req << "\t\t<param><value><base64>";
    req << convertToBase64(key);
    req << "</base64></value></param>" << std::endl;

    /* max response */
    req << "\t\t<param><value><int>";
    req << maxresponse;
    req << "</int></value></param>" << std::endl;

    /* placemark (NULL) */
    req << "\t\t<param><value><base64>";
    req << "</base64></value></param>" << std::endl;

    req << "\t\t<param><value><string>";
    req << client;
    req << "</string></value></param>" << std::endl;

    req << "\t</params>" << std::endl;
    req << "</methodCall>" << std::endl;

    return req.str();
}




