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
#include "ft/fttemplist.h"
#include "ft/ftofflmlist.h"
#include "ft/ftfilewatcher.h"
#include "ft/ftcontroller.h"
#include "ft/ftfileprovider.h"
#include "ft/ftdatademultiplex.h"
#include "ft/ftborrower.h"

#include "services/mixologyservice.h"
#include "server/librarymixer-library.h"
#include "server/librarymixer-friendlibrary.h"

#include "pqi/pqi.h"
#include <pqi/friendsConnectivityManager.h>

#include "serialiser/serviceids.h"

#include <iostream>
#include <sstream>

#include <QSettings>
#include <interface/settings.h>

/* Setup */
ftServer::ftServer() : persongrp(NULL), mFtDataplex(NULL) {}

void ftServer::setP3Interface(P3Interface *pqi) {persongrp = pqi;}

void ftServer::setupMixologyService() {
    connect(mixologyService, SIGNAL(responseLendOfferReceived(uint,uint,QString,QStringList,QStringList,QList<qlonglong>)),
            this, SIGNAL(responseLendOfferReceived(uint,uint,QString,QStringList,QStringList,QList<qlonglong>)));
}

/* Final Setup (once everything is assigned) */
void ftServer::SetupFtServer() {
    fileDownloadController = new ftController();
    mFtDataplex = new ftDataDemultiplex(fileDownloadController);

    tempList = new ftTempList();
    mFtDataplex->addFileMethod(tempList);
    connect(tempList, SIGNAL(fileNoLongerAvailable(QString,qulonglong)), mFtDataplex, SLOT(fileNoLongerAvailable(QString,qulonglong)));

    /* librarymixermanager was already instantiated in init. */
    mFtDataplex->addFileMethod(librarymixermanager);
    connect(librarymixermanager, SIGNAL(fileNoLongerAvailable(QString,qulonglong)), mFtDataplex, SLOT(fileNoLongerAvailable(QString,qulonglong)));
    connect(librarymixermanager, SIGNAL(libraryItemAboutToBeInserted(int)), this, SIGNAL(libraryItemAboutToBeInserted(int)), Qt::DirectConnection);
    connect(librarymixermanager, SIGNAL(libraryItemAboutToBeRemoved(int)), this, SIGNAL(libraryItemAboutToBeRemoved(int)), Qt::DirectConnection);
    connect(librarymixermanager, SIGNAL(libraryItemInserted()), this, SIGNAL(libraryItemInserted()));
    connect(librarymixermanager, SIGNAL(libraryItemRemoved()), this, SIGNAL(libraryItemRemoved()));
    connect(librarymixermanager, SIGNAL(libraryStateChanged(int)), this, SIGNAL(libraryStateChanged(int)));

    connect(libraryMixerFriendLibrary, SIGNAL(friendLibraryItemAboutToBeInserted(int)), this, SIGNAL(friendLibraryItemAboutToBeInserted(int)), Qt::DirectConnection);
    connect(libraryMixerFriendLibrary, SIGNAL(friendLibraryItemAboutToBeRemoved(int)), this, SIGNAL(friendLibraryItemAboutToBeRemoved(int)), Qt::DirectConnection);
    connect(libraryMixerFriendLibrary, SIGNAL(friendLibraryItemInserted()), this, SIGNAL(friendLibraryItemInserted()));
    connect(libraryMixerFriendLibrary, SIGNAL(friendLibraryItemRemoved()), this, SIGNAL(friendLibraryItemRemoved()));
    connect(libraryMixerFriendLibrary, SIGNAL(friendLibraryStateChanged(int)), this, SIGNAL(friendLibraryStateChanged(int)));

    QSettings settings(*mainSettings, QSettings::IniFormat);
    if (settings.value("Transfers/EnableOffLibraryMixer", DEFAULT_ENABLE_OFF_LIBRARYMIXER_SHARING).toBool()) {
        offLMList = new ftOffLMList();
        mFtDataplex->addFileMethod(offLMList);
        connect(offLMList, SIGNAL(fileNoLongerAvailable(QString,qulonglong)), mFtDataplex, SLOT(fileNoLongerAvailable(QString,qulonglong)));
        connect(offLMList, SIGNAL(offLMFriendAboutToBeAdded(int)), this, SIGNAL(offLMFriendAboutToBeAdded(int)), Qt::DirectConnection);
        connect(offLMList, SIGNAL(offLMFriendAboutToBeRemoved(int)), this, SIGNAL(offLMFriendAboutToBeRemoved(int)), Qt::DirectConnection);
        connect(offLMList, SIGNAL(offLMFriendAdded()), this, SIGNAL(offLMFriendAdded()));
        connect(offLMList, SIGNAL(offLMFriendRemoved()), this, SIGNAL(offLMFriendRemoved()));
        connect(offLMList, SIGNAL(offLMOwnItemAboutToBeAdded(OffLMShareItem*)), this, SIGNAL(offLMOwnItemAboutToBeAdded(OffLMShareItem*)), Qt::DirectConnection);
        connect(offLMList, SIGNAL(offLMOwnItemAdded()), this, SIGNAL(offLMOwnItemAdded()));
        connect(offLMList, SIGNAL(offLMOwnItemChanged(OffLMShareItem*)), this, SIGNAL(offLMOwnItemChanged(OffLMShareItem*)));
        connect(offLMList, SIGNAL(offLMOwnItemAboutToBeRemoved(OffLMShareItem*)), this, SIGNAL(offLMOwnItemAboutToBeRemoved(OffLMShareItem*)), Qt::DirectConnection);
        connect(offLMList, SIGNAL(offLMOwnItemRemoved()), this, SIGNAL(offLMOwnItemRemoved()));
    }

    friendsConnectivityManager->addMonitor(fileDownloadController);

    return;
}

