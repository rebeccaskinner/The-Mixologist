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

#include "ft/ftcontroller.h"

#include "ft/ftfilecreator.h"
#include "ft/fttransfermodule.h"
#include "ft/ftsearch.h"
#include "ft/ftdatamultiplex.h"
#include "ft/ftitemlist.h"
#include "ft/ftserver.h"

#include "interface/peers.h"

#include "util/dir.h"
#include "util/debug.h"


#include "pqi/p3connmgr.h"
#include "pqi/pqinotify.h"

#include <stdio.h>

#include <QSettings>
#include <interface/settings.h>

/******
 * #define CONTROL_DEBUG 1
 *****/

/* Format of settings
//a hash
    Transfers
        //iniEncode of (an orig_item_id with filename appended)
            Orig_item_id = the item id
            Librarymixer_name = a library mixer name
            Filename = a file name (possibly including subdirectories)
        //another orig_item_id
    Sources
        //a source id = ""
        //another source = ""
    Filesize = filesize
//another hash
*/

ftController::ftController(ftDataMultiplex *dm)
    :mDataplex(dm), mFtActive(false) {
    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Files");
    QStringList fileTransfers = saved.childGroups();
    for (int i = 0; i < fileTransfers.size(); i++) {
        QList<int> orig_item_ids;
        QStringList librarymixer_names;
        QStringList filenames;
        QList<int> sourceIds;

        saved.beginGroup(fileTransfers[i]);

        int filesize = saved.value("Filesize").toInt();

        saved.beginGroup("Transfers");
        QStringList librarymixerTransfers = saved.childGroups();
        for (int j = 0; j < librarymixerTransfers.size(); j++) {
            saved.beginGroup(librarymixerTransfers[j]);
            orig_item_ids.append(saved.value("Orig_item_id").toInt());
            librarymixer_names.append(saved.value("Librarymixer_name").toString());
            filenames.append(saved.value("Filename").toString());
            saved.endGroup(); //librarymixerTransfers[j]
        }
        saved.endGroup(); //Transfers

        saved.beginGroup("Sources");
        QStringList raw_sources = saved.childKeys();
        //convert source ids to int
        for (int j = 0; j < raw_sources.size(); j++) {
            sourceIds.append(raw_sources[j].toInt());
        }
        saved.endGroup(); //sources

        saved.endGroup(); //fileTransfers[i]

        requestFile(librarymixer_names, orig_item_ids, filenames, fileTransfers[i].toStdString(),
                    filesize, 0, sourceIds);
    }
}

void ftController::setFtSearchNItem(ftSearch *search, ftItemList *list) {
    mSearch = search;
    mItemList = list;
}

void ftController::run() {
    /* check the queues */
    while (true) {
#ifdef WIN32
        Sleep(1000);
#else
        sleep(1);
#endif

        //Initial items to load
        bool doPending = false;
        {
            MixStackMutex stack(ctrlMutex); /******* LOCKED ********/
            doPending = (mFtActive) && (!mFtPendingDone);
        }

        if (doPending) {
            if (!handleAPendingRequest()) {
                MixStackMutex stack(ctrlMutex); /******* LOCKED ********/
                mFtPendingDone = true;
            }
        }

        /* tick the transferModules */
        {
            MixStackMutex stack(ctrlMutex); /******* LOCKED ********/
            QMap<std::string, ftFileControl>::iterator it;
            for (it = mDownloads.begin(); it != mDownloads.end(); it++) {
                if (it.value().mState == ftFileControl::DOWNLOADING) {
                    it.value().mTransfer->tick();
                } else if (it.value().mState == ftFileControl::COMPLETED_CHECK) {
                    finishFile(&(it.value()));
                    break;
                }
            }
        }
    }

}

//Not mutex protected, called by transfer module's tick by our tick from within mutex
void ftController::FlagFileComplete(std::string hash) {
    mDownloads[hash].mState = ftFileControl::COMPLETED_CHECK;
}

