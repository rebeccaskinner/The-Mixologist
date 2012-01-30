/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2008, Robert Fernie
 *
 *  This file is part of the Mixologist.
 *
 *  Thfe Mixologist is free software; you can redistribute it and/or
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

#include <ft/ftcontroller.h>

#include <ft/fttransfermodule.h>
#include <ft/ftofflmlist.h>
#include <ft/ftfilemethod.h>
#include <ft/ftdatademultiplex.h>
#include <ft/ftserver.h>
#include <ft/ftborrower.h>

#include <server/librarymixer-library.h>

#include <services/mixologyservice.h>

#include <interface/peers.h>

#include <util/dir.h>
#include <util/debug.h>

#include <pqi/p3connmgr.h>
#include <pqi/pqinotify.h>

#include <QSettings>
#include <interface/settings.h>

/******
 * #define CONTROL_DEBUG 1
 *****/

ftController::ftController() :mFtActive(false), mInitialLoadDone(false) {}

#define MAX_CONCURRENT_DOWNLOADS_IN_GROUP 2

void ftController::run() {
    while (true) {
        sleep(1);

        if (!mInitialLoadDone) loadSavedTransfers();

        /* Check on our downloadGroups and see if any transfers need to be activated or finished. */
        if (mFtActive) {
            QMutexLocker stack(&ctrlMutex);
            int waiting_to_download, downloading, completed, total;
            foreach (int key, mDownloadGroups.keys()) {
                mDownloadGroups[key].getStatus(&waiting_to_download, &downloading, &completed, &total);

                if (downloading < MAX_CONCURRENT_DOWNLOADS_IN_GROUP && waiting_to_download > 0) {
                    mDownloadGroups[key].startOneTransfer();
                }

                if (completed == total && !mDownloadGroups[key].downloadFinished) {
                    finishGroup(key);
                }
            }
        }

        /* Tick the active transferModules, i.e. send the requests for downloads */
        if (mFtActive) {
            QMutexLocker stack(&ctrlMutex);
            foreach (ftTransferModule* transfer, mDownloads.values()){
                if (transfer->transferStatus() == ftTransferModule::FILE_DOWNLOADING) {
                    transfer->tick();
                }
            }
            if (offLMList) offLMList->tick();
        }
    }
}

void ftController::loadSavedTransfers() {
    QMutexLocker stack(&ctrlMutex);
    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Transfers");
    foreach (QString key, saved.childGroups()) {

        saved.beginGroup(key);

        int friend_id = saved.value("friend_id").toInt();
        QString title = saved.value("title").toString();

        unsigned int source_type = saved.value("source_type").toInt();
        QString source_id = saved.value("source_id").toString();
        downloadGroup::DownloadType download_type = (downloadGroup::DownloadType) saved.value("download_type").toInt();

        saved.beginGroup("Files");

        QStringList paths;
        QStringList hashes;
        QList<qlonglong> filesizes;
        foreach (QString hash, saved.childGroups()) {
            saved.beginGroup(hash);
            hashes.append(hash);
            paths.append(saved.value("path").toString());
            filesizes.append(saved.value("filesize").toLongLong());
            saved.endGroup(); //hash
        }
        saved.endGroup(); //Files

        saved.endGroup(); //key

        internalDownloadFiles(friend_id, title, paths, hashes, filesizes, key.toInt(), download_type, source_type, source_id);
    }
    saved.endGroup(); //Transfers

    mInitialLoadDone = true;
}