void ftServer::StartupThreads() {
    fileDownloadController->start();

    mFtDataplex->start();
}

/***************************************************************/
/********************** Files Interface ************************/
/***************************************************************/


/***************************************************************/
/********************** Controller Access **********************/
/***************************************************************/

bool ftServer::LibraryMixerRequest(unsigned int librarymixer_id, unsigned int item_id, QString name) {
    return mixologyService->LibraryMixerRequest(librarymixer_id, item_id, name);
}

bool ftServer::LibraryMixerRequestCancel(unsigned int item_id) {
    return mixologyService->LibraryMixerRequestCancel(item_id);
}

void ftServer::MixologySuggest(unsigned int librarymixer_id, unsigned int item_id) {
    librarymixermanager->MixologySuggest(librarymixer_id, item_id);
}

bool ftServer::downloadFiles(unsigned int friend_id, const QString &title,
                             const QStringList &paths, const QStringList &hashes, const QList<qlonglong> &filesizes) {
    return fileDownloadController->downloadFiles(friend_id, title, paths, hashes, filesizes);
}

bool ftServer::borrowFiles(unsigned int friend_id, const QString &title, const QStringList &paths, const QStringList &hashes,
                           const QList<qlonglong> &filesizes, uint32_t source_type, QString source_id) {
    return fileDownloadController->borrowFiles(friend_id, title, paths, hashes, filesizes, source_type, source_id);
}

void ftServer::cancelDownloadGroup(int groupId) {
    fileDownloadController->cancelDownloadGroup(groupId);
}

void ftServer::cancelFile(int groupId, const QString &hash) {
    fileDownloadController->cancelFile(groupId, hash);
}

bool ftServer::controlFile(int orig_item_id, QString hash, uint32_t flags) {
    return fileDownloadController->controlFile(orig_item_id, hash, flags);
}

void ftServer::clearCompletedFiles() {
    fileDownloadController->clearCompletedFiles();
    mixologyService->clearCompleted();
}

void ftServer::clearUploads() {
    mFtDataplex->clearUploads();
}

/* Directory Handling */
void ftServer::setDownloadDirectory(QString path) {
    fileDownloadController->setDownloadDirectory(path);
}

void ftServer::setPartialsDirectory(QString path) {
    fileDownloadController->setPartialsDirectory(path);
}

QString ftServer::getDownloadDirectory() {
    return fileDownloadController->getDownloadDirectory();
}

