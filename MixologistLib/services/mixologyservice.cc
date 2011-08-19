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
#include <interface/librarymixer-library.h>
#include <ft/ftserver.h>
#include <interface/peers.h>
#include <interface/files.h>
#include <interface/types.h>
#include <interface/iface.h>
#include <interface/settings.h>
#include <QString>
#include <QSettings>
#include <QDir>
#include <util/dir.h>
#include <util/debug.h>


/****
 * #define LIBRARY_MIXER_SERVICE_DEBUG 1
 ****/

#define REQUEST_RETRY_DELAY 20 //20 seconds between retries

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
        MixologyRequest *request;
        MixologyResponse *response;
        MixologySuggestion *suggest;
        MixologyLending *lending;

        request = dynamic_cast<MixologyRequest *>(item) ;
        if (request != NULL) {
            log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Received a request for " + QString::number(request->item_id));
            MixologyResponse *response = new MixologyResponse();
            response->PeerId(request->PeerId());
            response->item_id = request->item_id;
            /*This is a hack, but it will have to do until we properly merge librarymixer-library and ftitemlist
              If the status is < 0 on searching the LibraryMixerLibraryManager, also search the ftitemlist to see
              if it's a temporary item that LibraryMixerLibraryManager wouldn't know about.
              Note that the call to LibraryMixerLibraryManager will update the item list from the server if it
              doesn't find anything and check again.*/
            int status = LibraryMixerLibraryManager::getLibraryMixerItemStatus(response->item_id, true);
            if (status < 0) status = ftserver->getItemStatus(response->item_id);

            if (status < 0) {
                response->itemStatus = MixologyResponse::ITEM_STATUS_NO_SUCH_ITEM;
                notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_NO_SUCH_ITEM, peers->findLibraryMixerByCertId(request->PeerId()));
            } else if (status == ITEM_UNMATCHED) {
                response->itemStatus = MixologyResponse::ITEM_STATUS_UNMATCHED;
                notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_UNMATCHED, peers->findLibraryMixerByCertId(request->PeerId()), request->item_id);
            } else if (status == ITEM_MATCHED_TO_CHAT) {
                response->itemStatus = MixologyResponse::ITEM_STATUS_CHAT;
                notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_CHAT, peers->findLibraryMixerByCertId(request->PeerId()), request->item_id);
            } else if (status == ITEM_MATCH_NOT_FOUND) {
                response->itemStatus = MixologyResponse::ITEM_STATUS_BROKEN_MATCH;
                notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_BROKEN_MATCH, peers->findLibraryMixerByCertId(request->PeerId()), request->item_id);
            } else if (status == ITEM_MATCHED_TO_MESSAGE) {
                response->itemStatus = MixologyResponse::ITEM_STATUS_MATCHED_TO_MESSAGE;
                LibraryMixerItem item = LibraryMixerLibraryManager::getLibraryMixerItem(request->item_id);
                notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_MESSAGE, peers->findLibraryMixerByCertId(request->PeerId()), request->item_id);
                response->subject = item.message;
            } else if (status == ITEM_MATCHED_TO_FILE) {
                LibraryMixerItem item = LibraryMixerLibraryManager::getLibraryMixerItem(request->item_id);
                if (item.empty()) item = ftserver->getItem(request->item_id);
                notifyBase->notifyUserOptional(peers->findLibraryMixerByCertId(request->PeerId()), NOTIFY_USER_FILE_REQUEST, item.name());
                prepFileResponse(request->item_id, MixologyResponse::ITEM_STATUS_MATCHED_TO_FILE, response);
            } else if (status == ITEM_MATCHED_TO_LEND) {
                LibraryMixerItem item = LibraryMixerLibraryManager::getLibraryMixerItem(request->item_id);
                notifyBase->notifyUserOptional(peers->findLibraryMixerByCertId(request->PeerId()), NOTIFY_USER_LEND_OFFERED, item.name());
                prepFileResponse(request->item_id, MixologyResponse::ITEM_STATUS_MATCHED_TO_LEND, response);
            } else if (status == ITEM_MATCHED_TO_LENT) {
                response->itemStatus = MixologyResponse::ITEM_STATUS_MATCHED_TO_LENT;
                notifyBase->notifyRequestEvent(NOTIFY_TRANSFER_LENT, peers->findLibraryMixerByCertId(request->PeerId()), request->item_id);
            }
            sendItem(response);
            goto donereceiving;
        }
        response = dynamic_cast<MixologyResponse *>(item) ;
        if (response != NULL) {
            std::list<pendingRequest>::iterator it;
            MixStackMutex stack(mMixologyServiceMutex);
            for(it = mPending.begin(); it != mPending.end(); it++) {
                //I'm sure I'm missing something stupid, but I can't figure out why it->item_id is becoming an unsigned int causing comparison between signed and unsigned warnings.
                if (response->item_id == it->item_id &&
                        peers->findLibraryMixerByCertId(response->PeerId()) == it->librarymixer_id) {
                    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Received a response from " + QString::number(it->librarymixer_id));
                    if (response->itemStatus == MixologyResponse::ITEM_STATUS_MATCHED_TO_FILE) {
                        QList<int> sourceIds;
                        sourceIds.push_back(it->librarymixer_id);
                        QStringList filenames = response->subject.split("\n");
                        QStringList hashes = QString(response->hashes.c_str()).split("\n");
                        QStringList filesizes = QString(response->filesizes.c_str()).split("\n");
                        for(int i = 0; i < filenames.count(); i++) {
                            if(!filenames[i].isEmpty() && !hashes[i].isEmpty() && !filesizes[i].isEmpty())
                                files->requestFile(it->name, it->item_id, filenames[i], hashes[i].toStdString(),
                                                   filesizes[i].toULong(), 0, sourceIds);
                        }
                        notifyBase->notifyUserOptional( it->librarymixer_id, NOTIFY_USER_FILE_RESPONSE, it->name);
                        remove_from_mPending(it);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_MATCHED_TO_LEND) {
                        ftserver->addPendingBorrow(it->item_id, it->name, it->librarymixer_id, response->subject.split("\n"),
                                                   QString(response->hashes.c_str()).split("\n"), QString(response->filesizes.c_str()).split("\n"));
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_LEND, it->librarymixer_id, it->name, QString::number(response->item_id));
                        remove_from_mPending(it);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_MATCHED_TO_LENT) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_LENT, it->librarymixer_id, it->name);
                        remove_from_mPending(it);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_MATCHED_TO_MESSAGE) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_MESSAGE, it->librarymixer_id, it->name, response->subject);
                        remove_from_mPending(it);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_INTERNAL_ERROR) {
                        it->status = pendingRequest::REPLY_INTERNAL_ERROR;
                        //Do not remove from mPending here, so that the error message is displayed in the transfers window
                        remove_from_mPending(it, false);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_NO_SUCH_ITEM) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_NO_SUCH_ITEM, it->librarymixer_id, it->name);
                        remove_from_mPending(it);
                        break;
                    }  else if (response->itemStatus == MixologyResponse::ITEM_STATUS_CHAT) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_CHAT, it->librarymixer_id, it->name);
                        remove_from_mPending(it);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_UNMATCHED) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_UNMATCHED, it->librarymixer_id, it->name);
                        remove_from_mPending(it);
                        break;
                    } else if (response->itemStatus == MixologyResponse::ITEM_STATUS_BROKEN_MATCH) {
                        notifyBase->notifyTransferEvent(NOTIFY_TRANSFER_BROKEN_MATCH, it->librarymixer_id, it->name);
                        remove_from_mPending(it);
                        break;
                    }
                }
            }
            goto donereceiving;
        }
        suggest = dynamic_cast<MixologySuggestion *>(item) ;
        if (suggest != NULL) {
            /*Note that we don't have to worry about negative item_ids from temp items causing collisions here
              because the LibraryMixerLibraryManager queries only the xml and not temp items. */
            LibraryMixerItem item = LibraryMixerLibraryManager::getLibraryMixerItem(suggest->item_id);
            int librarymixer_id = peers->findLibraryMixerByCertId(suggest->PeerId());
            if (!item.empty()) {
                if (item.itemState == ITEM_MATCHED_TO_LENT && item.lentTo == librarymixer_id) {
                    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Received a return offer from " + QString::number(librarymixer_id));
                    LibraryMixerRequest(librarymixer_id, suggest->item_id, suggest->title);
                }
            } else {
                log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Received a suggestion from " + QString::number(librarymixer_id));
                notifyBase->notifySuggestion(librarymixer_id, suggest->item_id, suggest->title);
            }
            goto donereceiving;
        }

        lending = dynamic_cast<MixologyLending *>(item) ;
        if (lending != NULL) {
            if (lending->flags & TRANSFER_COMPLETE_BORROWED) {
                log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Finished lending " + QString::number(lending->item_id));
                LibraryMixerLibraryManager::setLent(peers->findLibraryMixerByCertId(lending->PeerId()), lending->item_id);
                notifyBase->notifyLibraryUpdated();
            } else if (lending->flags & TRANSFER_COMPLETE_RETURNED) {
                log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Finished getting back " + QString::number(lending->item_id));
                ftserver->returnedBorrowed(lending->item_id);
            }
            goto donereceiving;
        }
        donereceiving:
        delete item;
    }

    /*check if any stale requests should be resent now.  We send whether or not peer is online, and it will be discarded
      if he is offline by the transfer modules */
    std::list<pendingRequest>::iterator it;
    MixStackMutex stack(mMixologyServiceMutex);
    for(it = mPending.begin(); it != mPending.end(); it++) {
        if((time(NULL) - it->timeOfLastTry > REQUEST_RETRY_DELAY) && it->status == pendingRequest::REPLY_NONE) {
            sendRequest(peers->findCertByLibraryMixerId(it->librarymixer_id), it->item_id, &*it);
        }
    }

    return 0;
}

