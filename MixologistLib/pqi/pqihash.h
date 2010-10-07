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

#include <openssl/sha.h>
#include <string>
#include <sstream>
#include <iomanip>

class pqihash {
public:
    pqihash() {

        sha_hash = new uint8_t[SHA_DIGEST_LENGTH];
        sha_ctx = new SHA_CTX;
        SHA1_Init(sha_ctx);
        doHash = true;
    }

    ~pqihash() {
        delete[] sha_hash;
        delete sha_ctx;
    }


    void    addData(void *data, uint32_t len) {
        if (doHash) {
            SHA1_Update(sha_ctx, data, len);
        }
    }

    void    Complete(std::string &hash) {
        if (!doHash) {
            hash = endHash;
            return;
        }

        SHA1_Final(sha_hash, sha_ctx);

        std::ostringstream out;
        for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
            out << std::setw(2) << std::setfill('0') << std::hex;
            out << (unsigned int) (sha_hash[i]);
        }
        endHash = out.str();
        hash = endHash;
        doHash = false;

        return;
    }

private:

    bool    doHash;
    std::string endHash;
    uint8_t *sha_hash;
    SHA_CTX *sha_ctx;
};


