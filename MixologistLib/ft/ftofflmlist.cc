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

#include <ft/ftserver.h>
#include <ft/ftfilewatcher.h>
#include <ft/ftofflmlist.h>
#include <ft/fttransfermodule.h>
#include <services/statusservice.h>
#include <util/xml.h>
#include <util/dir.h>
#include <util/debug.h>
#include <interface/iface.h>
#include <interface/peers.h>
#include <interface/init.h>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>

#define OWN_FILE "own.xml"
#define OWN_SANITIZED_FILE "sanitized.xml"
#define OFF_LM_DIR "offLM/"

ftOffLMList::ftOffLMList() {
    QMutexLocker stack(&offLmMutex);

    /* We directly handle these signals in ftOffLMList, so that we can check if they are dealing with the hash of our own XML file.
       If they are, we handle them directly in ftOffLMList, otherwise we pass shared file hashing information through to OffLMHelper.
       Other signals from fileWatcher aren't relevant to the XML, so we have OffLMHelper handle those directly. */
    connect(fileWatcher, SIGNAL(newFileHash(QString,qlonglong,uint,QString)), this, SLOT(newFileHash(QString,qlonglong,uint,QString)), Qt::QueuedConnection);

    /* Load the XML files that store the shares for both self and friends. */
    if (DirUtil::checkCreateDirectory(Init::getUserDirectory(true) + OFF_LM_DIR)) {
        /* Load own XML */
        QDomElement rootNode;
        if (XmlUtil::openXml(ownXmlFile(), ownXml, rootNode, "offLM", QIODevice::ReadWrite)){
            rootItem = new OffLMShareItem(rootNode, 0);
            connect(rootItem, SIGNAL(fileNoLongerAvailable(QString,qulonglong)), this, SIGNAL(fileNoLongerAvailable(QString,qulonglong)));
            connect(rootItem, SIGNAL(itemAboutToBeAdded(OffLMShareItem*)), this, SIGNAL(offLMOwnItemAboutToBeAdded(OffLMShareItem*)), Qt::DirectConnection);
            connect(rootItem, SIGNAL(itemAdded()), this, SIGNAL(offLMOwnItemAdded()));
            connect(rootItem, SIGNAL(itemChanged(OffLMShareItem*)), this, SIGNAL(offLMOwnItemChanged(OffLMShareItem*)));
            connect(rootItem, SIGNAL(itemAboutToBeRemoved(OffLMShareItem*)), this, SIGNAL(offLMOwnItemAboutToBeRemoved(OffLMShareItem*)), Qt::DirectConnection);
            connect(rootItem, SIGNAL(itemRemoved()), this, SIGNAL(offLMOwnItemRemoved()));
        }

        if (QFileInfo(ownSanitizedXmlFile()).exists()) {
            /* We must use a blocking hash here because if for whatever reason the hash is delayed,
               we don't want to be in a situation where we don't have own XML meta data yet and a
               status update gets sent out to friends with bad information. */
            DirUtil::getFileHash(ownSanitizedXmlFile(), sanitizedXmlHash);
            sanitizedXmlSize = QFileInfo(ownSanitizedXmlFile()).size();
        } else {
            sanitizedXmlHash = "";
            sanitizedXmlSize = 0;
        }

        /* Load friends' XMLs, which are stored in the OFF_LM_DIR with name equal to friend_id.xmlHash.xml. */
        QDir offLMDir(Init::getUserDirectory(true) + OFF_LM_DIR);
        foreach(QString currentFile, offLMDir.entryList(QDir::Files)) {
            /* Can't perform cleanup of orphaned xmls for removed friends here, because friends list is not yet ready.
               Where can we do it?
               It's probably best to add a method orphanCleanup that is called after startup from the tick.*/
            if (currentFile == OWN_FILE || currentFile == OWN_SANITIZED_FILE) continue;

            bool valid;
            int friendId = currentFile.split(".")[0].toInt(&valid);
            if (valid) {
                QString hash = currentFile.split(".")[1];
                if (!readFriendOffLMShares(friendId, hash)) valid = false;
            }

            if (!valid) QFile(offLMDir.canonicalPath() + QDir::separator() + currentFile).remove();
        }
    }

    /* Initiate the helper thread that is used to handle long-running operations. */
    QThread* offLMHelperThread = new QThread();
    offLMHelper = new OffLMHelper(this);
    offLMHelper->moveToThread(offLMHelperThread);
    offLMHelperThread->start();
}