//No mutex protection, call from within mutex
void ftController::finishFile(ftFileControl *fc) {
    log(LOG_DEBUG_ALERT, FTCONTROLLERZONE, "ftController.finishFile on " + fc->filenames.first());
    if ((fc->mCreator && !fc->mCreator->finished()) ||
            (fc->mTransfer && fc->mState == ftFileControl::DOWNLOADING)) {
        log(LOG_ERROR, FTCONTROLLERZONE, "Tried to close out a transfer on a file that was not done! " +
            QString::number(fc->mCreator->getRecvd()) + "/" + QString::number(fc->mCreator->getFileSize()));
        return;
    }
    if (fc->mTransfer) {
        delete fc->mTransfer;
        fc->mTransfer = NULL;
    }

    if (fc->mCreator) {
        delete fc->mCreator;
        fc->mCreator = NULL;
    }
    mDataplex->removeTransferModule(fc->mHash);

    /* If all transfers with same orig_item_id are done, then complete them.
       Otherwise, mark this one as completed and waiting for the rest to finish. */
    /* We need to clone this, because if there is more than one item id, and an id
       has completeTransfer called on it, completeTransfer will remove that id from
       the fc->orig_item_ids list. */
    QList<int> item_ids = fc->orig_item_ids;
    for (int item_index = 0; item_index < item_ids.size(); item_index++) {
        if (checkTransferComplete(item_ids[item_index])) {
            completeTransfer(item_ids[item_index]);
        } else {
            fc->mState = ftFileControl::COMPLETED_WAIT;
        }
    }
}

//No mutex protection, call from within mutex
bool ftController::checkTransferComplete(int orig_item_id){
    bool found = false;
    QMap<std::string, ftFileControl>::const_iterator file_it;
    for (file_it = mDownloads.begin(); file_it != mDownloads.end(); file_it++) {
        if (file_it->orig_item_ids.contains(orig_item_id)){
            found = true;
            if (file_it->mState == ftFileControl::DOWNLOADING) return false;
        }
    }
    return found;
}

