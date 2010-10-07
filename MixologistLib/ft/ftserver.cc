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

#include "util/debug.h"
const int ftserverzone = 29539;

#include "ft/ftserver.h"
#include "ft/ftitemlist.h"
#include "ft/ftfilesearch.h"
#include "ft/ftcontroller.h"
#include "ft/ftfileprovider.h"
#include "ft/ftdatamultiplex.h"
#include "ft/mixologyborrower.h"

#include "services/mixologyservice.h"
#include "interface/librarymixer-library.h"

// Includes CacheStrapper / FiMonitor / FiStore for us.

#include "pqi/pqi.h"
#include "pqi/p3connmgr.h"

#include "serialiser/serviceids.h"

#include <iostream>
#include <sstream>

/***
 * #define SERVER_DEBUG 1
 * #define DEBUG_TICK   1
 ***/

/* Setup */
ftServer::ftServer(AuthMgr *authMgr, p3ConnectMgr *connMgr)
    : mP3iface(NULL),
      mAuthMgr(authMgr), mConnMgr(connMgr),
      mFtController(NULL), mFtItems(NULL),
      mFtDataplex(NULL), mFtSearch(NULL) {
}

void    ftServer::setP3Interface(P3Interface *pqi) {
    mP3iface = pqi;
}

/* Control Interface */

std::string ftServer::OwnId() {
    std::string ownId;
    if (mConnMgr)
        ownId = mConnMgr->getOwnCertId();
    return ownId;
}

/* Final Setup (once everything is assigned) */
void ftServer::SetupFtServer() {

    /* needed to setup FiStore/Monitor */
    std::string ownId = mConnMgr->getOwnCertId();

    /* search/items List */
    mFtItems = new ftItemList();
    mFtSearch = new ftFileSearch();

    /* Transport */
    mFtDataplex = new ftDataMultiplex(ownId, this, mFtSearch);

    /* make Controller */
    mFtController = new ftController(mFtDataplex);
    mFtController -> setFtSearchNItem(mFtSearch, mFtItems);

    /* Create borrowing manager */
    mixologyborrower = new MixologyBorrower();

    /* complete search setup, add each searchable object to mFtSearch */
    mFtSearch->addSearchMode(mFtItems, FILE_HINTS_ITEM);

    mConnMgr->addMonitor(mFtController);

    return;
}


void    ftServer::StartupThreads() {
    /* start up order - important for dependencies */

    /* self contained threads */
    /* startup ItemList Thread */
    mFtItems->start();

    /* Controller thread */
    mFtController->start();

    /* Dataplex */
    mFtDataplex->start();
}

/***************************************************************/
/********************** Files Interface **********************/
/***************************************************************/


/***************************************************************/
/********************** Controller Access **********************/
/***************************************************************/

bool ftServer::LibraryMixerRequest(int librarymixer_id, int item_id, QString name) {
    return mixologyservice->LibraryMixerRequest(librarymixer_id, item_id, name);
}

bool ftServer::LibraryMixerRequestCancel(int item_id) {
    if (mixologyservice->LibraryMixerRequestCancel(item_id)) return true;
    mixologyborrower->cancelBorrow(item_id); //In case this is a borrow, cancel. Even if it isn't, no harm.
    return mFtController->LibraryMixerTransferCancel(item_id);
}

void ftServer::MixologySuggest(unsigned int librarymixer_id, int item_id) {
    mFtItems->MixologySuggest(librarymixer_id, item_id);
}

void ftServer::LibraryMixerBorrowed(unsigned int librarymixer_id, int item_id) {
    mixologyservice->LibraryMixerBorrowed(librarymixer_id, item_id);
}

void ftServer::LibraryMixerBorrowReturned(unsigned int librarymixer_id, int item_id) {
    mixologyservice->LibraryMixerBorrowReturned(librarymixer_id, item_id);
}