QString ftServer::getPartialsDirectory() {
    return fileDownloadController->getPartialsDirectory();
}


/***************************************************************/
/************************* Other Access ************************/
/***************************************************************/

void ftServer::getPendingRequests(std::list<pendingRequest> &requests) {
    mixologyService->getPendingRequests(requests);
}


void ftServer::FileDownloads(QList<downloadGroupInfo> &downloads) {
    fileDownloadController->FileDownloads(downloads);
}

void ftServer::FileUploads(QList<uploadFileInfo> &uploads) {
    mFtDataplex->FileUploads(uploads);
}

/**********************************************************************************
 * LibraryMixer Item Control
 **********************************************************************************/
QMap<unsigned int, LibraryMixerItem*>* ftServer::getLibrary() {
    return librarymixermanager->getLibrary();
}

LibraryMixerItem* ftServer::getLibraryMixerItem(unsigned int item_id) {
    return librarymixermanager->getLibraryMixerItem(item_id);
}

LibraryMixerItem* ftServer::getLibraryMixerItem(QStringList paths) {
    return librarymixermanager->getLibraryMixerItem(paths);
}

int ftServer::getLibraryMixerItemStatus(unsigned int item_id, bool retry) {
    return librarymixermanager->getLibraryMixerItemStatus(item_id, retry);
}

bool ftServer::setMatchChat(unsigned int item_id){
    return librarymixermanager->setMatchChat(item_id);
}

bool ftServer::setMatchFile(unsigned int item_id, QStringList paths, LibraryMixerItem::ItemState itemState, unsigned int recipient){
    return librarymixermanager->setMatchFile(item_id, paths, itemState, recipient);
}

bool ftServer::setMatchMessage(unsigned int item_id, QString message){
    return librarymixermanager->setMatchMessage(item_id, message);
}

/**********************************************************************************
 * Friend LibraryMixer Item Control
 **********************************************************************************/

QMap<unsigned int, FriendLibraryMixerItem*>* ftServer::getFriendLibrary() {
    return libraryMixerFriendLibrary->getFriendLibrary();
}

/**********************************************************************************
 * Temporary Share Control
 **********************************************************************************/

void ftServer::sendTemporary(QString title, QStringList paths, unsigned int friend_id) {
    tempList->addTempItem(title, paths, friend_id);
}

void ftServer::returnFiles(const QString &title, const QStringList &paths, unsigned int friend_id, const QString &itemKey) {
    tempList->addTempItem("Return of '" + title + "'", paths, friend_id, itemKey);
}

/**********************************************************************************
 * Off LibraryMixer Sharing Control
 **********************************************************************************/

void ftServer::addOffLMShare(QString path){
    if (offLMList) offLMList->addOffLMShare(path);
}

bool ftServer::removeOffLMShare(OffLMShareItem* toRemove){
    if (offLMList) return offLMList->removeOffLMShare(toRemove);
    return false;
}

bool ftServer::setOffLMShareMethod(OffLMShareItem *toModify, OffLMShareItem::shareMethodState state){
    if (offLMList) return offLMList->setOffLMShareMethod(toModify, state);
    return false;
}

void ftServer::setOffLMShareLabel(OffLMShareItem *toModify, QString newLabel) {
    if (offLMList) offLMList->setOffLMShareLabel(toModify, newLabel);
}

OffLMShareItem* ftServer::getOwnOffLMRoot() const {
    if (offLMList) return offLMList->getOwnOffLMRoot();
    return NULL;
}

void ftServer::recheckOffLMFiles() const {
    if (offLMList) offLMList->recheckFiles();
}

OffLMShareItem* ftServer::getFriendOffLMShares(int index) const {
    if (offLMList) return offLMList->getFriendOffLMShares(index);
    return NULL;
}

int ftServer::getOffLMShareFriendCount() const {
    if (offLMList) return offLMList->getOffLMShareFriendCount();
    return 0;
}

/***
 * Borrowing Management
 **/

void ftServer::getBorrowings(QStringList &titles, QStringList &itemKeys, QList<unsigned int> &friendIds) {
    return borrowManager->getBorrowings(titles, itemKeys, friendIds);
}