//No mutex protection, call from within mutex
void ftController::completeTransfer(int orig_item_id) {
    QString notificationName(""); //The name of the transfer being completed, to be passed up to notify
    QString finalDestination(""); //The root path to where the files will go
    QString createdDirectory(""); //If a directory is created, it will be noted here

    //These lists are needed if this item was lent in order to put the files back.
    QStringList item_filenames;
    QStringList item_hashes;
    QList<unsigned long> item_filesizes;

    QMap<std::string, ftFileControl>::iterator it = mDownloads.end();
    while(it != mDownloads.begin()){
        it--;
        //Step through each file's list of places it needs to go
        int index = it.value().orig_item_ids.size();
        while (index > 0){
            index--;
            if (it.value().orig_item_ids[index] == orig_item_id) {

                //If this is the first file we found for the Item, then we need to initialize notificationname and finalDestination
                if (notificationName.isEmpty()){
                    notificationName = it.value().librarymixer_names.at(index);
                }
                if (finalDestination.isEmpty()){
                    finalDestination = DirUtil::createUniqueDirectory(files->getDownloadDirectory() + QDir::separator() +
                                                                      it.value().librarymixer_names.at(index));
                    if (finalDestination.isEmpty()){
                        getPqiNotify()->AddSysMessage(0, SYS_WARNING, "Transfer Completion Error", "Unable to create folder to put downloaded files into");
                        log(LOG_ERROR, FTCONTROLLERZONE, "Unable to create folder to put downloaded files into");
                        it.value().mState = ftFileControl::COMPLETED_ERROR;
                        //Does this need to look more like the file errors below? They do stuff after the error.
                        //At any rate, this should be extraordinarily rare, so making this better can wait.
                        continue;
                    }
                    createdDirectory = finalDestination;
                }
                QString fileDestination = finalDestination + it.value().filenames[index];

                item_hashes.append(it.value().mHash.c_str());
                item_filesizes.append(it.value().mSize);
                item_filenames.append(fileDestination);

                /* If we only have one set of destination information, then move from mDownloads, and move the file.*/
                if (it.value().orig_item_ids.size() == 1) {
                    it.value().mState = ftFileControl::COMPLETED;

                    //Empty files are a special case, because we don't actually download anything, so there's nothing to copy.
                    if (it.value().mSize == 0) DirUtil::createEmptyFile(fileDestination);
                    //Otherwise, if not an empty file, move it over
                    else {
                        if (!DirUtil::moveFile(it.value().tempDestination(), fileDestination)) {
                            getPqiNotify()->AddSysMessage(0, SYS_WARNING, "File move error", "Error while moving file " + fileDestination + " from temporary location " + it.value().tempDestination());
                            log(LOG_ERROR, FTCONTROLLERZONE, "Error while moving file " + fileDestination + " from temporary location " + it.value().tempDestination());
                            it.value().mState = ftFileControl::COMPLETED_ERROR;
                        }
                    }

                    if (mCompleted.contains(it.value().mHash)) {
                        mCompleted[it.value().mHash].librarymixer_names.append(it.value().librarymixer_names.first());
                        mCompleted[it.value().mHash].filenames.append(it.value().filenames.first());
                        mCompleted[it.value().mHash].orig_item_ids.append(it.value().orig_item_ids.first());
                    } else {
                        mCompleted[it.value().mHash] = it.value();
                    }

                    QSettings saved(*savedTransfers, QSettings::IniFormat);
                    saved.beginGroup("Files");
                    saved.remove(it.value().mHash.c_str());
                    saved.sync();

                    it = mDownloads.erase(it);
                    break; //Step out of the inner for loop and onto the next it
                /* If we have more than one set, then move only one set of destination information from mDownloads, and copy the file. */
                } else {
                    //Empty files are a special case, because we don't actually download anything, so there's nothing to copy.
                    if (it.value().mSize == 0) DirUtil::createEmptyFile(fileDestination);
                    //Otherwise, if not an empty file, copy it over
                    else {
                        if (!DirUtil::copyFile(it.value().tempDestination(), fileDestination)) {
                            getPqiNotify()->AddSysMessage(0, SYS_WARNING, "File copy error", "Error while copying file " + fileDestination + " from temporary location " + it.value().tempDestination());
                            log(LOG_ERROR, FTCONTROLLERZONE, "Error while copying file " + fileDestination + " from temporary location " + it.value().tempDestination());
                        }
                    }
                    /* If mCompleted doesn't have this hash file yet, then add it in, but we clear out the transfer info,
                       so that we cana add in just this one, since we are only marking one transfer done at this time */
                    if (!mCompleted.contains(it.value().mHash)) {
                        mCompleted[it.value().mHash] = it.value();
                        mCompleted[it.value().mHash].mState = ftFileControl::COMPLETED;
                        mCompleted[it.value().mHash].librarymixer_names.clear();
                        mCompleted[it.value().mHash].filenames.clear();
                        mCompleted[it.value().mHash].orig_item_ids.clear();
                    }

                    //Take care of updating saved transfers before we mess with variables.
                    QSettings saved(*savedTransfers, QSettings::IniFormat);
                    saved.beginGroup("Files");
                    saved.beginGroup(it.value().mHash.c_str());
                    saved.beginGroup("Transfers");
                    saved.remove(iniEncode(QString::number(it.value().orig_item_ids[index]) + it.value().filenames[index]));
                    saved.sync();

                    mCompleted[it.value().mHash].librarymixer_names.append(it.value().librarymixer_names.takeAt(index));
                    mCompleted[it.value().mHash].filenames.append(it.value().filenames.takeAt(index));
                    mCompleted[it.value().mHash].orig_item_ids.append(it.value().orig_item_ids.takeAt(index));
                }
            }
        }
    }
    ftserver->completedDownload(orig_item_id, item_filenames, item_hashes, item_filesizes, createdDirectory);
    pqiNotify *notify = getPqiNotify();
    if (notify) notify->AddPopupMessage(POPUP_DOWNDONE, notificationName, "has finished downloading");
    log(LOG_WARNING, FTCONTROLLERZONE, "Finished downloading " + notificationName);
}

/***************************************************************/
/********************** Controller Access **********************/
/***************************************************************/

bool    ftController::activate() {
    MixStackMutex stack(ctrlMutex); /******* LOCKED ********/
    mFtActive = true;
    mFtPendingDone = false;
    return true;
}

bool    ftController::handleAPendingRequest() {
    ftPendingRequest req;
    {
        MixStackMutex stack(ctrlMutex); /******* LOCKED ********/

        if (mPendingRequests.size() < 1) {
            return false;
        }
        req = mPendingRequests.front();
        mPendingRequests.pop_front();
    }
    requestFile(req.librarymixer_name, req.orig_item_id, req.filename, req.mHash, req.mSize, req.mFlags, req.sourceIds);
    return true;
}

