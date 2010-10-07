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

#ifndef PQI_STRINGS_H
#define PQI_STRINGS_H

#include <string>
#include <list>

class Person;
class cert;

#include <openssl/ssl.h>

std::string get_cert_country(cert *c);
std::string get_cert_loc(cert *c);
std::string get_cert_org(cert *c);
std::string get_cert_name(cert *c);

std::string get_status_string(int status);
std::string get_autoconnect_string(Person *p);
std::string get_server_string(Person *p);
std::string get_trust_string(Person *p);
std::string get_timeperiod_string(int secs);

std::string get_cert_info(cert *c);
std::string get_neighbour_info(cert *c);

int get_lastconnecttime(Person *p);
std::string get_lastconnecttime_string(Person *p);

#define TIME_FORMAT_BRIEF       0x001
#define TIME_FORMAT_LONG        0x002
#define TIME_FORMAT_NORMAL      0x003
#define TIME_FORMAT_OLDVAGUE        0x004
#define TIME_FORMAT_OLDVAGUE_NOW    0x005
#define TIME_FORMAT_OLDVAGUE_WEEK   0x006
#define TIME_FORMAT_OLDVAGUE_OLD    0x007

std::string timeFormat(int epoch, int format);

#endif
