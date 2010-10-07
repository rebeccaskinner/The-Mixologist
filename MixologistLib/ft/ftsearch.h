/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2008, Robert Fernie
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

#ifndef FT_SEARCH_HEADER
#define FT_SEARCH_HEADER

/*
 * ftSearch
 *
 * This is a generic search interface - used by ft* to find files.
 * The derived class will search for Caches/Local/ItemList/Remote entries.
 *
 */

#include "interface/files.h" // includes interface/types.h too!

class ftSearch {

public:

    ftSearch() {
        return;
    }
    virtual ~ftSearch() {
        return;
    }
    virtual bool    search(std::string hash, uint64_t size, uint32_t hintflags, FileInfo &info) const = 0;

};

#endif
