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

#ifndef PQI_TOP_HEADER
#define PQI_TOP_HEADER

/* This just includes the standard headers required.
 */


#include "pqi/pqi_base.h"
#include "pqi/pqinetwork.h"
#include "serialiser/serial.h"
#include "serialiser/baseitems.h"

#include <iostream>
#include <functional>
#include <algorithm>

/********************** SEARCH INTERFACE ***************************/
// this is an interface.... so should be
// classified as virtual   = 0;

class SearchInterface {
public:
    SearchInterface()  {
        return;
    }

    virtual ~SearchInterface() {
        return;
    }

    // FileTransfer.
    virtual FileRequest *GetFileRequest() = 0;
    virtual int SendFileRequest(FileRequest *) = 0;

    virtual FileData *GetFileData() = 0;
    virtual int SendFileData(FileData *) = 0;

};

class P3Interface: public SearchInterface {
public:
    P3Interface() {
        return;
    }
    virtual ~P3Interface() {
        return;
    }

    virtual int tick() {
        return 1;
    }

    virtual int SendRawItem(RawItem *) = 0;
    virtual RawItem *GetRawItem() = 0;

};

#endif // PQI_TOP_HEADER

