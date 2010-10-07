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

#ifndef OPENDHT_STRING_CODE_H
#define OPENDHT_STRING_CODE_H

#include <inttypes.h>
#include <string>

std::string createHttpHeader(std::string host, uint16_t port,
                             std::string agent, uint32_t length);

std::string createHttpHeaderGET(std::string host, uint16_t port,
                                std::string page, std::string agent, uint32_t length);

std::string createOpenDHT_put(std::string key, std::string value,
                              uint32_t ttl, std::string client);

std::string createOpenDHT_get(std::string key,
                              uint32_t maxresponse, std::string client);

#endif