void ftOffLMList::tick() {
    QMutexLocker stack(&offLmMutex);
    foreach (unsigned int friend_id, friendsXmlDownloads.keys()) {
        /* Tick returns 1 when the transfer is complete. */
        int tickResult = friendsXmlDownloads[friend_id]->tick();
        if (tickResult == 1) {
            if (addFriendOffLMShares(friend_id,
                                     friendsXmlDownloads[friend_id]->mFileCreator->getPath(),
                                     friendsXmlDownloads[friend_id]->mFileCreator->getHash())) {
                removeFriendDownload(friend_id);
            }
        }
    }
}

void ftOffLMList::addOffLMShare(QString path){
    offLMHelper->addShare(path);
}

bool ftOffLMList::removeOffLMShare(OffLMShareItem* toRemove){
    /* This can be handled without involving the offLMHelper because removing is fast. */
    if (toRemove->isShare()){
        if (rootItem->removeChild(toRemove->order())) {
            emit offLMOwnXmlUpdated();
            return true;
        }
    }
    return false;
}

bool ftOffLMList::setOffLMShareMethod(OffLMShareItem *toModify, OffLMShareItem::shareMethodState state){
    if (toModify->shareMethod(state)){
        emit offLMOwnXmlUpdated();
        return true;
    }
    return false;
}

void ftOffLMList::setOffLMShareLabel(OffLMShareItem *toModify, QString newLabel) {
    toModify->label(newLabel);
    emit offLMOwnXmlUpdated();
}

OffLMShareItem* ftOffLMList::getOwnOffLMRoot() const {
    return rootItem;
}

void ftOffLMList::recheckFiles() {
    emit recheckFilesRequested();
}

OffLMShareItem* ftOffLMList::getOffLMShareItem(const QString &label) {
    for (int i = 0; i < rootItem->childCount(); i++) {
        if (rootItem->child(i)->label() == label) return rootItem->child(i);
    }
    return NULL;
}

void ftOffLMList::getOwnOffLMXmlInfo(QString *hash, qlonglong *size) const {
    QMutexLocker stack(&offLmMutex);
    *hash = sanitizedXmlHash;
    *size = sanitizedXmlSize;
}

void ftOffLMList::receiveFriendOffLMXmlInfo(unsigned int friend_id, const QString &hash, qlonglong size) {
    QMutexLocker stack(&offLmMutex);
    if (friendRoots.contains(friend_id) &&
        friendRoots[friend_id]->xmlHash() == hash &&
        friendRoots[friend_id]->xmlSize() == size) return;

    if (hash.isEmpty()) {
        removeFriendOffLMShares(friend_id);
        return;
    } else {
        if (friendsXmlDownloads.contains(friend_id)) {
            if (hash == friendsXmlDownloads[friend_id]->mFileCreator->getHash()) {
                return;
            } else {
                removeFriendDownload(friend_id);
            }
        } else if (friendRoots.contains(friend_id) &&
                   friendRoots[friend_id]->xmlHash() == hash) {
            return;
        }
    }

    ftTransferModule* newXmlDownload = new ftTransferModule(friend_id, size, hash);
    newXmlDownload->transferStatus(ftTransferModule::FILE_DOWNLOADING);
    newXmlDownload->setPeerState(friend_id, PQIPEER_ONLINE_IDLE);
    friendsXmlDownloads[friend_id] = newXmlDownload;
}

