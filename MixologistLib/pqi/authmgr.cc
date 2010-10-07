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

#include "authmgr.h"
#include "pqinetwork.h"
#include "util/debug.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <sstream>
#include <iomanip>

//100 is an arbitrary number, but should be large enough for any LibraryMixer ids for long to come
#define ID_BUF_SIZE 100

// the single instance of this.
static AuthMgr instance_authroot;

AuthMgr *getAuthMgr() {
    return &instance_authroot;
}

/* We maintain an internal store of certificates in AuthMgr, that are downloaded from LibraryMixer.
   These represent all certificates that we trust the validity for.
   If an incoming certificate is in that store, then we trust its validity.
   We shouldn't need to verify anything on the cert, because that is the cert that was uploaded
   by its owner, and also, we verified things like CN name match on add. */
int OpenSSLVerifyCB(X509_STORE_CTX *store, void *unused) {
    (void) unused;
    unsigned char peercert_id[20];
    if (getAuthMgr()->getCertId(store->cert, peercert_id)) {
        std::string cert_id = std::string((char *)peercert_id, sizeof(peercert_id));
        if (!cert_id.empty()) {
            if (getAuthMgr()->findLibraryMixerByCertId(cert_id) > 0) return 1;
        };
    }
    return 0;
}

#ifdef false
#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self()

/* This array will store all of the mutexes available to OpenSSL. */
static MUTEX_TYPE *mutex_buf = NULL;

static void locking_function(int mode, int n, const char *file, int line) {
    if (mode & CRYPTO_LOCK)
        MUTEX_LOCK(mutex_buf[n]);
    else
        MUTEX_UNLOCK(mutex_buf[n]);
}
static unsigned long id_function(void) {
    return (reinterpret_cast<unsigned long>(THREAD_ID.p));
}
#endif

/********AuthMgr*************/

void AuthMgr::initSSL() {
    SSL_load_error_strings();
    SSL_library_init();

    //OpenSSL thread setup
    /*        int i;
            mutex_buf = (MUTEX_TYPE *)malloc(CRYPTO_num_locks() *sizeof(MUTEX_TYPE));
            if (!mutex_buf)
                return;
            for (i = 0; i < CRYPTO_num_locks(); i++)
                MUTEX_SETUP(mutex_buf[i]);
            CRYPTO_set_id_callback(id_function);
            CRYPTO_set_locking_callback(locking_function);*/
}