void ftController::finishGroup(int groupKey) {
    mDownloadGroups[groupKey].downloadFinished = true;

    bool success = false;
    if (mDownloadGroups[groupKey].downloadType == downloadGroup::DOWNLOAD_NORMAL || mDownloadGroups[groupKey].downloadType == downloadGroup::DOWNLOAD_BORROW) {
        success = moveFilesToDownloadPath(groupKey);
    } else if (mDownloadGroups[groupKey].downloadType == downloadGroup::DOWNLOAD_RETURN) {
        success = moveLentFilesToOriginalPaths(groupKey);
    }

    /* We leave the group in mDownloadGroups but remove from saved so that completed items continue to display until cleared but don't display on quit and restart. */
    /* Note that we have left the ftTransferModules in place undeleted and linked in its downloadGroup in mDownloadGroups, so that we can continue to query them for display
       (though these same ftTransferModule have already been removed from mDownloads where applicable) */
    removeGroupFromSavedTransfers(groupKey);

    if (success) {
        if (mDownloadGroups[groupKey].downloadType == downloadGroup::DOWNLOAD_BORROW) {
            borrowManager->addBorrowed(mDownloadGroups[groupKey].friend_id, mDownloadGroups[groupKey].source_type, mDownloadGroups[groupKey].source_id, mDownloadGroups[groupKey].title);
        } else if (mDownloadGroups[groupKey].downloadType == downloadGroup::DOWNLOAD_RETURN) {
            mixologyService->sendBorrowedReturned(mDownloadGroups[groupKey].friend_id, mDownloadGroups[groupKey].source_type, mDownloadGroups[groupKey].source_id);
        }
    }

    getPqiNotify()->AddPopupMessage(POPUP_DOWNDONE, mDownloadGroups[groupKey].title, "has finished downloading");
    log(LOG_WARNING, FTCONTROLLERZONE, "Finished downloading " + mDownloadGroups[groupKey].title);
}

bool ftController::moveFilesToDownloadPath(int groupKey) {
    /* First create the target directory where we will put the files. */
    mDownloadGroups[groupKey].finalDestination = DirUtil::createUniqueDirectory(files->getDownloadDirectory() + QDir::separator() + mDownloadGroups[groupKey].title);
    if (mDownloadGroups[groupKey].finalDestination.isEmpty()){
        /* If the group's title has an illegal character for the local OS/filesystem, this should get around that problem. */
        mDownloadGroups[groupKey].finalDestination = DirUtil::createUniqueDirectory(files->getDownloadDirectory() + QDir::separator() + "Incoming");
        if (mDownloadGroups[groupKey].finalDestination.isEmpty()){
            getPqiNotify()->AddSysMessage(0, SYS_WARNING, "Transfer Completion Error", "Unable to create folder to put downloaded files into");
            log(LOG_ERROR, FTCONTROLLERZONE, "Unable to create folder to put downloaded files into");
            return false;
        }
    }

    /* Now move the files if they aren't being used as part of any other transfer, or copy them if they are. */
    bool success;
    for (int i = 0; i < mDownloadGroups[groupKey].filesInGroup.count(); i++) {
        QString currentLocation = mDownloadGroups[groupKey].filesInGroup[i]->mFileCreator->getPath();
        QString finalLocation = mDownloadGroups[groupKey].finalDestination + QDir::separator() + mDownloadGroups[groupKey].filenames[i];
        if (inActiveDownloadGroup(mDownloadGroups[groupKey].filesInGroup[i])) {
            if (!DirUtil::copyFile(currentLocation, finalLocation)) {
                getPqiNotify()->AddSysMessage(0, SYS_WARNING, "File copy error", "Error while copying file " + finalLocation + " from temporary location " + currentLocation);
                log(LOG_ERROR, FTCONTROLLERZONE, "Error while copying file " + finalLocation + " from temporary location " + currentLocation);
                success = false;
            }
        } else {
            if (!DirUtil::moveFile(currentLocation, finalLocation)) {
                getPqiNotify()->AddSysMessage(0, SYS_WARNING, "File move error", "Error while moving file " + finalLocation + " from temporary location " + currentLocation);
                log(LOG_ERROR, FTCONTROLLERZONE, "Error while moving file " + finalLocation + " from temporary location " + currentLocation);
                success = false;
            }
            /* We only remove from mDownloads if no other downloadGroup needs it still. */
            mDownloads.remove(mDownloadGroups[groupKey].filesInGroup[i]->mFileCreator->getHash());
        }
    }

    return success;
}

