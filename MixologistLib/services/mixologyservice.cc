/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
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

#include <services/mixologyservice.h>
#include <serialiser/mixologyitems.h>
#include <server/librarymixer-library.h>
#include <ft/ftofflmlist.h>
#include <ft/ftcontroller.h>
#include <ft/ftserver.h>
#include <ft/ftborrower.h>
#include <interface/peers.h>
#include <interface/files.h>
#include <interface/types.h>
#include <interface/notify.h>
#include <interface/settings.h>
#include <QString>
#include <QSettings>
#include <QDir>
#include <util/dir.h>
#include <util/debug.h>
#include <time.h>


/****
 * #define LIBRARY_MIXER_SERVICE_DEBUG 1
 ****/

#define REQUEST_RETRY_DELAY 20 //20 seconds between retries
#define REQUEST_LENT_RETRY_DELAY 3600 //1 hour between retries of items that are lent out

MixologyService::MixologyService()
    :p3Service(SERVICE_TYPE_MIX) {
    addSerialType(new MixologySerialiser());
    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.beginGroup("Pending");
    mStartup = saved.childGroups();
}

int MixologyService::tick() {
    //try to receive an item and handle it if available
    if (mStartup.isEmpty() != true) {
        QString request = mStartup.first();
        mStartup.removeFirst();
        QSettings saved(*savedTransfers, QSettings::IniFormat);
        LibraryMixerRequest(saved.value("Pending/" + request + "/friend_id").toInt(),
                            request.toInt(),
                            saved.value("Pending/" + request + "/name").toString());
    }
    NetItem *item ;
    if ((item=recvItem()) != NULL) {
        unsigned int friend_id = item->LibraryMixerId();

        MixologyRequest *request = dynamic_cast<MixologyRequest *>(item);
        if (request != NULL) {
            log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Received a request for " + QString::number(request->item_id));
            MixologyResponse *response = new MixologyResponse();
            LibraryMixerItem* libraryItem;
            response->LibraryMixerId(request->LibraryMixerId());
            response->item_id = request->item_id;

            switch(files->getLibraryMixerItemStatus(response->item_id, true)){
                case LibraryMixerItem::UNMATCHED:
                    response->itemStatus = MixologyResponse::ITEM_STATUS_UNMATCHED;
                    notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_UNMATCHED, friend_id, request->item_id);
                    sendItem(response);
                    break;
                case LibraryMixerItem::MATCHED_TO_CHAT:
                    response->itemStatus = MixologyResponse::ITEM_STATUS_CHAT;
                    notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_CHAT, friend_id, request->item_id);
                    sendItem(response);
                    break;
                case LibraryMixerItem::MATCH_NOT_FOUND:
                    response->itemStatus = MixologyResponse::ITEM_STATUS_BROKEN_MATCH;
                    notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_BROKEN_MATCH, friend_id, request->item_id);
                    sendItem(response);
                    break;
                case LibraryMixerItem::MATCHED_TO_MESSAGE:
                    response->itemStatus = MixologyResponse::ITEM_STATUS_MATCHED_TO_MESSAGE;
                    libraryItem = files->getLibraryMixerItem(request->item_id);
                    notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_MESSAGE, friend_id, request->item_id);
                    response->message(libraryItem->message());
                    sendItem(response);
                    break;
                case LibraryMixerItem::MATCHED_TO_FILE:
                    libraryItem = files->getLibraryMixerItem(request->item_id);
                    notifyBase->notifyUserOptional(friend_id, NotifyBase::NOTIFY_USER_FILE_REQUEST, libraryItem->title());
                    if (prepFileResponse(request->item_id, MixologyResponse::ITEM_STATUS_MATCHED_TO_FILE, response)) sendItem(response);
                    break;
                case LibraryMixerItem::MATCHED_TO_LEND:
                    libraryItem = files->getLibraryMixerItem(request->item_id);
                    notifyBase->notifyUserOptional(friend_id, NotifyBase::NOTIFY_USER_LEND_OFFERED, libraryItem->title());
                    if (prepFileResponse(request->item_id, MixologyResponse::ITEM_STATUS_MATCHED_TO_LEND, response)) sendItem(response);
                    break;
                case LibraryMixerItem::MATCHED_TO_LENT:
                    response->itemStatus = MixologyResponse::ITEM_STATUS_MATCHED_TO_LENT;
                    notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_LENT, friend_id, request->item_id);
                    sendItem(response);
                    break;
                default:
                    response->itemStatus = MixologyResponse::ITEM_STATUS_NO_SUCH_ITEM;
                    notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_NO_SUCH_ITEM, friend_id);
                    sendItem(response);
            }
        }

        MixologyResponse *response = dynamic_cast<MixologyResponse *>(item);
        if (response != NULL) {
            std::list<pendingRequest>::iterator it;
            QMutexLocker stack(&mMixologyServiceMutex);
            for(it = mPending.begin(); it != mPending.end(); it++) {
                if (response->item_id == it->item_id && friend_id == it->friend_id) {
                    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Received a response from " + QString::number(it->friend_id));
                    if (response->itemStatus == MixologyResponse::ITEM_STATUS_MATCHED_TO_FILE) {
                        if (!response->checkWellFormed())
                            log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Response from " + QString::number(it->friend_id) + " wasn't well-formed");
                        else files->downloadFiles(it->friend_id, it->name, response->files(), response->hashes(), response->filesizes());
                        notifyBase->notifyUserOptional(it->friend_id, NotifyBase::NOTIFY_USER_FILE_RESPONSE, it->name);
                        remove_from_mPending(it);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_MATCHED_TO_LEND) {
                        emit responseLendOfferReceived(it->friend_id, it->item_id, it->name, response->files(), response->hashes(), response->filesizes());
                        remove_from_mPending(it);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_MATCHED_TO_LENT) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_LENT, it->friend_id, it->name);
                        it->status = pendingRequest::REPLY_LENT_OUT;
                        //Do not remove from mPending so that next time we restart the Mixologist, it will try again
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_MATCHED_TO_MESSAGE) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_MESSAGE, it->friend_id, it->name, response->message());
                        it->status = pendingRequest::REPLY_MESSAGE;
                        remove_from_mPending(it, false);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_INTERNAL_ERROR) {
                        it->status = pendingRequest::REPLY_INTERNAL_ERROR;
                        remove_from_mPending(it, false);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_NO_SUCH_ITEM) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_NO_SUCH_ITEM, it->friend_id, it->name);
                        it->status = pendingRequest::REPLY_NO_SUCH_ITEM;
                        remove_from_mPending(it, false);
                        break;
                    }  else if (response->itemStatus == MixologyResponse::ITEM_STATUS_CHAT) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_CHAT, it->friend_id, it->name);
                        it->status = pendingRequest::REPLY_CHAT;
                        remove_from_mPending(it, false);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_UNMATCHED) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_UNMATCHED, it->friend_id, it->name);
                        it->status = pendingRequest::REPLY_UNMATCHED;
                        remove_from_mPending(it, false);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_BROKEN_MATCH) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_BROKEN_MATCH, it->friend_id, it->name);
                        it->status = pendingRequest::REPLY_BROKEN_MATCH;
                        remove_from_mPending(it, false);
                        break;
                    }
                }
            }
        }

        MixologySuggestion *suggest = dynamic_cast<MixologySuggestion *>(item);
        if (suggest != NULL) {
            log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Received a suggestion from " + QString::number(friend_id));
            notifyBase->notifySuggestion(friend_id, suggest->title, suggest->files(), suggest->hashes(), suggest->filesizes());
        }

        MixologyReturn *mixReturn = dynamic_cast<MixologyReturn *>(item);
        if (mixReturn != NULL) {
            log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Received a return offer from " + QString::number(friend_id));
            fileDownloadController->downloadBorrowedFiles(friend_id, mixReturn->source_type, mixReturn->source_id, mixReturn->files(), mixReturn->hashes(), mixReturn->filesizes());
        }

        MixologyLending *lending = dynamic_cast<MixologyLending *>(item);
        if (lending != NULL) {
            if (lending->flags & TRANSFER_COMPLETE_BORROWED) {
                unsigned int friend_id = lending->LibraryMixerId();
                log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Lent " + QString::number(lending->source_type) + ", " + lending->source_id + " to " + QString::number(friend_id));
                if (lending->source_type & FILE_HINTS_ITEM) {
                    librarymixermanager->setLent(friend_id, lending->source_id);
                } else if (lending->source_type & FILE_HINTS_OFF_LM) {
                    if (offLMList) offLMList->setLent(friend_id, lending->source_id);
                }
            } else if (lending->flags & TRANSFER_COMPLETE_RETURNED) {
                log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Finished getting back " + lending->source_id);
                borrowManager->returnedBorrowed(friend_id, lending->source_type, lending->source_id);
            }
        }

        if (request == NULL && response == NULL && suggest == NULL && mixReturn == NULL && lending == NULL) {
            std::cerr << "Received a malformed packet.\n";
        }

        delete item;
    }

    /*check if any stale requests should be resent now.  We send whether or not peer is online, and it will be discarded
      if he is offline by the transfer modules */
    std::list<pendingRequest>::iterator it;
    QMutexLocker stack(&mMixologyServiceMutex);
    for(it = mPending.begin(); it != mPending.end(); it++) {
        if(it->status == pendingRequest::REPLY_NONE && (time(NULL) - it->timeOfLastTry > REQUEST_RETRY_DELAY)) {
            sendRequest(it->friend_id, it->item_id, &*it);
        }
        if(it->status == pendingRequest::REPLY_LENT_OUT && (time(NULL) - it->timeOfLastTry > REQUEST_LENT_RETRY_DELAY)) {
            sendRequest(it->friend_id, it->item_id, &*it);
        }
    }

    return 0;
}