// args: server cert, server private key, trusted certificates.
int AuthMgr::InitAuth(int librarymixer_id, QString &cert) {
    authMtx.lock();   /***** LOCK *****/

    if (init == 1) {
        return 1;
    }

    mLibraryMixerId = librarymixer_id;

    //Call OpenSSL to create own keys and cert
    EVP_PKEY *pkey = EVP_PKEY_new();
    if (pkey == NULL) return false;
    X509 *x509;
    if ((x509=X509_new()) == NULL) return false;
    RSA *rsa = RSA_generate_key(2048, RSA_3, NULL, NULL);
    if (rsa == NULL) return false;
    if (!EVP_PKEY_assign_RSA(pkey, rsa)) return false;

    X509_set_version(x509,2);
    X509_gmtime_adj(X509_get_notBefore(x509),0);
    X509_gmtime_adj(X509_get_notAfter(x509),60*60*24*365); //Set validity to a year
    X509_set_pubkey(x509,pkey);

    X509_NAME *x509name=NULL;
    x509name = X509_get_subject_name(x509);
    char idstring[ID_BUF_SIZE];
    sprintf(idstring, "%d", librarymixer_id);
    if (!X509_NAME_add_entry_by_txt(x509name,"CN", MBSTRING_ASC, (unsigned char *)idstring, -1, -1, 0)) return false;
    if (!X509_set_issuer_name(x509,x509name)) return false;
    if (!X509_sign(x509,pkey,EVP_sha1())) return false;


    unsigned char cert_id[20];
    if (!getCertId(x509, cert_id)) return false;
    mCertId = std::string((char *)cert_id, 20);

    //Call OpenSSL to output the new certificate to the cert variable
    BIO *bp = BIO_new(BIO_s_mem());
    if (!PEM_write_bio_X509(bp, x509)) return false;
    /* translate the bp data to a string */
    char *data;
    int len = BIO_get_mem_data(bp, &data);
    for (int i = 0; i < len; i++) {
        cert.append(data[i]);
    }
    BIO_free(bp);

    //Call OpenSSL to setup ssl context
    sslctx = SSL_CTX_new(TLSv1_method());
    if (SSL_CTX_set_cipher_list(sslctx, "DEFAULT") != 1) return false;
    if (SSL_CTX_use_PrivateKey(sslctx, pkey) != 1) return false;
    if (SSL_CTX_use_certificate(sslctx, x509) != 1) return false;
    if (SSL_CTX_check_private_key(sslctx) != 1) return false;
    //SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT|SSL_VERIFY_CLIENT_ONCE, OpenSSLVerifyCB);
    //SSL_CTX_set_verify_depth(sslctx, 1);
    SSL_CTX_set_verify(sslctx, SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    SSL_CTX_set_cert_verify_callback(sslctx, OpenSSLVerifyCB, NULL);

    EVP_PKEY_free(pkey); //This also frees rsa
    X509_free(x509);
    init = 1;

    authMtx.unlock(); /**** UNLOCK ****/

    return true;
}

bool    AuthMgr::CloseAuth() {
    authMtx.lock();   /***** LOCK *****/
    SSL_CTX_free(sslctx);
    // clean up private key....
    // remove certificates etc -> opposite of initssl.
    init = 0;
    authMtx.unlock(); /**** UNLOCK ****/

    //OpenSSL thread shutdown
    /*        int i;
            if (!mutex_buf) return 0;
            CRYPTO_set_id_callback(NULL);
            CRYPTO_set_locking_callback(NULL);
            for (i = 0; i < CRYPTO_num_locks(); i++)
                MUTEX_CLEANUP(mutex_buf[i]);
            free(mutex_buf);
            mutex_buf = NULL;*/
    return 1;
}

bool AuthMgr::getCertId(X509 *cert, unsigned char message_digest[]) {
    unsigned int length;
    return X509_digest(cert,EVP_sha1(),message_digest,&length);
}

std::string AuthMgr::findCertByLibraryMixerId(int librarymixer_id) {
    std::map<int, X509 *>::iterator it;
    authMtx.lock();   /***** LOCK *****/
    it = mCerts.find(librarymixer_id);

    if (it == mCerts.end()) {
        authMtx.unlock(); /**** UNLOCK ****/
        return "";
    }
    std::string result = std::string(reinterpret_cast<char *>(it->second->sha1_hash), sizeof(it->second->sha1_hash));
    authMtx.unlock(); /**** UNLOCK ****/
    return result;
}

int AuthMgr::findLibraryMixerByCertId(std::string cert_id) {
    std::map<int, X509 *>::iterator it;
    authMtx.lock();   /***** LOCK *****/

    if(cert_id.empty()) log(LOG_ERROR, AUTHMGRZONE, "findLibraryMixerByCertId called with empty string!\n");

    for(it = mCerts.begin(); it != mCerts.end(); it++) {
        if (cert_id == std::string(reinterpret_cast<char *>(it->second->sha1_hash), sizeof(it->second->sha1_hash))) {
            int librarymixer_id = it->first;
            authMtx.unlock(); /**** UNLOCK ****/
            return librarymixer_id;
        }
    }
    authMtx.unlock(); /**** UNLOCK ****/
    return -1;
}

int AuthMgr::addUpdateCertificate(QString cert, int librarymixer_id) {
    int retint = 0;
    unsigned char *name = NULL;
    X509_NAME *x509_name = NULL;
    int pos;
    X509_NAME_ENTRY *entry = NULL;
    ASN1_STRING *entry_str;
    if (cert == "") return retint;
    std::map<int, X509 *>::iterator it;

    //Use OpenSSL to create an X509 certificate object
    int cert_size = cert.length() + 1;
    char *certificate = (char *)malloc(cert_size);
    strcpy(certificate, cert.toStdString().c_str());
    BIO *bp = BIO_new_mem_buf(certificate, cert_size);

    X509 *x509 = PEM_read_bio_X509(bp, NULL, 0, NULL);
    if(!x509) {
        retint = -1;
        goto end;
    }

    //Check that CN name matches Librarymixer ID
    x509_name = X509_get_subject_name(x509);
    pos = X509_NAME_get_index_by_NID(x509_name, NID_commonName, -1);
    if (pos < 0) {
        retint = -1;
        goto end;
    }
    entry = X509_NAME_get_entry(x509_name, pos);
    if (!entry) {
        retint = -1;
        goto end;
    }
    entry_str = X509_NAME_ENTRY_get_data(entry);
    if (!entry_str) {
        retint = -1;
        goto end;
    }
    ASN1_STRING_to_UTF8(&name, entry_str);
    if (QString((char *)name).toInt() != librarymixer_id) {
        retint = -1;
        goto end;
    }

    //Check that issuer name matches LibraryMixer ID
    x509_name = X509_get_issuer_name(x509);
    pos = X509_NAME_get_index_by_NID(x509_name, NID_commonName, -1);
    if (pos < 0) {
        retint = -1;
        goto end;
    }
    entry = X509_NAME_get_entry(x509_name, pos);
    if (!entry) {
        retint = -1;
        goto end;
    }
    entry_str = X509_NAME_ENTRY_get_data(entry);
    if (!entry_str) {
        retint = -1;
        goto end;
    }
    ASN1_STRING_to_UTF8(&name, entry_str);
    if (QString((char *)name).toInt() != librarymixer_id) {
        retint = -1;
        goto end;
    }

    //Add in hash
    if (!getCertId(x509, x509->sha1_hash)) {
        retint = -1;
        goto end;
    }
    //Check if we are updating or adding a new entry or doing nothing
    authMtx.lock();   /***** LOCK *****/
    //If this is a new user
    it = mCerts.find(librarymixer_id);
    if (it == mCerts.end()) {
        retint = 2;
    } else {
        //existing user with same information
        if (std::string(reinterpret_cast<char *>(it->second->sha1_hash), sizeof(it->second->sha1_hash)) ==
                std::string(reinterpret_cast<char *>(x509->sha1_hash), sizeof(x509->sha1_hash))) {
            retint = 0;
        } else { //updated information
            retint = 1;
        }
    }

    //Actually add the new x509 certificate
    mCerts[librarymixer_id] = x509;
    authMtx.unlock(); /**** UNLOCK ****/

end:
    if (certificate != NULL) free(certificate);
    if (bp != NULL) BIO_free(bp);
    return retint;
}

bool AuthMgr::RemoveCertificate(int librarymixer_id) {
    authMtx.lock();   /***** LOCK *****/
    mCerts.erase(librarymixer_id);
    authMtx.unlock(); /**** UNLOCK ****/
    return true;
}

/********** SSL ERROR STUFF ******************************************/

int printSSLError(SSL *, int retval, int err, unsigned long err2,
                  std::ostream &out) {
    std::string mainreason = std::string("UNKNOWN ERROR CODE");
    if (err == SSL_ERROR_NONE) {
        mainreason =  std::string("SSL_ERROR_NONE");
    } else if (err == SSL_ERROR_ZERO_RETURN) {
        mainreason =  std::string("SSL_ERROR_ZERO_RETURN");
    } else if (err == SSL_ERROR_WANT_READ) {
        mainreason =  std::string("SSL_ERROR_WANT_READ");
    } else if (err == SSL_ERROR_WANT_WRITE) {
        mainreason =  std::string("SSL_ERROR_WANT_WRITE");
    } else if (err == SSL_ERROR_WANT_CONNECT) {
        mainreason =  std::string("SSL_ERROR_WANT_CONNECT");
    } else if (err == SSL_ERROR_WANT_ACCEPT) {
        mainreason =  std::string("SSL_ERROR_WANT_ACCEPT");
    } else if (err == SSL_ERROR_WANT_X509_LOOKUP) {
        mainreason =  std::string("SSL_ERROR_WANT_X509_LOOKUP");
    } else if (err == SSL_ERROR_SYSCALL) {
        mainreason =  std::string("SSL_ERROR_SYSCALL");
    } else if (err == SSL_ERROR_SSL) {
        mainreason =  std::string("SSL_ERROR_SSL");
    }
    out << "RetVal(" << retval;
    out << ") -> SSL Error: " << mainreason << std::endl;
    out << "\t + ERR Error: " << ERR_error_string(err2, NULL) << std::endl;
    return 1;
}