bool ftOffLMList::handleReceiveData(const std::string &peerId, const QString &hash, uint64_t offset, uint32_t chunksize, void *data) {
    QMutexLocker stack(&offLmMutex);
    QMap<QString, ftTransferModule*>::const_iterator it;
    unsigned int friend_id = peers->findLibraryMixerByCertId(peerId);
    if (friendsXmlDownloads.contains(friend_id)) {
        friendsXmlDownloads[friend_id]->recvFileData(peerId, offset, chunksize, data);
        return true;
    }
    return false;
}

void ftOffLMList::setLent(unsigned int friend_id, const QString &label) {
    for (int i = 0; i < rootItem->childCount(); i++){
        if (rootItem->child(i)->label() == label)  rootItem->child(i)->lent(friend_id);
    }
    emit offLMOwnXmlUpdated();
}

int ftOffLMList::returnBorrowedFiles(unsigned int friend_id, const QString &item_id, const QStringList &paths, const QStringList &hashes,
                                     const QList<qlonglong> &filesizes, const QList<bool> &moveFile, QString &finalDestination) {
    OffLMShareItem* item = getOffLMShareItem(item_id);
    if (!item) return -3;
    if (item->lent() == 0) return -4;
    if (item->lent() != friend_id) return -5;

    item->lent(0);
    emit offLMOwnXmlUpdated();

    QStringList originalPaths;
    QStringList originalHashes;
    QList<qlonglong> originalFilesizes;
    item->getRecursiveFileInfo(originalPaths, originalHashes, originalFilesizes);

    int result = DirUtil::returnFilesToOriginalLocations(paths, hashes, filesizes, moveFile, originalPaths, originalHashes, originalFilesizes);
    if (result == 1) finalDestination = item->path();

    return result;
}

OffLMShareItem* ftOffLMList::getFriendOffLMShares(int index) const {
    QMutexLocker stack(&offLmMutex);
    if (index < 0 || index >= friends.size()) return NULL;
    return friendRoots[friends.at(index)];
}

int ftOffLMList::getOffLMShareFriendCount() const {
    /* This causes deadlocks when we emit we're about to insert a new because the view needs to check the size.
       QMutexLocker stack(&offLmMutex); */
    return friends.size();
}

ftFileMethod::searchResult ftOffLMList::search(const QString &hash, qlonglong size, uint32_t hintflags, QString &path) {
    if (hintflags | FILE_HINTS_OFF_LM) {
        QMutexLocker stack(&offLmMutex);
        if (hash == sanitizedXmlHash && size == sanitizedXmlSize) {
            path = ownSanitizedXmlFile();
            return ftFileMethod::SEARCH_RESULT_FOUND_INTERNAL_FILE;
        } else {
            return recursiveSearch(hash, size, path, rootItem);
        }
    }
    return ftFileMethod::SEARCH_RESULT_NOT_FOUND;
}

ftFileMethod::searchResult ftOffLMList::recursiveSearch(const QString &hash, qlonglong size, QString &path, OffLMShareItem *current) const {
    /* If lent out then don't search here. */
    if (current->isShare() && current->lent() != 0) return ftFileMethod::SEARCH_RESULT_NOT_FOUND;
    if (current->isShare() && current->shareMethod() == OffLMShareItem::STATE_TO_SEND_MISSING) return ftFileMethod::SEARCH_RESULT_NOT_FOUND;
    if (current->isShare() && current->shareMethod() == OffLMShareItem::STATE_TO_LEND_MISSING) return ftFileMethod::SEARCH_RESULT_NOT_FOUND;

    if (current->isRootItem() || current->isFolder()) {
        for (int i = 0; i < current->childCount(); i++) {
            ftFileMethod::searchResult result = recursiveSearch(hash, size, path, current->child(i));
            if (result != ftFileMethod::SEARCH_RESULT_NOT_FOUND) return result;
        }
    }
    else if (current->isFile()) {
        if ((current->hash() == hash) && (current->size() == (qlonglong)size) && !current->lent()) {
            path = current->fullPath();
            return ftFileMethod::SEARCH_RESULT_FOUND_SHARED_FILE;
        }
    }
    return ftFileMethod::SEARCH_RESULT_NOT_FOUND;
}