bool  MixologyService::LibraryMixerRequest(unsigned int friend_id, unsigned int item_id, const QString &name) {
    notifyBase->notifyUserOptional(friend_id, NotifyBase::NOTIFY_USER_REQUEST, name);
    QMutexLocker stack(&mMixologyServiceMutex);

    //Remove any pre-existing requests for this one, so we start from a clean slate
    std::list<pendingRequest>::iterator pending;
    for(pending = mPending.begin(); pending != mPending.end(); pending++) {
        if (pending->friend_id == friend_id &&
            pending->item_id == item_id) {
            remove_from_mPending(pending, true);
            pending = mPending.end();
        }
    }
    if (pending == mPending.end()) {
        pendingRequest newPendingRequest;
        newPendingRequest.friend_id = friend_id;
        newPendingRequest.item_id = item_id;
        newPendingRequest.name = name;
        newPendingRequest.timeOfLastTry = 0;
        mPending.push_back(newPendingRequest); //this pushback means that pending now points to newPendingRequest
        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.beginGroup("Pending");
        saved.beginGroup(QString::number(item_id));
        saved.setValue("friend_id", friend_id);
        saved.setValue("name", name);
    }
    {
        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.beginGroup("Pending");
        saved.beginGroup(QString::number(item_id));
    }
    return true;
}