bool ftController::moveLentFilesToOriginalPaths(int groupKey) {
    /* Build the argument lists for the returnBorrowedFiles functions. */
    QStringList paths;
    QStringList hashes;
    QList<qlonglong> filesizes;
    QList<bool> moveFile;

    for (int i = 0; i < mDownloadGroups[groupKey].filesInGroup.count(); i++) {
        paths.append(mDownloadGroups[groupKey].filesInGroup[i]->mFileCreator->getPath());
        hashes.append(mDownloadGroups[groupKey].filesInGroup[i]->mFileCreator->getHash());
        filesizes.append(mDownloadGroups[groupKey].filesInGroup[i]->mFileCreator->getFileSize());
        if (inActiveDownloadGroup(mDownloadGroups[groupKey].filesInGroup[i])) {
            moveFile.append(false);
        } else {
            moveFile.append(true);
            /* We only remove from mDownloads if no other downloadGroup needs it still. */
            mDownloads.remove(mDownloadGroups[groupKey].filesInGroup[i]->mFileCreator->getHash());
        }
    }

    /* Delegate actual work to the file providers that know how to reset their own internal states. */
    int result = 0;
    if (mDownloadGroups[groupKey].source_type & FILE_HINTS_ITEM) {
        result = librarymixermanager->returnBorrowedFiles(mDownloadGroups[groupKey].friend_id, mDownloadGroups[groupKey].source_id,
                                                          paths, hashes, filesizes, moveFile, mDownloadGroups[groupKey].finalDestination);
    } else if (mDownloadGroups[groupKey].source_type & FILE_HINTS_OFF_LM) {
        if (offLMList) result = offLMList->returnBorrowedFiles(mDownloadGroups[groupKey].friend_id, mDownloadGroups[groupKey].source_id,
                                                               paths, hashes, filesizes, moveFile, mDownloadGroups[groupKey].finalDestination);
    }

    if (result == 1) {
        return true;
    } else if (result == 0) {
        /* This is the worst case scenario: returnBorrowedFiles will return this if some files were successfully returned and some were not.
           The returnBorrowedFiles functions will already have notified the GUI of this problem, it's on the user to fix it now. */
        return false;
    }

    /* All of these below are failure states that we will notify the user of, and then simply place the files in the downloads directory. */
    if (result == -1) {
        getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                      "The Mixologist",
                                      "The number of files you were returned by " +
                                      peers->getPeerName(mDownloadGroups[groupKey].friend_id) +
                                      " for " +
                                      mDownloadGroups[groupKey].title +
                                      " is not the same as the number of files you lent.\n" +
                                      "All files have been left in your downloads folder.");
    } else if (result == -2) {
        bool plural = (mDownloadGroups[groupKey].filesInGroup.count() > 1);
        if (plural) {
            getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                          "The Mixologist",
                                          "The files you were returned by " +
                                          peers->getPeerName(mDownloadGroups[groupKey].friend_id) +
                                          " for " +
                                          mDownloadGroups[groupKey].title +
                                          " have been altered and are no longer exactly the same.\n" +
                                          "All files have been left in your downloads folder.");
        } else {
            getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                          "The Mixologist",
                                          "The file you were returned by " +
                                          peers->getPeerName(mDownloadGroups[groupKey].friend_id) +
                                          " for " +
                                          mDownloadGroups[groupKey].title +
                                          " has been altered and is no longer exactly the same.\n" +
                                          "The file has been left in your downloads folder.");
        }
    } else if (result == -3) {
        getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                      "The Mixologist",
                                      "You were returned " +
                                      mDownloadGroups[groupKey].title +
                                      " by " +
                                      peers->getPeerName(mDownloadGroups[groupKey].friend_id) +
                                      " but you have recently removed it from your library.\n" +
                                      "All files have been left in your downloads folder.");
    } else {
        getPqiNotify()->AddSysMessage(0, SYS_WARNING,
                                      "The Mixologist",
                                      "You were returned " +
                                      mDownloadGroups[groupKey].title +
                                      " by " +
                                      peers->getPeerName(mDownloadGroups[groupKey].friend_id) +
                                      " but it wasn't lent to " +
                                      peers->getPeerName(mDownloadGroups[groupKey].friend_id) +
                                      ".\n\n" +
                                      "All files have been left in your downloads folder.");
    }

    /* Even if the files were not moved into original location, as long as they were moved out of the temporary folder,
       this is considered successful enough to return true. */
    return moveFilesToDownloadPath(groupKey);
}