bool    ftController::requestFile(QString librarymixer_name, int orig_item_id, QString fname, std::string hash,
                                  uint64_t size, uint32_t flags, QList<int> &sourceIds) {

    if (orig_item_id == 0 || fname.isEmpty() || hash.empty()) return false;

    //If file transfer is not yet activated, save into a pending queue.
    {
        MixStackMutex stack(ctrlMutex); /******* LOCKED ********/
        if (!mFtActive) {
            /* store in pending queue */
            ftPendingRequest req(librarymixer_name, orig_item_id, fname, hash, size, flags, sourceIds);
            mPendingRequests.push_back(req);
            mFtPendingDone = false;
            return true;
        }
    }

    {
        MixStackMutex stack(ctrlMutex); /******* LOCKED ********/

        QMap<std::string, ftFileControl>::iterator download_it;

        /* First check if the file is already being downloaded */
        download_it= mDownloads.find(hash);
        if (download_it != mDownloads.end()) {
            log(LOG_DEBUG_ALERT, FTCONTROLLERZONE, "ftcontroller::requestFile - Already downloading " + fname);

            /* We're already downloading this file, but see if this is a new set of destination information,
               and if it is, add it in, as well as the sources */
            bool found = false;
            for (int i = 0; i < download_it.value().orig_item_ids.size(); i++){
                if (download_it.value().librarymixer_names[i] == librarymixer_name &&
                    download_it.value().orig_item_ids[i] == orig_item_id &&
                    download_it.value().filenames[i] == fname){
                    found = true;
                    break;
                }
            }
            if (!found) {
                QSettings saved(*savedTransfers, QSettings::IniFormat);
                saved.beginGroup("Files");
                saved.beginGroup(hash.c_str());

                saved.beginGroup("Transfers");
                saved.beginGroup(iniEncode(QString::number(orig_item_id) + fname));
                download_it.value().orig_item_ids.append(orig_item_id);
                saved.setValue("Orig_item_id", orig_item_id);
                download_it.value().librarymixer_names.append(librarymixer_name);
                saved.setValue("Librarymixer_name", librarymixer_name);
                download_it.value().filenames.append(fname);
                saved.setValue("Filename", fname);
                saved.endGroup(); //iniEncode
                saved.endGroup(); //Transfers

                saved.beginGroup("Sources");
                for (int i = 0; i < sourceIds.size(); i++) {
                    uint32_t unused1, unused2;
                    if (download_it.value().mTransfer->getPeerState(sourceIds[i], unused1, unused2)) {
                        continue; /* already added peer */
                    }
                    log(LOG_DEBUG_BASIC, FTCONTROLLERZONE, "ftcontroller::requestFile - Adding a new source " + QString::number(sourceIds[i]));
                    download_it.value().mTransfer->addFileSource(sourceIds[i]);
                    setPeerState(download_it.value().mTransfer, sourceIds[i], mConnMgr->isOnline(sourceIds[i]));
                    saved.setValue(QString::number(sourceIds[i]), "");
                }
                saved.endGroup(); //Sources
                saved.sync();

                /* If this transfer is already done and waiting, the addition of
                   a new transfer means we need to check it again. */
                if (download_it.value().mState == ftFileControl::COMPLETED_WAIT)
                    download_it.value().mState = ftFileControl::COMPLETED_CHECK;
            }
            return true;
        }
    } /******* UNLOCKED ********/

    /* add in new item for download */

    log(LOG_WARNING, FTCONTROLLERZONE, "Beginning request for " + fname);

    MixStackMutex stack(ctrlMutex); /******* LOCKED ********/
    ftFileControl ftfc(size, hash, flags);
    ftfc.librarymixer_names.append(librarymixer_name);
    ftfc.orig_item_ids.append(orig_item_id);
    ftfc.filenames.append(fname);

    ftfc.mCreator = new ftFileCreator(ftfc.tempDestination(), size, hash);
    ftfc.mTransfer = new ftTransferModule(ftfc.mCreator, mDataplex, this);

    /* add to ClientModule */
    mDataplex->addTransferModule(ftfc.mTransfer, ftfc.mCreator);

    /* now add source peers (and their current state) */
    ftfc.mTransfer->setFileSources(sourceIds);

    /* get current state for transfer module */
    for (int i = 0; i < sourceIds.size(); i++) {
        log(LOG_DEBUG_BASIC, FTCONTROLLERZONE, "ftcontroller::requestFile - Adding a source to a new request " + QString::number(sourceIds[i]));
        setPeerState(ftfc.mTransfer, sourceIds[i], mConnMgr->isOnline(sourceIds[i]));
    }

    mDownloads[hash] = ftfc;

    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Files");
    saved.beginGroup(hash.c_str());
    saved.setValue("Filesize", size);
    saved.beginGroup("Transfers");
    saved.beginGroup(iniEncode(QString::number(orig_item_id) + fname));
    saved.setValue("Orig_item_id", orig_item_id);
    saved.setValue("Librarymixer_name", librarymixer_name);
    saved.setValue("Filename", fname);
    saved.endGroup(); //QString::number(orig_item_id)
    saved.endGroup(); //Transfers
    saved.beginGroup("Sources");
    for (int i = 0; i < sourceIds.size(); i++) {
        saved.setValue(QString::number(sourceIds[i]), "");
    }
    saved.sync();
    return true;
}