bool  MixologyService::LibraryMixerRequestCancel(unsigned int item_id) {
    QMutexLocker stack(&mMixologyServiceMutex);
    std::list<pendingRequest>::iterator pending;
    for(pending = mPending.begin(); pending != mPending.end(); pending++) {
        if (pending->item_id == item_id) {
            remove_from_mPending(pending);
            return true;
        }
    }
    return false;
}

void MixologyService::sendSuggestion(unsigned int friend_id, const QString &title, const QStringList &files, const QStringList &hashes, const QList<qlonglong> &filesizes) {
    MixologySuggestion *suggest = new MixologySuggestion();
    suggest->title = title;
    suggest->LibraryMixerId(friend_id);
    suggest->files(files);
    suggest->hashes(hashes);
    suggest->filesizes(filesizes);
    notifyBase->notifyUserOptional(friend_id, NotifyBase::NOTIFY_USER_SUGGEST_SENT, title);
    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Sending suggestion to " + QString::number(friend_id) + ".");
    sendItem(suggest);
}

void MixologyService::sendReturn(unsigned int friend_id, int source_type, const QString &source_id,
                                 const QString &title, const QStringList &files, const QStringList &hashes, const QList<qlonglong> &filesizes) {
    MixologyReturn *returnItem = new MixologyReturn();
    returnItem->LibraryMixerId(friend_id);
    returnItem->source_type = source_type;
    returnItem->source_id = source_id;
    returnItem->files(files);
    returnItem->hashes(hashes);
    returnItem->filesizes(filesizes);
    notifyBase->notifyUserOptional(friend_id, NotifyBase::NOTIFY_USER_SUGGEST_SENT, title);
    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Returning " + QString::number(returnItem->source_type) + ", " + returnItem->source_id + " to " + QString::number(friend_id) + ".");
    sendItem(returnItem);
}