bool ftController::inMultipleDownloadGroups(ftTransferModule *file) const {
    int found = 0;
    foreach (downloadGroup group, mDownloadGroups.values()) {
        if (group.filesInGroup.contains(file)) {
            found++;
            if (found >= 2) return true;
        }
    }
    return false;
}

bool ftController::inActiveDownloadGroup(ftTransferModule *file) const {
    foreach (downloadGroup group, mDownloadGroups.values()) {
        if (!group.downloadFinished &&
            group.filesInGroup.contains(file)) return true;
    }
    return false;
}

void ftController::activate() {
    mFtActive = true;
}

/***************************************************************/
/********************** Controller Access **********************/
/***************************************************************/

bool ftController::downloadFiles(unsigned int friend_id, const QString &title, const QStringList &paths, const QStringList &hashes, const QList<qlonglong> &filesizes) {
    QMutexLocker stack(&ctrlMutex);
    return internalDownloadFiles(friend_id, title, paths, hashes, filesizes, -1, downloadGroup::DOWNLOAD_NORMAL, 0, "");
}

bool ftController::borrowFiles(unsigned int friend_id, const QString &title, const QStringList &paths, const QStringList &hashes, const QList<qlonglong> &filesizes,
                               uint32_t source_type, QString source_id) {
    QMutexLocker stack(&ctrlMutex);
    return internalDownloadFiles(friend_id, title, paths, hashes, filesizes, -1, downloadGroup::DOWNLOAD_BORROW, source_type, source_id);
}

bool ftController::downloadBorrowedFiles(unsigned int friend_id, uint32_t source_type, QString source_id, const QStringList &paths, const QStringList &hashes, const QList<qlonglong> &filesizes) {
    QString title;
    if (source_type & FILE_HINTS_ITEM) {
        LibraryMixerItem* originalItem = librarymixermanager->getLibraryMixerItem(source_id.toInt());
        if (originalItem == NULL || originalItem->lentTo() != friend_id) return false;
        title = originalItem->title();
    } else if (source_type & FILE_HINTS_OFF_LM) {
        if (!offLMList) return false;
        OffLMShareItem* originalItem = offLMList->getOffLMShareItem(source_id);
        if (!originalItem || originalItem->lent() != friend_id) return false;
        title = originalItem->label();
    } else return false;

    QMutexLocker stack(&ctrlMutex);
    return internalDownloadFiles(friend_id, "Return of " + title, paths, hashes, filesizes, -1, downloadGroup::DOWNLOAD_RETURN, source_type, source_id);
}

bool ftController::internalDownloadFiles(unsigned int friend_id, const QString &title, const QStringList &paths, const QStringList &hashes, const QList<qlonglong> &filesizes,
                                         int specificKey, downloadGroup::DownloadType download_type, unsigned int source_type, const QString &source_id) {
    /* Basic check for well-formed request. */
    if (paths.size() != hashes.size() || paths.size() != filesizes.size() || paths.size() < 1 ||
        title.isEmpty() ||
        friend_id < 1)
        return false;

    downloadGroup newGroup;
    newGroup.friend_id = friend_id;
    newGroup.title = title;
    newGroup.filenames = paths;
    newGroup.downloadType = download_type;
    newGroup.source_type = source_type;
    newGroup.source_id = source_id;
    /* Now we must construct an ftTransferModule for each of the files.
       Note that the order is preserved between newGroup.filenames and newGroup.files. */
    for (int i = 0; i < paths.size(); i++) {
        ftTransferModule* file = internalRequestFile(friend_id, hashes[i], filesizes[i]);
        /* If one of the file requests has failed, abort the whole thing and clear already created file requests. */
        if (file == NULL) goto failureDeleteAllNewModules;
        newGroup.filesInGroup.append(file);
    }

    /* The int that serves as the key to mDownloadGroups is arbitrary.
       We only need it in order to provide a unique address for each downloadGroup.
       Therefore, we start from 0 and we can safely just add one to the last key to get a new unique key.
       The only potential duplication is caused by integer looping, which can't realistically happen. */
    int newKey;
    if (specificKey != -1) newKey = specificKey;
    else if (mDownloadGroups.count() == 0) newKey = 0;
    else newKey = mDownloadGroups.keys().last() + 1;

    if (mDownloadGroups.contains(newKey)) goto failureDeleteAllNewModules;
    mDownloadGroups.insert(newKey, newGroup);
    addGroupToSavedTransfers(newKey, newGroup);

    return true;

    failureDeleteAllNewModules:
    log(LOG_WARNING, FTCONTROLLERZONE, "Error initializing download of " + title);
    foreach (ftTransferModule* currentFile, newGroup.filesInGroup) {
        mDownloads.remove(currentFile->mFileCreator->getHash());
        delete currentFile;
    }
    return false;
}

