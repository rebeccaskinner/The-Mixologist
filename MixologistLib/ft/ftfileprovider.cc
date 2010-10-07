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

#include "ftfileprovider.h"

#include <util/dir.h>
#include <stdlib.h>
#include <stdio.h>
#include <interface/peers.h>

#include <QFile>

/*
 * #define DEBUG_FT_FILE_PROVIDER
 */

ftFileProvider::ftFileProvider(QString _path, uint64_t size, std::string
                               hash) : mSize(size), hash(hash), path(_path), file(NULL), transfer_rate(0), total_size(0) {
    lastTS = time(NULL) ;
    lastTS_t = lastTS ;
}

ftFileProvider::~ftFileProvider() {
    if (file != NULL) {
        file->close();
        file->deleteLater();
    }
}

void ftFileProvider::setPeerId(const std::string &id) {
    MixStackMutex stack(ftcMutex); /********** STACK LOCKED MTX ******/
    lastRequestor = id ;
}

std::string ftFileProvider::getHash() {
    MixStackMutex stack(ftcMutex); /********** STACK LOCKED MTX ******/
    return hash;
}

bool    ftFileProvider::FileDetails(FileInfo &info) {
    MixStackMutex stack(ftcMutex); /********** STACK LOCKED MTX ******/
    info.hash = hash;
    info.size = mSize;
    info.paths.append(path);
    info.transfered = req_loc ;
    info.lastTS = lastTS;
    info.status = FT_STATE_DOWNLOADING ;

    info.peers.clear() ;

    TransferInfo inf ;
    inf.cert_id = lastRequestor;
    inf.librarymixer_id = peers->findLibraryMixerByCertId(lastRequestor);
    inf.status = FT_STATE_DOWNLOADING ;

    inf.tfRate = transfer_rate/1024.0 ;
    info.tfRate = transfer_rate/1024.0 ;
    info.peers.push_back(inf) ;

    /* Use req_loc / req_size to estimate data rate */

    return true;
}


bool ftFileProvider::getFileData(uint64_t offset, uint32_t &chunk_size, void *data) {
    /* dodgey checking outside of mutex...
     * much check again inside FileAttrs().
     */
    MixStackMutex stack(ftcMutex); /********** STACK LOCKED MTX ******/

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    /*
     * FIXME: Warning of comparison between unsigned and signed int?
     */

    uint32_t data_size    = chunk_size;
    uint64_t base_loc     = offset;

    if (base_loc + data_size > mSize) {
        data_size = mSize - base_loc;
        chunk_size = mSize - base_loc;
#ifdef DEBUG_FT_FILE_PROVIDER
        std::cerr <<"Chunk Size greater than total file size, adjusting chunk size " << data_size << std::endl;
#endif
    }

    if (data_size > 0) {
        /*
                 * seek for base_loc
                 */
        file.seek(base_loc);

        // Data space allocated by caller.
        //void *data = malloc(chunk_size);

        /*
         * read the data
                 */

        if (file.read((char *)data, data_size) == -1) {
#ifdef DEBUG_FT_FILE_PROVIDER
            std::cerr << "ftFileProvider::getFileData() Failed to get data!";
#endif
            //free(data); no! already deleted in ftDataMultiplex::locked_handleServerRequest()
            return 0;
        }

        /*
         * Update status of ftFileStatus to reflect last usage (for GUI display)
         * We need to store.
         * (a) Id,
         * (b) Offset,
         * (c) Size,
         * (d) timestamp
         */

        time_t now_t = time(NULL) ;

        long int diff = (long int)now_t - (long int)lastTS_t ;  // in bytes/s. Average over multiple samples

#ifdef DEBUG_FT_FILE_PROVIDER
        std::cout << "diff = " << diff << std::endl ;
#endif

        if (diff > 3) {
            transfer_rate = total_size / (float)diff ;
#ifdef DEBUG_FT_FILE_PROVIDER
            std::cout << "updated TR = " << transfer_rate << ", total_size=" << total_size << std::endl ;
#endif
            lastTS_t = now_t ;
            total_size = 0 ;
        }

        req_loc = offset;
        lastTS = time(NULL) ;
        req_size = data_size;
        total_size += req_size ;
    } else {
#ifdef DEBUG_FT_FILE_PROVIDER
        std::cerr << "No data to read" << std::endl;
#endif
        return 0;
    }
    return 1;
}

bool ftFileProvider::moveFile(QString newPath) {
    bool ok;
    MixStackMutex stack(ftcMutex); /********** STACK LOCKED MTX ******/
    file->close();
    ok = DirUtil::moveFile(path, newPath);
    if (ok) {
        file->deleteLater();
        file=NULL;
        path = newPath;
        return true;
    } else return false;
}