void MixologyService::sendBorrowed(unsigned int friend_id, int source_type, const QString &source_id) {
    MixologyLending *lend = new MixologyLending();
    lend->LibraryMixerId(friend_id);
    lend->flags = TRANSFER_COMPLETE_BORROWED;
    lend->source_type = source_type;
    lend->source_id = source_id;
    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Letting " + QString::number(friend_id) + " know borrow of " + QString::number(lend->source_type) + ", " + lend->source_id + " fully received.");
    sendItem(lend);
}

void MixologyService::sendBorrowedReturned(unsigned int friend_id, int source_type, const QString &source_id) {
    MixologyLending *lend = new MixologyLending();
    lend->LibraryMixerId(friend_id);
    lend->flags = TRANSFER_COMPLETE_RETURNED;
    lend->source_type = source_type;
    lend->source_id = source_id;
    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Letting " + QString::number(friend_id) + " know return of " + QString::number(source_type) + ", " + source_id + " received.");
    sendItem(lend);
}

void MixologyService::getPendingRequests(std::list<pendingRequest> &requests) {
    std::list<pendingRequest>::iterator it;
    QMutexLocker stack(&mMixologyServiceMutex);
    for (it = mPending.begin(); it != mPending.end(); it++) {
        pendingRequest request;
        request.friend_id = it->friend_id;
        request.item_id = it->item_id;
        request.name = it->name;
        request.timeOfLastTry = it->timeOfLastTry;
        request.status = it->status;
        requests.push_back(request);
    }
}

void MixologyService::clearCompleted() {
    QMutexLocker stack(&mMixologyServiceMutex);
    std::list<pendingRequest>::iterator pending;
    for(pending = mPending.begin(); pending != mPending.end(); pending++) {
        if (pending->status != pendingRequest::REPLY_NONE &&
            pending->status != pendingRequest::REPLY_LENT_OUT) {
            std::list<pendingRequest>::iterator temp = pending;
            pending++;
            mPending.erase(temp);
        }
    }
}

bool MixologyService::prepFileResponse(unsigned int item_id, int status, MixologyResponse *response) {
    LibraryMixerItem* item = NULL;
    int result = librarymixermanager->getLibraryMixerItemWithCheck(item_id, item);
    if (result == -1) response->itemStatus = MixologyResponse::ITEM_STATUS_INTERNAL_ERROR;
    else if (result == 0) return false; //Not ready at the moment, so try again next time friend asks for it and ignore for now.
    else if (result == 1){
        if (item->itemState() == LibraryMixerItem::MATCH_NOT_FOUND) response->itemStatus = MixologyResponse::ITEM_STATUS_BROKEN_MATCH;
        else {
            response->itemStatus = status;
            response->files(DirUtil::getRelativePaths(item->paths()));
            response->hashes(item->hashes());
            response->filesizes(item->filesizes());
            if (!response->checkWellFormed()) {
                log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Error preparing a response for " + item->title());
                response->itemStatus = MixologyResponse::ITEM_STATUS_INTERNAL_ERROR;
            }
        }
    }
    return true;
}

void MixologyService::sendRequest(unsigned int librarymixer_id, uint32_t item_id, pendingRequest *pending) {
    MixologyRequest *mrequest = new MixologyRequest();
    mrequest->LibraryMixerId(librarymixer_id);
    mrequest->item_id = item_id;
    pending->timeOfLastTry = time(NULL);
    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Sending a request for " + pending->name);
    sendItem(mrequest);
}

void MixologyService::remove_from_mPending(std::list<pendingRequest>::iterator request, bool erase) {
    QSettings saved(*savedTransfers, QSettings::IniFormat);
    saved.remove(QString("Pending/").append(QString::number(request->item_id)));
    if (erase) mPending.erase(request);
}
