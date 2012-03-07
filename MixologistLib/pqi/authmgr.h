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

/*
 * AuthMgr
 *
 * The master store house for all recognized certificates, including own
 * Can be queried to manage and check validity of certificates.
 *
 * Also responsible for letting OpenSSL know whether or not we accept their certificates.
 *
 */

class AuthMgr;
extern AuthMgr *authMgr;

/* Not called directly, this function is passed to OpenSSL by InitAuth for use in verifying certificates.
   This takes the place of checking a chain of trusted certificates in a more classic usage,
   instead we accept only certificates that are within the limited universe of certificates downloaded from the LibraryMixer. */
int OpenSSLVerifyCB(X509_STORE_CTX *store, void *unused);

class AuthMgr {
public:
    AuthMgr() :authMgrInitialized(false), sslctx(NULL) {}
    ~AuthMgr() {}

    /**********************************************************************************
     * Initialization
     **********************************************************************************/
    /* True if initAuth has been called. */
    bool active() {return authMgrInitialized;}

    /* Initializes the AuthMgr, generating the encryption keys
       The new certificate will be written into cert in PEM format. */
    int InitAuth(unsigned int librarymixer_id, QString &cert);

    /* Why would we ever do this before shutdown, when it'll be closed anyway? Disabled.
    Frees the memory of internal AuthMgr data and shutsdown AuthMgr
    bool CloseAuth(); */

    /**********************************************************************************
     * SSL Utility Methods.
     **********************************************************************************/
    /* Returns the SSL_CTX object shared for all the application. */
    SSL_CTX *getCTX() {return sslctx;}

    /* Calculates an X509 certificates message digest, which is used as its ID.
       message_digest must be at least long enough to hold a SHA1 Digest, i.e. 20 */
    bool getCertId(X509 *cert, unsigned char message_digest[]);

    /* Adds a certificate to the list, or if there is already a certificate under that user, updates it.
       Returns 2 if a new user is added, 1 if a user is updated, 0 on no action or no cert, or -1 on error. */
    int addUpdateCertificate(QString cert, unsigned int librarymixer_id) ;

    /* Currently unused, as we aren't currently removing friends ever.
       Remove a user completely.
    bool RemoveCertificate(unsigned int librarymixer_id); */

    /**********************************************************************************
     * Body of public-facing API functions called through p3peers
     **********************************************************************************/
    /* Used by the entire application to get own certificate. */
    std::string OwnCertId() {return ownCertificateID;}

    /* Used by the entire application to get own LibraryMixer ID. */
    unsigned int OwnLibraryMixerId() {return ownLibraryMixerID;}

    /* Returns the associated cert_id or "" if unable to find. */
    std::string findCertByLibraryMixerId(unsigned int librarymixer_id);

    /* Returns the associated librarymixer_id or 0 if unable to find. */
    unsigned int findLibraryMixerByCertId(std::string cert_id);

private:
    mutable QMutex authMtx;

    /* Whether AuthMgr has been initializd. */
    bool authMgrInitialized;

    /* This OpenSSL structure stores our own keys, among other things, and will use them automatically. */
    SSL_CTX *sslctx;

    /* Master storage of a user's own certificate and LibraryMixer ID. */
    std::string ownCertificateID;
    unsigned int ownLibraryMixerID;

    std::map<unsigned int, X509 *> friendsCertificates; //friends' librarymixer_ids, friends' certs map
};

/* Helper Functions */
int printSSLError(SSL *ssl, int retval, int err, unsigned long err2, std::ostream &out);

#endif