ftTransferModule* ftController::internalRequestFile(unsigned int friend_id, const QString &hash, uint64_t size) {
    if (hash.isEmpty()) return false;

    if (mDownloads.contains(hash)) return mDownloads[hash];

    log(LOG_DEBUG_ALERT, FTCONTROLLERZONE, "Beginning download for " + hash);

    ftTransferModule* file = new ftTransferModule(friend_id, size, hash);
    setPeerState(file, friend_id, connMgr->isOnline(friend_id));

    mDownloads[hash] = file;

    return file;
}

void ftController::cancelDownloadGroup(int groupId) {
    QMutexLocker stack(&ctrlMutex);
    foreach(ftTransferModule* file, mDownloadGroups[groupId].filesInGroup) {
        internalCancelFile(groupId, file);
    }
    mDownloadGroups.remove(groupId);
    removeGroupFromSavedTransfers(groupId);
}

void ftController::cancelFile(int groupId, const QString &hash) {
    QMutexLocker stack(&ctrlMutex);
    internalCancelFile(groupId, mDownloads[hash]);
    //If we just removed the last file from a group, the whole thing should be removed.
    if (mDownloadGroups[groupId].filesInGroup.count() == 0) {
        mDownloadGroups.remove(groupId);
        removeGroupFromSavedTransfers(groupId);
    } else {
        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.remove("Transfers/" + QString::number(groupId) + "/Files/" + hash);
    }
}

void ftController::internalCancelFile(int groupId, ftTransferModule *file) {
    /* If this file is still part of more than one transfer, we need to keep it around because cancellations only affect one downloadGroup.
       Otherwise we clean it up by removing the partial file from the disk,
       removing the ftTransferModule from mDownloads and its downloadGroup and freeing its memory. */
    if (!inMultipleDownloadGroups(file)) {
        file->mFileCreator->deleteFileFromDisk();

        mDownloads.remove(file->mFileCreator->getHash());

        int index = mDownloadGroups[groupId].filesInGroup.indexOf(file);
        mDownloadGroups[groupId].filesInGroup.removeAt(index);
        mDownloadGroups[groupId].filenames.removeAt(index);

        delete file;
    }
}

bool ftController::controlFile(int orig_item_id, QString hash, uint32_t flags) {
    (void) orig_item_id;
    (void) hash;
    (void) flags;
    return true;
}

void ftController::clearCompletedFiles() {
    log(LOG_DEBUG_BASIC, FTCONTROLLERZONE, "ftController.clearCompletedFiles - clearing all completed transfers");
    QMutexLocker stack(&ctrlMutex);
    //Find all downloadGroups that are fully completed
    //Remove the ones that are from mDownloadGroups
    foreach (int key, mDownloadGroups.keys()) {
        if (mDownloadGroups[key].downloadFinished) {
            foreach (ftTransferModule* file, mDownloadGroups[key].filesInGroup) {
                if (!inMultipleDownloadGroups(file)) {
                    mDownloads.remove(file->mFileCreator->getHash());
                    delete file;
                }
            }

            mDownloadGroups.remove(key);
        }
    }
}