bool  MixologyService::LibraryMixerRequest(int librarymixer_id, int item_id, QString name) {
    notifyBase->notifyUserOptional(librarymixer_id, NOTIFY_USER_REQUEST, name);
    MixStackMutex stack(mMixologyServiceMutex);
    //check if we already have this one, if we do no need to add, but either way send a new request
    std::list<pendingRequest>::iterator pending;
    for(pending = mPending.begin(); pending != mPending.end(); pending++) {
        if (pending->librarymixer_id == librarymixer_id &&
                pending->item_id == item_id) break;
    }
    if (pending == mPending.end()) {
        pendingRequest newPendingRequest;
        newPendingRequest.librarymixer_id = librarymixer_id;
        newPendingRequest.item_id = item_id;
        newPendingRequest.name = name;
        newPendingRequest.timeOfLastTry = 0;
        mPending.push_back(newPendingRequest); //this pushback means that pending now points to newPendingRequest
        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.beginGroup("Pending");
        saved.beginGroup(QString::number(item_id));
        saved.setValue("friend_id", librarymixer_id);
        saved.setValue("name", name);
    }
    {
        QSettings saved(*savedTransfers, QSettings::IniFormat);
        saved.beginGroup("Pending");
        saved.beginGroup(QString::number(item_id));
    }
    return true;
}