bool ftServer::requestFile(QString librarymixer_name, int orig_item_id, QString fname, std::string hash, uint64_t size,
                           uint32_t flags, QList<int> sourceIds) {
    return mFtController->requestFile(librarymixer_name, orig_item_id, fname, hash, size, flags, sourceIds);
}

bool ftServer::cancelFile(int orig_item_id, QString filename, std::string hash) {
    return mFtController->cancelFile(orig_item_id, filename, hash);
}

bool ftServer::controlFile(int orig_item_id, std::string hash, uint32_t flags) {
    return mFtController->controlFile(orig_item_id, hash, flags);
}

void ftServer::clearCompletedFiles() {
    mFtController->clearCompletedFiles();
    mixologyservice->clearCompleted();
}

void ftServer::clearUploads() {
    mFtDataplex->clearUploads();
}

/* Directory Handling */
void ftServer::setDownloadDirectory(QString path, bool trySavedSettings) {
    mFtController->setDownloadDirectory(path, trySavedSettings);
}

QString ftServer::getDownloadDirectory() {
    return mFtController->getDownloadDirectory();
}

void ftServer::setPartialsDirectory(QString path, bool trySavedSettings) {
    mFtController->setPartialsDirectory(path, trySavedSettings);
}

QString ftServer::getPartialsDirectory() {
    return mFtController->getPartialsDirectory();
}


/***************************************************************/
/************************* Other Access ************************/
/***************************************************************/

void ftServer::getPendingRequests(std::list<pendingRequest> &requests) {
    return mixologyservice->getPendingRequests(requests);
}


bool ftServer::FileDownloads(QList<FileInfo> &downloads) {
    return mFtController->FileDownloads(downloads);
}

bool ftServer::FileUploads(QList<FileInfo> &uploads) {
    return mFtDataplex->FileUploads(uploads);
}

/***************************************************************/
/******************* Item Management **************************/
/***************************************************************/
LibraryMixerItem ftServer::getItem(int id) {
    return mFtItems->getItem(id);
}

LibraryMixerItem ftServer::getItem(QStringList paths) {
    return mFtItems->getItem(paths);
}


/***************************************************************/
/******************* ItemList Access **********************/
/***************************************************************/

void ftServer::setItems() {
    mFtItems->setItems();
}

void ftServer::addItem(LibraryMixerItem item) {
    mFtItems->addItem(item);
}

void ftServer::removeItem(int id) {
    mFtItems->removeItem(id);
}

void ftServer::deleteRemoveItem(int id) {
    mFtItems->deleteRemoveItem(id);
}

int ftServer::getItemStatus(int id) {
    return mFtItems->getItemStatus(id);
}

LibraryMixerItem *ftServer::recheckItem(int id, bool *changed) {
    return mFtItems->recheckItem(id, changed);
}

bool ftServer::matchAndSend(int item_id, QStringList paths, int recipient) {
    LibraryMixerItem item = LibraryMixerLibraryManager::getLibraryMixerItem(item_id);
    if (item.empty()) return false;
    if (item.itemState != ITEM_UNMATCHED &&
            item.itemState != ITEM_MATCH_NOT_FOUND) return false;
    LibraryMixerLibraryManager::setMatchFile(item_id, paths, ITEM_MATCHED_TO_FILE, recipient);
    return true;
}

void ftServer::sendTemporary(QString title, QStringList paths, int recipient) {
    mFtItems->addTempItem(title, paths, recipient);
}

/***************************************************************/
/****************** End of Files Interface *******************/
/***************************************************************/

/***
 * Borrowing Management
 **/

void ftServer::completedDownload(int item_id, QStringList paths, QStringList hashes, QList<unsigned long>filesizes, QString createdDirectory) {
    mixologyborrower->completedDownloadBorrowCheck(item_id);
    LibraryMixerLibraryManager::completedDownloadLendCheck(item_id, paths, hashes, filesizes, createdDirectory);
}