bool ftController::requestFile(QStringList librarymixer_names, QList<int> orig_item_ids, QStringList filenames,
                               std::string hash, uint64_t size, uint32_t flags, QList<int> &sourceIds) {
    if (librarymixer_names.size() != orig_item_ids.size() ||
            librarymixer_names.size() != filenames.size() ||
            librarymixer_names.size() < 1)
        return false;
    for (int i = 0; i < orig_item_ids.size(); i++) {
        requestFile(librarymixer_names[i], orig_item_ids[i], filenames[i], hash, size, flags, sourceIds);
    }
    return true;
}


bool    ftController::cancelFile(int orig_item_id, QString filename, std::string hash, bool checkComplete) {
    /*check if the file in the download map*/
    QMap<std::string,ftFileControl>::iterator mit;
    MixStackMutex stack(ctrlMutex);
    mit=mDownloads.find(hash);
    if (mit==mDownloads.end() || !mit.value().orig_item_ids.contains(orig_item_id)) {
        log(LOG_ERROR, FTCONTROLLERZONE, "Tried to cancel a file not being transferred!");
        return false;
    }

    /* check if finished */
    if (mit.value().mState == ftFileControl::COMPLETED) {
        log(LOG_WARNING, FTCONTROLLERZONE, "Tried to cancel a file that was already done!");
        return false;
    }

    int index = -1;
    for (int i = 0; i < mit.value().orig_item_ids.size(); i++){
        if (mit.value().orig_item_ids[i] == orig_item_id &&
            mit.value().filenames[i] == filename){
            index = i;
            break;
        }
    }
    if (index < 0) {
        log(LOG_WARNING, FTCONTROLLERZONE, "Tried to cancel a non-existent transfer!");
        return false;
    }

    log(LOG_WARNING, FTCONTROLLERZONE, "Cancelling file: " + filename);

    //If this file is being requested for more than one transfer
    if (mit.value().orig_item_ids.size() > 1) {
        mit.value().librarymixer_names.removeAt(index);
        mit.value().filenames.removeAt(index);
        mit.value().orig_item_ids.removeAt(index);
        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.beginGroup("Files");
        saved.beginGroup(hash.c_str());
        saved.beginGroup("Transfers");
        saved.remove(iniEncode(QString::number(orig_item_id) + filename));
        saved.sync();
    } else {
        mDataplex->removeTransferModule(mit.value().mHash);

        if (mit.value().mTransfer) {
            mit.value().mTransfer->cancelTransfer();
            delete mit.value().mTransfer;
            mit.value().mTransfer = NULL;
        }

        if (mit.value().mCreator) {
            delete mit.value().mCreator;
            mit.value().mCreator = NULL;
        }

        /* delete the temporary file */
        if (0 != QFile::remove(mit.value().tempDestination())) {
            log(LOG_ERROR, FTCONTROLLERZONE, "Unable to remove temporary file " + mit.value().tempDestination() + "!");
        }

        mDownloads.erase(mit);

        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.beginGroup("Files");
        saved.remove(hash.c_str());
        saved.sync();
    }

    /* It's possible this item was the last one remaining for a download item,
       and now the whole item should be completed.*/
    if (checkComplete && checkTransferComplete(orig_item_id)) completeTransfer(orig_item_id);

    return true;
}