bool  MixologyService::LibraryMixerRequestCancel(int item_id) {
    MixStackMutex stack(mMixologyServiceMutex);
    std::list<pendingRequest>::iterator pending;
    for(pending = mPending.begin(); pending != mPending.end(); pending++) {
        if (pending->item_id == item_id) {
            remove_from_mPending(pending);
            return true;
        }
    }
    return false;
}

void MixologyService::sendMixologySuggestion(unsigned int librarymixer_id, int item_id, QString name) {
    MixologySuggestion *suggest = new MixologySuggestion();
    suggest->title = name;
    suggest->PeerId(peers->findCertByLibraryMixerId(librarymixer_id));
    suggest->item_id = item_id;
    notifyBase->notifyUserOptional(librarymixer_id, NOTIFY_USER_SUGGEST_SENT, name);
    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Sending suggestion to " + QString::number(librarymixer_id) + ".");
    sendItem(suggest);
}

void MixologyService::LibraryMixerBorrowed(unsigned int librarymixer_id, int item_id) {
    MixologyLending *lend= new MixologyLending();
    lend->PeerId(peers->findCertByLibraryMixerId(librarymixer_id));
    lend->item_id = item_id;
    lend->flags = TRANSFER_COMPLETE_BORROWED;
    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Letting " + QString::number(librarymixer_id) + " know borrow received.");
    sendItem(lend);
}