void ftServer::getBorrowings(QStringList &titles, QStringList &itemKeys, unsigned int friendId) {
    return borrowManager->getBorrowings(titles, itemKeys, friendId);
}

void ftServer::deleteBorrowed(const QString &itemKey) {
    borrowManager->deleteBorrowed(itemKey);
}

/***************************************************************/
/**********************     Data Flow     **********************/
/***************************************************************/

/* Client Send */
bool ftServer::sendDataRequest(unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t offset, uint32_t chunksize) {
    /* create a packet */
    /* push to networking part */
    FileRequest *rfi = new FileRequest();

    /* id */
    rfi->LibraryMixerId(librarymixer_id);

    /* file info */
    rfi->file.filesize = size;
    rfi->file.hash = hash; /* ftr->hash; */

    /* offsets */
    rfi->fileoffset = offset; /* ftr->offset; */
    rfi->chunksize = chunksize; /* ftr->chunk; */

    persongrp->SendFileRequest(rfi);

    return true;
}

const uint32_t MAX_FT_CHUNK  = 8 * 1024; /* 8K */

/* Server Send */
bool ftServer::sendData(unsigned int librarymixer_id, QString hash, uint64_t size, uint64_t baseOffset, uint32_t chunkSize, void *data) {
    uint32_t remainingToSend = chunkSize;
    uint64_t offset = 0;
    uint32_t chunk;

    log(LOG_DEBUG_ALL, ftserverzone,
        QString("ftServer::sendData") +
        " hash: " + hash +
        " offset: " + QString::number(baseOffset) +
        " chunksize: " + QString::number(chunkSize));

    //We must now break up the chunk into multiple packets, each of which is <= than MAX_FT_CHUNK
    while (remainingToSend > 0) {
        /* workout size */
        chunk = (remainingToSend > MAX_FT_CHUNK) ? MAX_FT_CHUNK : remainingToSend;

        /******** New Serialiser Type *******/

        FileData *rfd = new FileData();

        /* set id */
        rfd->LibraryMixerId(librarymixer_id);

        /* file info */
        rfd->fd.file.filesize = size;
        rfd->fd.file.hash     = hash;
        rfd->fd.file.name     = ""; /* blank other data */
        rfd->fd.file.path     = "";
        rfd->fd.file.pop      = 0;
        rfd->fd.file.age      = 0;

        rfd->fd.file_offset = baseOffset + offset;

        /* file data */
        rfd->fd.binData.setBinData(&(((uint8_t *) data)[offset]), chunk);

        persongrp->SendFileData(rfd);

        offset += chunk;
        remainingToSend -= chunk;
    }

    /* clean up data */
    free(data);

    return true;
}

/* NB: The core lock must be activated before calling this.
 * This Lock should be moved lower into the system...
 * most likely destination is in ftServer.
 */
int ftServer::tick() {
    log(LOG_DEBUG_ALL, ftserverzone, "ftServer::tick()");

    if (persongrp == NULL) {
        log(LOG_DEBUG_ALERT, ftserverzone, "ftServer::tick() Invalid Interface()");
        return 1;
    }

    int moreToTick = 0;
    if (0 < persongrp->tick()) moreToTick = 1;
    if (0 < handleFileData()) moreToTick = 1;

    return moreToTick;
}

bool ftServer::handleFileData() {
    // now File Input.
    FileRequest *fr;
    FileData *fd;

    int i_init = 0;
    int i = 0;

    i_init = i;
    while ((fr = persongrp->GetFileRequest()) != NULL ) {
        i++; /* count */
        mFtDataplex->recvDataRequest(fr->LibraryMixerId(),
                                     fr->file.hash,  fr->file.filesize,
                                     fr->fileoffset, fr->chunksize);

        uploadFileInfo(ffr);
        delete fr;
    }

    // now File Data.
    i_init = i;
    while ((fd = persongrp->GetFileData()) != NULL ) {
        i++; /* count */

        /* incoming data */
        mFtDataplex->recvData(fd->LibraryMixerId(),
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

bool ftServer::ResumeTransfers() {
    fileDownloadController->activate();

    return true;
}