bool ftServer::getBorrowings(int librarymixer_id, QStringList &titles, QList<int> &item_ids) {
    return mixologyborrower->getBorrowings(librarymixer_id, titles, item_ids);
}

void ftServer::getBorrowingInfo(QList<int> &item_ids, QList<borrowStatuses> &statuses, QStringList &names) {
    return mixologyborrower->getBorrowingInfo(item_ids, statuses, names);
}

void ftServer::addPendingBorrow(int item_id, QString librarymixer_name, int librarymixer_id, QStringList filenames, QStringList hashes, QStringList filesizes) {
    mixologyborrower->addPendingBorrow(item_id, librarymixer_name, librarymixer_id, filenames, hashes, filesizes);
}

void ftServer::cancelBorrow(int item_id) {
    mixologyborrower->cancelBorrow(item_id);
}

void ftServer::borrowPending(int item_id) {
    mixologyborrower->borrowPending(item_id);
}

void ftServer::returnBorrowed(int librarymixer_id, int item_id, QString title, QStringList paths) {
    mFtItems->addTempItem("Return of '" + title + "'", paths, librarymixer_id, item_id);
}

void ftServer::returnedBorrowed(int item_id) {
    mixologyborrower->returnedBorrowed(item_id);
}

/***************************************************************/
/**********************     Data Flow     **********************/
/***************************************************************/

/* Client Send */
bool    ftServer::sendDataRequest(std::string peerId, std::string hash,
                                  uint64_t size, uint64_t offset, uint32_t chunksize) {
    /* create a packet */
    /* push to networking part */
    FileRequest *rfi = new FileRequest();

    /* id */
    rfi->PeerId(peerId);

    /* file info */
    rfi->file.filesize   = size;
    rfi->file.hash       = hash; /* ftr->hash; */

    /* offsets */
    rfi->fileoffset = offset; /* ftr->offset; */
    rfi->chunksize  = chunksize; /* ftr->chunk; */

    mP3iface->SendFileRequest(rfi);

    return true;
}

//const uint32_t    MAX_FT_CHUNK  = 32 * 1024; /* 32K */
//const uint32_t    MAX_FT_CHUNK  = 16 * 1024; /* 16K */
const uint32_t  MAX_FT_CHUNK  = 8 * 1024; /* 16K */

/* Server Send */
bool    ftServer::sendData(std::string peerId, std::string hash, uint64_t size,
                           uint64_t baseoffset, uint32_t chunksize, void *data) {
    /* create a packet */
    /* push to networking part */
    uint32_t tosend = chunksize;
    uint64_t offset = 0;
    uint32_t chunk;

#ifdef SERVER_DEBUG
    std::cerr << "ftServer::sendData() to " << peerId << std::endl;
    std::cerr << "hash: " << hash;
    std::cerr << " offset: " << baseoffset;
    std::cerr << " chunk: " << chunksize;
    std::cerr << " data: " << data;
    std::cerr << std::endl;
#endif

    while (tosend > 0) {
        /* workout size */
        chunk = MAX_FT_CHUNK;
        if (chunk > tosend) {
            chunk = tosend;
        }

        /******** New Serialiser Type *******/

        FileData *rfd = new FileData();

        /* set id */
        rfd->PeerId(peerId);

        /* file info */
        rfd->fd.file.filesize = size;
        rfd->fd.file.hash     = hash;
        rfd->fd.file.name     = ""; /* blank other data */
        rfd->fd.file.path     = "";
        rfd->fd.file.pop      = 0;
        rfd->fd.file.age      = 0;

        rfd->fd.file_offset = baseoffset + offset;

        /* file data */
        rfd->fd.binData.setBinData(
            &(((uint8_t *) data)[offset]), chunk);

        /* print the data pointer */
#ifdef SERVER_DEBUG
        std::cerr << "ftServer::sendData() Packet: " << std::endl;
        std::cerr << " offset: " << rfd->fd.file_offset;
        std::cerr << " chunk: " << chunk;
        std::cerr << " len: " << rfd->fd.binData.bin_len;
        std::cerr << " data: " << rfd->fd.binData.bin_data;
        std::cerr << std::endl;
#endif


        mP3iface->SendFileData(rfd);

        offset += chunk;
        tosend -= chunk;
    }

    /* clean up data */
    free(data);

    return true;
}

