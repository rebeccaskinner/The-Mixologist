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

/* This is the interface implemented by all classes that will be making files available. */

#include "interface/files.h" // includes interface/types.h too!
#include "ft/ftfilewatcher.h"
#include <QString>

class ftFileMethod: public QObject {
    Q_OBJECT

public:
    enum searchResult {
        SEARCH_RESULT_NOT_FOUND,
        SEARCH_RESULT_FOUND_SHARED_FILE, //Used for files shared for transfer with friends
        SEARCH_RESULT_FOUND_INTERNAL_FILE //Used for internal Mixologist files that are not user shares
    };

    virtual ~ftFileMethod() {}

    /* Returns true if it is able to find a file with matching hash and size.
       If a file is found, populates path. */
    virtual ftFileMethod::searchResult search(const QString &hash, qlonglong size, uint32_t hintflags, QString &path) = 0;

signals:
    void fileNoLongerAvailable(QString hash, qulonglong size);
};

#endif
