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
        /* Used for files shared for transfer with all friends. */
        SEARCH_RESULT_FOUND_SHARED_FILE,
        /* Used for files that are not to be shared with all friends, but sharing with this particular friend is permitted. */
        SEARCH_RESULT_FOUND_SHARED_FILE_LIMITED,
        /* Used for internal Mixologist files that are not user shares. */
        SEARCH_RESULT_FOUND_INTERNAL_FILE
    };

    virtual ~ftFileMethod() {}

    /* Returns true if it is able to find a file with matching hash and size.
       hintflags is a combination of the flags from files.h to indicate where to search.
       librarymixer_id is the friend that is asking for it, in case it is relevant for limited shares.
       If a file is found, populates path.
       In practice, called by ftDataDemultiplex when it is looking for a file to send. */
    virtual ftFileMethod::searchResult search(const QString &hash, qlonglong size, uint32_t hintflags, unsigned int librarymixer_id, QString &path) = 0;

signals:
    /* Emitted when a shared file becomes no longer available on disk. */
    void fileNoLongerAvailable(QString hash, qulonglong size);
};

#endif