void ftOffLMList::statusChange(const std::list<pqipeer> &changeList) {
    QMutexLocker stack(&offLmMutex);

    QMap<QString, ftTransferModule*>::const_iterator it;
    std::list<pqipeer>::const_iterator changeListIt;

    foreach (pqipeer peer, changeList) {
        if (friendsXmlDownloads.contains(peer.librarymixer_id)) {
            if (peer.actions & PEER_CONNECTED) {
                friendsXmlDownloads[peer.librarymixer_id]->setPeerState(peer.librarymixer_id, PQIPEER_ONLINE_IDLE);
            } else {
                friendsXmlDownloads[peer.librarymixer_id]->setPeerState(peer.librarymixer_id, PQIPEER_NOT_ONLINE);
            }
        }
    }
}

void ftOffLMList::newFileHash(QString path, qlonglong size, unsigned int modified, QString newHash) {
    /* ftOffLMList receives two different types of hash jobs:
       (1) The hash of the user's own XML file
       (2) Hashes of the files that are being shared. */
    if (path == ownSanitizedXmlFile()) {
        QMutexLocker stack(&offLmMutex);
        emit fileNoLongerAvailable(sanitizedXmlHash, sanitizedXmlSize);
        sanitizedXmlHash = newHash;
        sanitizedXmlSize = size;
        /* If we just updated our sanitizedXml, and we don't have any other files that are pending information,
           send an immediate status update to friends. */
        if (rootItem->allFilesHashed()) statusService->sendStatusUpdateToAll();
    } else {
        emit updateOwnFileHash(path, size, modified, newHash);
    }
}

/**********************************************************************************
 * Private utility methods
 **********************************************************************************/

bool ftOffLMList::addFriendOffLMShares(unsigned int friend_id, QString path, QString xmlHash){
    log(LOG_WARNING, FTOFFLMLIST, "Received updated off-LibraryMixer list from " + QString::number(friend_id));
    if (friendRoots.contains(friend_id)) removeFriendOffLMShares(friend_id);
    DirUtil::moveFile(path, friendXmlFile(friend_id, xmlHash));
    return readFriendOffLMShares(friend_id, xmlHash);
}

bool ftOffLMList::readFriendOffLMShares(unsigned int friend_id, QString xmlHash) {
    QDomDocument friendXml;
    QDomElement friendRootNode;

    if (XmlUtil::openXml(friendXmlFile(friend_id, xmlHash), friendXml, friendRootNode, "offLM", QIODevice::ReadOnly)){
        OffLMShareItem* friendRootItem = new OffLMShareItem(friendRootNode, 0);
        friendRootItem->friendId(friend_id);
        friendRootItem->xmlHash(xmlHash);
        friendRootItem->xmlSize(QFileInfo(friendXmlFile(friend_id, xmlHash)).size());

        emit offLMFriendAboutToBeAdded(friends.size());
        friends.append(friend_id);
        friendRoots[friend_id] = friendRootItem;
        emit offLMFriendAdded();

        return true;
    }
    return false;
}

void ftOffLMList::removeFriendOffLMShares(unsigned int friend_id) {
    if (friendRoots.contains(friend_id)) {
        emit offLMFriendAboutToBeRemoved(friends.indexOf(friend_id));
        QFile::remove(friendXmlFile(friend_id, friendRoots[friend_id]->xmlHash()));
        friendRoots.remove(friend_id);
        friends.removeOne(friend_id);
        emit offLMFriendRemoved();
    }
}

