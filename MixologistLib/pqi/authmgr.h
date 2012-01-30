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

#ifndef GENERIC_AUTH_HEADER
#define GENERIC_AUTH_HEADER

#include <map>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <QString>

#include <QMutex>

/************** AUTHENTICATION MANAGER ***********
 * The master store house for all recognized certificates, including own
 * Can be queried to manage and check validity of certificates.
 */

class AuthMgr;
extern AuthMgr *authMgr;

//Not called directly, this function is attached to OpenSSL by InitAuth.
//Verifies validity of certificates of friends attempting to connect.
int OpenSSLVerifyCB(X509_STORE_CTX *store, void *unused);

class AuthMgr {
public:
    AuthMgr() :init(0), sslctx(NULL) {}
    ~AuthMgr() {
        return;
    }

    /* initialisation -> done by derived classes */
    //True if initAuth has been called
    bool active() {
        return init;
    }
    //Initializes SSL systems
    void initSSL();
    //Initializes the AuthMgr, generating the encryption keys
    //The new certificate will be written into cert in PEM format
    int InitAuth(unsigned int librarymixer_id, QString &cert);
    //Frees the memory of internal AuthMgr data and shutsdown AuthMgr
    bool CloseAuth();

    //Returns the SSL_CTX object shared for all the application
    SSL_CTX *getCTX() {
        return sslctx;
    }

    std::string OwnCertId() {
        return mCertId;
    }
    unsigned int OwnLibraryMixerId() {
        return mLibraryMixerId;
    }

    //Utility functions
    //Calculates an X509 certificates message digest, which is used as its ID.
    //message_digest must be at least long enough to hold a SHA1 Digest, i.e. 20
    bool getCertId(X509 *cert, unsigned char message_digest[]);
    //Returns the associated cert_id or "" if unable to find.
    std::string findCertByLibraryMixerId(unsigned int librarymixer_id);
    //Returns the associated librarymixer_id or 0 if unable to find.
    unsigned int findLibraryMixerByCertId(std::string cert_id);


    /* Add/Remove certificates */
    //Adds a certificate to mCerts, or if there is already a certificate under that user, updates it
    //Returns 2 if a new user is added, 1 if a user is updated, 0 on no action or no cert, or -1 on error
    int addUpdateCertificate(QString cert, unsigned int librarymixer_id) ;
    //Remove a user completely from the AuthMgr
    bool RemoveCertificate(unsigned int librarymixer_id);

private:
    /* Data */
    mutable QMutex authMtx;

    int init;

    //This OpenSSL structure stores our own keys, among other things, and will use them automatically
    SSL_CTX *sslctx;

    std::string mCertId;
    unsigned int mLibraryMixerId;

    std::map<unsigned int, X509 *> mCerts; //friends' librarymixer_ids, friends' certs map
};

/* Helper Functions */
int printSSLError(SSL *ssl, int retval, int err, unsigned long err2, std::ostream &out);

#endif