bool    ftController::LibraryMixerTransferCancel(int orig_item_id) {
    //Need to clone because cancelFile can remove elements from mDownloads
    //No need for mutex protection, because cancelFile is mutex protected, and we cloned mDownloads
    QMap<std::string, ftFileControl> currentDownloads = mDownloads;
    QMap<std::string, ftFileControl>::const_iterator it;
    for (it = currentDownloads.begin(); it != currentDownloads.end(); it++) {
        int i = it.value().orig_item_ids.size();
        while(i > 0){
            i--;
            //If a given download is related to the orig_item_id being completed.
            if (it.value().orig_item_ids[i] == orig_item_id) {
                cancelFile(orig_item_id, it.value().filenames[i], it.value().mHash, false);
            }
        }
    }
    return true;
}

bool    ftController::controlFile(int orig_item_id, std::string hash, uint32_t flags) {
#ifdef false
    /*check if the file in the download map*/
    std::map<std::string,ftFileControl>::iterator mit;
    mit=mDownloads.find(hash);
    if (mit==mDownloads.end()) {
        return false;
    }

    /*find the point to transfer module*/
    ftTransferModule *ft=(mit->second).mTransfer;
    switch (flags) {
        case FILE_CTRL_PAUSE:
            ft->pauseTransfer();
            break;
        case FILE_CTRL_START:
            ft->resumeTransfer();
            break;
        default:
            return false;
    }
#endif
    (void) orig_item_id;
    (void) hash;
    (void) flags;
    return true;
}

void    ftController::clearCompletedFiles() {
    log(LOG_DEBUG_BASIC, FTCONTROLLERZONE, "ftController.clearCompletedFiles - clearing all completed transfers");
    mCompleted.clear();
}

/* get Details of File Transfers */
bool    ftController::FileDownloads(QList<FileInfo> &downloads) {
    MixStackMutex stack(ctrlMutex); /******* LOCKED ********/

    QMap<std::string, ftFileControl>::const_iterator it;
    for (it = mCompleted.begin(); it != mCompleted.end(); it++) {
        FileInfo info;
        FileDetails(it, info);
        downloads.push_back(info);
    }
    for (it = mDownloads.begin(); it != mDownloads.end(); it++) {
        FileInfo info;
        FileDetails(it, info);
        downloads.push_back(info);
    }
    return true;
}

bool    ftController::FileDetails(QMap<std::string, ftFileControl>::const_iterator file, FileInfo &info) {
    /* extract details */
    info.hash = file.value().mHash;
    info.paths = file.value().filenames;
    info.librarymixer_names = file.value().librarymixer_names;
    info.orig_item_ids = file.value().orig_item_ids;
    info.flags = file.value().mFlags;

    /* get list of sources from transferModule */
    QList<int> sourceIds;
    if (file.value().mState == ftFileControl::DOWNLOADING) file.value().mTransfer->getFileSources(sourceIds);

    double totalRate = 0;
    uint32_t tfRate = 0;
    uint32_t state = 0;
    bool isDownloading = false;
    //bool isSuspended = false;

    for (int i = 0; i < sourceIds.size(); i++) {
        if (file.value().mTransfer->getPeerState(sourceIds[i], state, tfRate)) {
            TransferInfo ti;
            switch (state) {
                case PQIPEER_INIT:
                    ti.status = FT_STATE_INIT;
                    break;
                case PQIPEER_NOT_ONLINE:
                    ti.status = FT_STATE_WAITING;
                    break;
                case PQIPEER_DOWNLOADING:
                    isDownloading = true;
                    ti.status = FT_STATE_DOWNLOADING;
                    break;
                case PQIPEER_IDLE:
                    ti.status = FT_STATE_IDLE;
                    break;
                default:
                case PQIPEER_SUSPEND:
                    ti.status = FT_STATE_WAITING;
                    break;
            }

            ti.tfRate = tfRate / 1024.0;
            ti.librarymixer_id = sourceIds[i];
            info.peers.push_back(ti);
            totalRate += tfRate / 1024.0;
        }
    }

    if (file.value().mState == ftFileControl::COMPLETED_ERROR) {
        info.downloadStatus = FT_STATE_FAILED;
    } else if (file.value().mState == ftFileControl::COMPLETED_CHECK || file.value().mState == ftFileControl::COMPLETED_WAIT){
        info.downloadStatus = FT_STATE_COMPLETE_WAIT;
    } else if (file.value().mState == ftFileControl::COMPLETED) {
        info.downloadStatus = FT_STATE_COMPLETE;
    } else if (isDownloading) {
        info.downloadStatus = FT_STATE_DOWNLOADING;
    } else {
        info.downloadStatus = FT_STATE_WAITING;
    }
    info.tfRate = totalRate;
    info.size = file.value().mSize;

    if (file.value().mState == ftFileControl::DOWNLOADING) {
        info.transfered  = file.value().mCreator->getRecvd();
        info.avail = info.transfered;
    } else {
        info.transfered  = info.size;
        info.avail = info.transfered;
    }

    return true;
}

