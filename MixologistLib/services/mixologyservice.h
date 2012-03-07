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

#ifndef SERVICE_LIBRARY_MIXER_HEADER
#define SERVICE_LIBRARY_MIXER_HEADER

/*
 * Service for requesting Library Mixer items and responding to requests.
 */

#include <interface/types.h>
#include <services/p3service.h>
#include <pqi/pqimonitor.h>
#include <pqi/connectivitymanager.h>
#include <QString>
#include <QStringList>
#include <serialiser/mixologyitems.h>

class MixologyService;
extern MixologyService *mixologyService;

class MixologyService: public QObject, public p3Service {
    Q_OBJECT

public:

    MixologyService();
    virtual int tick();

    /* Adds a request to mPending for an item identified by item_id from friend with friend_id
      labeling the request name */
    bool LibraryMixerRequest(unsigned int friend_id, unsigned int item_id, const QString &name);

    /* Removes a request from mPending*/
    bool LibraryMixerRequestCancel(unsigned int item_id);

    /* Used for recommending files to friends. */
    void sendSuggestion(unsigned int friend_id, const QString &title, const QStringList &files, const QStringList &hashes, const QList<qlonglong> &filesizes);

    /* Used for returning borrowed files to friends. */
    void sendReturn(unsigned int friend_id, int source_type, const QString &source_id,
                    const QString &title, const QStringList &files, const QStringList &hashes, const QList<qlonglong> &filesizes);

    /* Called on completion of a borrow download to let the lender know the transfer is complete.*/
    void sendBorrowed(unsigned int friend_id, int source_type, const QString &source_id);

    /* Called on completion of a borrow return download to let the borrower know the transfer is complete.*/
    void sendBorrowedReturned(unsigned int friend_id, int source_type, const QString &source_id);

    /* Sets a list of all pendingRequests into requests.*/
    void getPendingRequests(std::list<pendingRequest> &requests);

    /* Clears all errored requests from mPending*/
    void clearCompleted();

signals:
    /* When a response is received from a request, and it is an offer to lend a set of files. */
    void responseLendOfferReceived(unsigned int friend_id, unsigned int item_id, QString title, QStringList paths, QStringList hashes, QList<qlonglong> filesizes);

private:
    /* For a given item_id, rechecks the files and sets the relevant file information into response.
       If all goes smoothly, response->itemStatus is set to status, otherwise it is set to error.
       Used on requests that are either matched to file or matched to lend.
       Ordinarily returns true, but if a request is made for an item that is not yet ready because it is still being hashed, return false. */
    bool prepFileResponse(unsigned int item_id, int status, MixologyResponse *response);

    /* Sends a LibraryMixerRequestItem. Not mutex protected, must be called from within the mutex. */
    void sendRequest(std::string cert_id, uint32_t item_id, pendingRequest *pending);

    /* List of LibraryMixer requests to be sent, as well as those that have been receieved and errored. */
    std::list<pendingRequest> mPending;

    /* Removes an item from mPending and also updates saved transfers file. */
    void remove_from_mPending(std::list<pendingRequest>::iterator request, bool erase = true);

    /* List populated on startup by saved requests.  Cannot immediately request because
      system is not yet ready, so stored here until tick can take care of them. */
    QStringList mStartup;

    mutable QMutex mMixologyServiceMutex;
};

#endif