void MixologyService::LibraryMixerBorrowReturned(unsigned int librarymixer_id, int item_id) {
    MixologyLending *lend= new MixologyLending();
    lend->PeerId(peers->findCertByLibraryMixerId(librarymixer_id));
    lend->item_id = item_id;
    lend->flags = TRANSFER_COMPLETE_RETURNED;
    log(LOG_WARNING, MIXOLOGYSERVICEZONE, "Letting " + QString::number(librarymixer_id) + " know return received.");
    sendItem(lend);
}

void MixologyService::getPendingRequests(std::list<pendingRequest> &requests) {
    std::list<pendingRequest>::iterator it;
    MixStackMutex stack(mMixologyServiceMutex);
    for (it = mPending.begin(); it != mPending.end(); it++) {
        pendingRequest request;
        request.librarymixer_id = it->librarymixer_id;
        request.item_id = it->item_id;
        request.name = it->name;
        request.timeOfLastTry = it->timeOfLastTry;
        request.status = it->status;
        requests.push_back(request);
    }
}

void MixologyService::clearCompleted() {
    MixStackMutex stack(mMixologyServiceMutex);
    std::list<pendingRequest>::iterator pending;
    for(pending = mPending.begin(); pending != mPending.end(); pending++) {
        if (pending->status != pendingRequest::REPLY_NONE) {
            std::list<pendingRequest>::iterator temp = pending;
            pending++;
            mPending.erase(temp);
        }
    }
}

void MixologyService::prepFileResponse(int item_id, int status, MixologyResponse *response) {
    LibraryMixerItem *item = LibraryMixerLibraryManager::recheckItemNode(item_id);
    if (item == NULL) response->itemStatus = MixologyResponse::ITEM_STATUS_INTERNAL_ERROR;
    else if (item->itemState == ITEM_MATCH_NOT_FOUND) response->itemStatus = MixologyResponse::ITEM_STATUS_BROKEN_MATCH;
    else {
        response->itemStatus = status;
        /*First we find the common root so that we can remove that and send over the subdirectory structure*/
        bool trimmingDone = false;
        int toTrim = 0;

        while(!trimmingDone) {
            int currentSubDirEnd = item->paths[0].indexOf(QDir::separator(), toTrim);
            if (currentSubDirEnd == -1) {
                trimmingDone = true;
            } else {
                QString currentSubDir = item->paths[0].left(currentSubDirEnd + 1);
                for (int i = 0; i < item->paths.count(); i++) {
                    if (!item->paths[i].startsWith(currentSubDir)) {
                        trimmingDone = true;
                        break;
                    }
                }
                if (!trimmingDone) toTrim = currentSubDir.length();
            }
        }

        for(int i = 0; i < item->paths.count(); i++) {
            response->subject.append(item->paths[i].mid(toTrim));
            response->subject.append("\n");
            response->hashes.append(item->hashes[i].toStdString());
            response->hashes.append("\n");
            response->filesizes.append(QString::number(item->filesizes[i]).toStdString());
            response->filesizes.append("\n");
        }
    }
}

void MixologyService::sendRequest(std::string cert_id, uint32_t item_id, pendingRequest *pending) {
    MixologyRequest *mrequest = new MixologyRequest();
    mrequest->PeerId(cert_id);
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