/* get Details of File Transfers */
void ftController::FileDownloads(QList<downloadGroupInfo> &downloads) {
    QMutexLocker stack(&ctrlMutex);

    foreach (int key, mDownloadGroups.keys()) {
        downloadGroupInfo groupInfo;
        groupInfo.title = mDownloadGroups[key].title;
        groupInfo.groupId = key;
        groupInfo.finalDestination = mDownloadGroups[key].finalDestination;
        groupInfo.filenames = mDownloadGroups[key].filenames;
        foreach (ftTransferModule* file, mDownloadGroups[key].filesInGroup) {
            downloadFileInfo fileInfo;
            fileDetails(file, fileInfo);
            groupInfo.filesInGroup.append(fileInfo);
        }
        downloads.append(groupInfo);
    }
}

void ftController::fileDetails(ftTransferModule* file, downloadFileInfo &info) {
    info.hash = file->mFileCreator->getHash();
    info.totalSize = file->mFileCreator->getFileSize();
    info.downloadedSize = file->mFileCreator->amountReceived();

    QList<unsigned int> sourceIds;
    file->getFileSources(sourceIds);
    info.totalTransferRate = 0;
    uint32_t indivTransferRate = 0;
    uint32_t indivState = 0;

    for (int i = 0; i < sourceIds.size(); i++) {
        if (file->getPeerState(sourceIds[i], indivState, indivTransferRate)) {
            TransferInfo ti;
            switch (indivState) {
                case PQIPEER_NOT_ONLINE:
                    ti.status = FT_STATE_WAITING;
                    break;
                case PQIPEER_DOWNLOADING:
                    ti.status = FT_STATE_TRANSFERRING;
                    break;
                case PQIPEER_ONLINE_IDLE:
                    ti.status = FT_STATE_ONLINE_IDLE;
                    break;
                default:
                    ti.status = FT_STATE_WAITING;
                    break;
            }

            ti.transferRate = indivTransferRate / 1024.0;
            ti.librarymixer_id = sourceIds[i];
            info.peers.push_back(ti);
            info.totalTransferRate += indivTransferRate / 1024.0;
        }
    }

    switch (file->transferStatus()) {
    case ftTransferModule::FILE_WAITING:
        info.downloadStatus = FT_STATE_WAITING;
        break;
    case ftTransferModule::FILE_COMPLETE:
        info.downloadStatus = FT_STATE_COMPLETE;
        break;
    case ftTransferModule::FILE_DOWNLOADING:
        info.downloadStatus = FT_STATE_TRANSFERRING;
        break;
    }

}

void ftController::addGroupToSavedTransfers(int groupKey, const downloadGroup &group) {
    removeGroupFromSavedTransfers(groupKey);

    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Transfers");
    saved.beginGroup(QString::number(groupKey));
    saved.setValue("friend_id", group.friend_id);
    saved.setValue("title", group.title);
    saved.setValue("download_type", group.downloadType);
    saved.setValue("source_type", group.source_type);
    saved.setValue("source_id", group.source_id);

    saved.beginGroup("Files");
    for (int i = 0; i < group.filesInGroup.count(); i++) {
        saved.beginGroup(group.filesInGroup[i]->mFileCreator->getHash());
        saved.setValue("path", group.filenames[i]);
        saved.setValue("filesize", QString::number(group.filesInGroup[i]->mFileCreator->getFileSize()));
        saved.endGroup(); //hashes[i]
    }
    saved.endGroup(); //Files
    saved.endGroup(); //Transfers
}

void ftController::removeGroupFromSavedTransfers(int groupKey) {
    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Transfers");
    saved.remove(QString::number(groupKey));
    saved.endGroup(); //Transfers
}

/* Directory Handling */
bool ftController::setDownloadDirectory(QString path) {
    if (DirUtil::checkCreateDirectory(path)) {
        QMutexLocker stack(&ctrlMutex);
        if (mDownloadPath != path){
            mDownloadPath = path;
            QSettings settings(*mainSettings, QSettings::IniFormat);
            settings.setValue("Transfers/DownloadsDirectory", mDownloadPath);
        }
        return true;
    }
    return false;
}

bool ftController::loadOrSetDownloadDirectory(QString path) {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    return setDownloadDirectory(settings.value("Transfers/DownloadsDirectory", path).toString());
}

