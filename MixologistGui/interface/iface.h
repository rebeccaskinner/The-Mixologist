/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2008, Robert Fernie.
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

#ifndef GUI_INTERFACE_H
#define GUI_INTERFACE_H

#include "types.h"

#include <map>

#include <QString>

class NotifyBase;
class Control;
class Init;
class LibraryMixerConnect;

extern Control *control;
extern NotifyBase *notifyBase;
extern LibraryMixerConnect *librarymixerconnect;

/*
The interface for controlling MixologistLib at a high-level.
*/
class Control {
public:

    Control(){return;}

    virtual ~Control() {return;}

    //Starts all of MixologistLib's threads
    virtual bool StartupMixologist() = 0;
    //Shuts down connections
    virtual bool ShutdownMixologist() = 0;
    //Reloads transfer limits set in settings.ini by the user
    virtual void ReloadTransferRates() = 0;

};


/********************** Overload this Class for callback *****************/

//Used with NotifyBase::notifyTransferEvent and NotifyBase::notifyRequestEvent
enum transferEvent {
    NOTIFY_TRANSFER_CHAT, //Received a response to a request that a chat is needed
    NOTIFY_TRANSFER_MESSAGE, //Received a response to a request that is a message
    NOTIFY_TRANSFER_LEND, //Received a response to a request that is an invitation to borrow
    NOTIFY_TRANSFER_LENT, //Received a response to a request that item is currently lent out
    NOTIFY_TRANSFER_UNMATCHED, //Received a response that the item has no auto response
    NOTIFY_TRANSFER_BROKEN_MATCH, //Recieved a response that the item has an auto file match, but the file is missing
    NOTIFY_TRANSFER_NO_SUCH_ITEM //Received a response that requested item isn't in recipient's library
};

//Used with NotifyBase::notifyUserOptional
enum userOptionalCodes {
    NOTIFY_USER_REQUEST, //An outgoing request was made
    NOTIFY_USER_FILE_REQUEST, //A request was received, and file(s) are being auto sent
    NOTIFY_USER_FILE_RESPONSE, //A request was sent, and file(s) are being received
    NOTIFY_USER_LEND_OFFERED, //A request was received, offer to lend sent
    NOTIFY_USER_BORROW_DECLINED, //Offer to lend was sent, and it was declined
    NOTIFY_USER_BORROW_ACCEPTED, //Offer to lend was sent, and it was accepted
    NOTIFY_USER_SUGGEST_WAITING, //Hashing a file and will send an invitation to download
    NOTIFY_USER_SUGGEST_SENT, //Sent an invitation to download
    NOTIFY_USER_SUGGEST_RECEIVED //Received an invitation to download
};

/* The main class via which information is passed between MixologistLib and MixologistGui. */
class NotifyBase {
public:
    //A connected friend has sent a notice of availability of a download
    virtual void notifySuggestion(int librarymixer_id, int item_id, QString name) = 0;
    //A request has been received for an item that requires a popup
    virtual void notifyRequestEvent(transferEvent event, int librarymixer_id, int item_id = 0) = 0;
    //A response has been received on a request for an item that requires a popup
    virtual void notifyTransferEvent(transferEvent event, int librarymixer_id, QString transfer_name, QString extra_info = "") = 0;
    //When library list updated
    virtual void notifyLibraryUpdated() = 0;
    //New chat status info
    virtual void notifyChatStatus(int librarymixer_id, const QString &status_string) = 0;
    //A file is being hashed, or "" if all hashing is complete
    virtual void notifyHashingInfo(QString fileinfo) = 0;
    //A message can be added to the log
    virtual void notifyLog(QString message) = 0;
    //Something happened involving a user that is low priority, and only optionally should be displayed
    virtual void notifyUserOptional(int librarymixer_id, userOptionalCodes code, QString message) = 0;
};
#endif //GUI_INTERFACE_H