void ftServer::sendMixologySuggestion(unsigned int librarymixer_id, int item_id, QString name) {
    mixologyservice->sendMixologySuggestion(librarymixer_id, item_id, name);
}

/* NB: The core lock must be activated before calling this.
 * This Lock should be moved lower into the system...
 * most likely destination is in ftServer.
 */
int ftServer::tick() {
    log(LOG_DEBUG_ALL, ftserverzone, "ftServer::tick()");

    if (mP3iface == NULL) {
#ifdef SERVER_DEBUG
        std::cerr << "ftServer::tick() ERROR: mP3iface == NULL";
#endif

        std::ostringstream out;
        log(LOG_DEBUG_BASIC, ftserverzone,
            "ftServer::tick() Invalid Interface()");

        return 1;
    }

    int moreToTick = 0;

    if (0 < mP3iface -> tick()) {
        moreToTick = 1;
#ifdef DEBUG_TICK
        std::cerr << "ftServer::tick() moreToTick from mP3iface" << std::endl;
#endif
    }

    if (0 < handleFileData()) {
        moreToTick = 1;
#ifdef DEBUG_TICK
        std::cerr << "ftServer::tick() moreToTick from InputQueues" << std::endl;
#endif
    }
    return moreToTick;
}

bool    ftServer::handleFileData() {
    // now File Input.
    FileRequest *fr;
    FileData *fd;

    int i_init = 0;
    int i = 0;

    i_init = i;
    while ((fr = mP3iface -> GetFileRequest()) != NULL ) {
#ifdef SERVER_DEBUG
        std::cerr << "ftServer::handleFileData() Recvd ftFiler Request" << std::endl;
        std::ostringstream out;
        if (i == i_init) {
            out << "Incoming(Net) File Item:" << std::endl;
        }
        fr -> print(out);
        log(LOG_DEBUG_BASIC, ftserverzone, out.str());
#endif

        i++; /* count */
        mFtDataplex->recvDataRequest(fr->PeerId(),
                                     fr->file.hash,  fr->file.filesize,
                                     fr->fileoffset, fr->chunksize);

        FileInfo(ffr);
        delete fr;
    }

    // now File Data.
    i_init = i;
    while ((fd = mP3iface -> GetFileData()) != NULL ) {
#ifdef SERVER_DEBUG
        std::cerr << "ftServer::handleFileData() Recvd ftFiler Data" << std::endl;
        std::cerr << "hash: " << fd->fd.file.hash;
        std::cerr << " length: " << fd->fd.binData.bin_len;
        std::cerr << " data: " << fd->fd.binData.bin_data;
        std::cerr << std::endl;

        std::ostringstream out;
        if (i == i_init) {
            out << "Incoming(Net) File Data:" << std::endl;
        }
        fd -> print(out);
        log(LOG_DEBUG_BASIC, ftserverzone, out.str());
#endif
        i++; /* count */

        /* incoming data */
        mFtDataplex->recvData(fd->PeerId(),
                              fd->fd.file.hash,  fd->fd.file.filesize,
                              fd->fd.file_offset,
                              fd->fd.binData.bin_len,
                              fd->fd.binData.bin_data);

        /* we've stolen the data part -> so blank before delete
         */
        fd->fd.binData.TlvShallowClear();
        delete fd;
    }

    if (i > 0) {
        return 1;
    }
    return 0;
}

/**********************************
 **********************************
 **********************************
 *********************************/

/***************************** CONFIG ****************************/

bool    ftServer::ResumeTransfers() {
    mFtController->activate();

    return true;
}