bool ftController::setPartialsDirectory(QString path) {
    if (DirUtil::checkCreateDirectory(path)) {
        QMutexLocker stack(&ctrlMutex);
        if (mPartialsPath != path){
            mPartialsPath = path;
            QSettings settings(*mainSettings, QSettings::IniFormat);
            settings.setValue("Transfers/PartialsDirectory", mPartialsPath);

            /* move all existing files! */
            foreach (ftTransferModule* file, mDownloads.values()) {
                file->mFileCreator->moveFile(mPartialsPath);
            }
        }
        return true;
    }
    return false;
}

bool ftController::loadOrSetPartialsDirectory(QString path) {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    return setPartialsDirectory(settings.value("Transfers/PartialsDirectory", path).toString());
}

QString ftController::getDownloadDirectory() const {
    return mDownloadPath;
}

QString ftController::getPartialsDirectory() const {
    return mPartialsPath;
}

bool ftController::handleReceiveData(const std::string &peerId, const QString &hash, uint64_t offset, uint32_t chunksize, void *data) {
    QMutexLocker stack(&ctrlMutex);
    QMap<QString, ftTransferModule*>::const_iterator it;
    it = mDownloads.find(hash);
    if (it != mDownloads.end()) {
        it.value()->recvFileData(peerId, offset, chunksize, data);
        return true;
    } else if (offLMList) {
        /* If it's not for any of our files, see if it might be for ftOffLMList's Xmls. */
        return offLMList->handleReceiveData(peerId, hash, offset, chunksize, data);
    }
    return false;
}

/***************************************************************/
/********************** Controller Access **********************/
/***************************************************************/

void ftController::statusChange(const std::list<pqipeer> &changeList) {
    log(LOG_DEBUG_BASIC, FTCONTROLLERZONE, "ftController.statusChange");
    QMutexLocker stack(&ctrlMutex);

    /* add online to all downloads */
    QMap<QString, ftTransferModule*>::const_iterator it;
    std::list<pqipeer>::const_iterator changeListIt;

    for (it = mDownloads.begin(); it != mDownloads.end(); it++) {
        for (changeListIt = changeList.begin(); changeListIt != changeList.end(); changeListIt++) {
            if (changeListIt->actions & PEER_CONNECTED) {
                setPeerState(it.value(), changeListIt->librarymixer_id, true);
            } else {
                setPeerState(it.value(), changeListIt->librarymixer_id, false);
            }
        }
    }

    /* Also pass through information to the ftOffLMList for its XML transfer modules. */
    if (offLMList) offLMList->statusChange(changeList);
}

bool ftController::setPeerState(ftTransferModule *tm, unsigned int librarymixer_id, bool online) {
    if (tm != NULL){
        if (librarymixer_id == peers->getOwnLibraryMixerId()) {
            tm->setPeerState(librarymixer_id, PQIPEER_ONLINE_IDLE);
        } else if (online) {
            tm->setPeerState(librarymixer_id, PQIPEER_ONLINE_IDLE);
        } else {
            tm->setPeerState(librarymixer_id, PQIPEER_NOT_ONLINE);
        }
    }
    return true;
}

/***************************************************************/
/********************** downloadGroup **************************/
/***************************************************************/

void downloadGroup::getStatus(int *waiting_to_download, int *downloading, int *completed, int *total) const {
    *waiting_to_download = 0;
    *downloading = 0;
    *completed = 0;
    *total = 0;
    foreach (ftTransferModule* file, filesInGroup) {
        (*total)++;
        switch (file->transferStatus()) {
        case ftTransferModule::FILE_WAITING:
            (*waiting_to_download)++;
            break;
        case ftTransferModule::FILE_DOWNLOADING:
            (*downloading)++;
            break;
        case ftTransferModule::FILE_COMPLETE:
            (*completed)++;
            break;
        }
    }
}

void downloadGroup::startOneTransfer() {
    for (int i = 0; i < filesInGroup.count(); i++) {
        if (filesInGroup[i]->transferStatus() == ftTransferModule::FILE_WAITING) {
            filesInGroup[i]->transferStatus(ftTransferModule::FILE_DOWNLOADING);
            return;
        }
    }
}