QString ftOffLMList::ownXmlFile() const {
    return QDir::toNativeSeparators(Init::getUserDirectory(true).append(OFF_LM_DIR).append(OWN_FILE));
}

QString ftOffLMList::ownSanitizedXmlFile() const {
    return QDir::toNativeSeparators(Init::getUserDirectory(true).append(OFF_LM_DIR).append(OWN_SANITIZED_FILE));
}

QString ftOffLMList::friendXmlFile(unsigned int friend_id, QString xmlHash) const {
    return QDir::toNativeSeparators(Init::getUserDirectory(true) +
                                    OFF_LM_DIR +
                                    QString::number(friend_id) +
                                    "." +
                                    xmlHash +
                                    ".xml");
}

void ftOffLMList::removeFriendDownload(unsigned int friend_id) {
    QFile partialDownload(friendsXmlDownloads[friend_id]->mFileCreator->getPath());
    delete friendsXmlDownloads[friend_id];
    friendsXmlDownloads.remove(friend_id);
    //This will remove the file if it exists, or do nothing if it doesn't
    partialDownload.remove();
}

/**********************************************************************************
 * OffLMHelper
 **********************************************************************************/

OffLMHelper::OffLMHelper(ftOffLMList *list) :list(list){
    connect(list, SIGNAL(updateOwnFileHash(QString,qlonglong,uint,QString)), this, SLOT(updateOwnFileHash(QString,qlonglong,uint,QString)), Qt::QueuedConnection);
    connect(list, SIGNAL(recheckFilesRequested()), this, SLOT(recheckFiles()), Qt::QueuedConnection);
    connect(fileWatcher, SIGNAL(oldHashInvalidated(QString,qlonglong,uint)), this, SLOT(oldHashInvalidated(QString,qlonglong,uint)), Qt::QueuedConnection);
    connect(fileWatcher, SIGNAL(fileRemoved(QString)), this, SLOT(fileRemoved(QString)), Qt::QueuedConnection);
    connect(fileWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(directoryChanged(QString)), Qt::QueuedConnection);

    connect(this, SIGNAL(addShareRequested()), this, SLOT(processAddShare()), Qt::QueuedConnection);

    saveTimer = new QTimer(this);
    saveTimer->setSingleShot(true);
    connect(saveTimer, SIGNAL(timeout()), this, SLOT(processSaveOwnXml()));

    connect(list, SIGNAL(offLMOwnXmlUpdated()), this, SLOT(requestSaveOwnXml()), Qt::QueuedConnection);

    recheckFiles();
}

void OffLMHelper::addShare(QString path) {
    QMutexLocker stack(&offLMHelperMutex);
    if (findOffLMShare(path) != NULL) return;

    if (!pathsToAdd.contains(path)) {
        pathsToAdd.append(path);
        emit addShareRequested();
    }
}

void OffLMHelper::processAddShare() {
    QString path;
    {
        QMutexLocker stack(&offLMHelperMutex);
        path = pathsToAdd.first();
        pathsToAdd.removeFirst();
    }
    list->rootItem->addShare(path);
    requestSaveOwnXml();
}

/* This sets the time that must pass without a new save requested before the XML is actually saved. */
#define SAVE_OWN_XML_INTERVAL 5000 //5 seconds

void OffLMHelper::requestSaveOwnXml() {
    /* No mutex protection required, as this is the only function that alters the saveTimer, and
       this function is already specified to only be called via a queued connection or from inside its own thread. */
    saveTimer->start(SAVE_OWN_XML_INTERVAL);
}

void OffLMHelper::processSaveOwnXml() {
    /* No mutex protection as this is only called from the expiring timer, and never directly. */
    if (XmlUtil::writeXml(list->ownXmlFile(), list->ownXml)) {
        if (saveOwnSanitizedXml()) {
            if (QFile::exists(list->ownSanitizedXmlFile())) {
                fileWatcher->addHashJob(list->ownSanitizedXmlFile(), true);
            } else {
                list->sanitizedXmlHash = "";
                list->sanitizedXmlSize = 0;
            }
        }
    }
}