/* Directory Handling */
bool    ftController::setDownloadDirectory(QString path, bool trySavedSettings) {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    QString tentativePath;
    if (trySavedSettings) {
        tentativePath = settings.value("Transfers/DownloadsDirectory", path).toString();
    } else {
        tentativePath = path;
    }
    /* check if it exists */
    if (DirUtil::checkCreateDirectory(tentativePath)) {
        MixStackMutex stack(ctrlMutex); /******* LOCKED ********/
        mDownloadPath = tentativePath;
        settings.setValue("Transfers/DownloadsDirectory", mDownloadPath);
        return true;
    }
    return false;
}

bool    ftController::setPartialsDirectory(QString path, bool trySavedSettings) {
    QSettings settings(*mainSettings, QSettings::IniFormat);
    QString tentativePath;
    if (trySavedSettings) {
        tentativePath = settings.value("Transfers/PartialsDirectory", path).toString();
    } else {
        tentativePath = path;
    }
    /* check if it exists */
    if (DirUtil::checkCreateDirectory(tentativePath)) {
        MixStackMutex stack(ctrlMutex); /******* LOCKED ********/
        mPartialsPath = tentativePath;
        settings.setValue("Transfers/PartialsDirectory", mPartialsPath);
        /* move all existing files! */
        QMap<std::string, ftFileControl>::const_iterator it;
        for (it = mDownloads.begin(); it != mDownloads.end(); it++) {
            it.value().mCreator->moveFile(mPartialsPath);
        }
        return true;
    }
    return false;
}

QString ftController::getDownloadDirectory() {
    return mDownloadPath;
}

QString ftController::getPartialsDirectory() {
    return mPartialsPath;
}

/***************************************************************/
/********************** Controller Access **********************/
/***************************************************************/

/* pqiMonitor callback:
 * Used to tell TransferModules new available peers
 */
void    ftController::statusChange(const std::list<pqipeer> &plist) {
    log(LOG_DEBUG_BASIC, FTCONTROLLERZONE, "ftController.statusChange");
    MixStackMutex stack(ctrlMutex); /******* LOCKED ********/

    /* add online to all downloads */
    QMap<std::string, ftFileControl>::const_iterator it;
    std::list<pqipeer>::const_iterator pit;

    for (it = mDownloads.begin(); it != mDownloads.end(); it++) {
        for (pit = plist.begin(); pit != plist.end(); pit++) {
            if (pit->actions & PEER_CONNECTED) {
#ifdef CONTROL_DEBUG
                std::cerr << " is Newly Connected!";
                std::cerr << std::endl;
#endif
                setPeerState(it.value().mTransfer, pit->librarymixer_id, true);
            } else if (pit->actions & PEER_DISCONNECTED) {
#ifdef CONTROL_DEBUG
                std::cerr << " is Just disconnected!";
                std::cerr << std::endl;
#endif
                setPeerState(it.value().mTransfer, pit->librarymixer_id, false);
            } else {
#ifdef CONTROL_DEBUG
                std::cerr << " had something happen to it: ";
                std::cerr << pit-> actions;
                std::cerr << std::endl;
#endif
                setPeerState(it.value().mTransfer, pit->librarymixer_id, false);
            }
        }
    }
}

bool    ftController::setPeerState(ftTransferModule *tm, int librarymixer_id, bool online) {
    if (tm != NULL){
        if (librarymixer_id == peers->getOwnLibraryMixerId()) {
            tm->setPeerState(librarymixer_id, PQIPEER_IDLE);
        } else if (online) {
            tm->setPeerState(librarymixer_id, PQIPEER_IDLE);
        } else {
            tm->setPeerState(librarymixer_id, PQIPEER_NOT_ONLINE);
        }
    }
    return true;
}

