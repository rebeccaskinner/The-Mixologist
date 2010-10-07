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

#ifndef FT_FILE_SEARCH_HEADER
#define FT_FILE_SEARCH_HEADER

/*
 * ftFileSearch
 *
 * This implements an array of ftSearch Interfaces.
 * Each search interface that is added can be searched using the search method.
 *
 */

#include <vector>

#include "ft/ftsearch.h"

class ftFileSearch: public ftSearch {

public:

    ftFileSearch();

    bool    addSearchMode(ftSearch *search, uint32_t hintflags);
    virtual bool    search(std::string hash, uint64_t size, uint32_t hintflags, FileInfo &info) const;

private:

    // should have a mutex to protect vector....
    // but not really necessary as it is const most of the time.

    std::vector<ftSearch *> mSearchs;
};


#endif