bool OffLMHelper::saveOwnSanitizedXml() {
    QDomDocument sanitizedXml;
    QDomElement sanitizedRootElement = sanitizedXml.createElement("offLM");
    sanitizedXml.appendChild(sanitizedRootElement);
    QDomElement currentShare = list->rootItem->domElement().firstChildElement("share");

    /* Originally I tried to simply clone the rootNode, but that approach failed in QT when appending the cloned rootNode to the Xml with the error:
       "Calling appendChild() on a null node does nothing." */
    while (!currentShare.isNull()) {
        QDomElement currentSanitizedShare = currentShare.cloneNode(true).toElement();
        sanitizedRootElement.appendChild(currentSanitizedShare);
        currentShare = currentShare.nextSiblingElement("share");
    }

    OffLMShareItem sanitizedRoot(sanitizedRootElement, 0, NULL);
    sanitizedRoot.makeShareableWithFriends();

    if (sanitizedRoot.childCount() > 0) {
        return XmlUtil::writeXml(list->ownSanitizedXmlFile(), sanitizedXml);
    } else {
        QFile(list->ownSanitizedXmlFile()).remove();
        /* This will update the internal stats on the sanitized XML file. */
        list->newFileHash(list->ownSanitizedXmlFile(), 0, 0, "");
        return true;
    }
}

void OffLMHelper::updateOwnFileHash(QString path, qlonglong size, unsigned int modified, QString newHash) {
    //If the same path shows up more than once we'll save the hash into all of them
    QList<OffLMShareItem*> matches = findOffLMShareItems(path, list->rootItem);

    if (!matches.isEmpty()) {
        foreach(OffLMShareItem* match, matches) {
            if (match->isFile()) {
                match->setFileInfo(size, modified, newHash);
            }
        }
        requestSaveOwnXml();
    }
}

void OffLMHelper::oldHashInvalidated(QString path, qlonglong size, unsigned int modified) {
    updateOwnFileHash(path, size, modified, "");
}

void OffLMHelper::fileRemoved(QString path) {
    /* Not the most focused on a single file being removed, but gets the job done for now. */
    directoryChanged(path);
}

void OffLMHelper::directoryChanged(QString path) {
    QList<OffLMShareItem*> matches = findOffLMShareItems(path, list->rootItem);

    if (!matches.isEmpty()) {
        foreach(OffLMShareItem* match, matches) {
            match->checkPathsOrUpdate();
        }
        requestSaveOwnXml();
    }
}

void OffLMHelper::recheckFiles() {
    if (!list->rootItem->checkPathsOrUpdate()) requestSaveOwnXml();
}

OffLMShareItem* OffLMHelper::findOffLMShare(const QString &path){
    QString normalizedPath = QDir::toNativeSeparators(path);
    for (int i = 0; i < list->rootItem->childCount(); i++){
        if (list->rootItem->child(i)->fullPath() == normalizedPath) return list->rootItem->child(i);
    }
    return NULL;
}

QList<OffLMShareItem*> OffLMHelper::findOffLMShareItems(const QString &path, OffLMShareItem* startingItem){
    QString normalizedPath = QDir::toNativeSeparators(path);
    QList<OffLMShareItem*> matches;
    if (startingItem->isShare() && startingItem->lent() != 0) return matches;

    if ((startingItem->isFolder() || startingItem->isFile()) &&
        normalizedPath == startingItem->fullPath()){
        matches.append(startingItem);
    } else if (startingItem->isRootItem() ||
               (startingItem->isFolder() &&
                normalizedPath.startsWith(startingItem->fullPath()))){
        for(int i = 0; i < startingItem->childCount(); i++){
            matches.append(findOffLMShareItems(normalizedPath, startingItem->child(i)));
        }
    }
    return matches;
}

