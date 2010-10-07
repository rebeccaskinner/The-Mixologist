/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-2006, Robert Fernie.
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

#ifndef TYPES_GUI_INTERFACE_H
#define TYPES_GUI_INTERFACE_H

#include <list>
#include <iostream>
#include <string>
#include <stdint.h>
#include <QStringList>

const uint32_t FT_STATE_FAILED        = 0x0000; //Generally an error in copying or moving the file
const uint32_t FT_STATE_INIT          = 0x0001;
const uint32_t FT_STATE_WAITING       = 0x0002; //Generally peer is offline
const uint32_t FT_STATE_DOWNLOADING   = 0x0003;
const uint32_t FT_STATE_COMPLETE      = 0x0004;
const uint32_t FT_STATE_COMPLETE_WAIT = 0x0005; //When this file is done, but it is multifile transfer and the rest aren't
const uint32_t FT_STATE_IDLE          = 0x0006;

class TransferInfo {
public:
    /**** Need Some of these Fields ****/
    std::string cert_id;
    int librarymixer_id;
    std::string name; /* if has alternative name? */
    double tfRate; /* kbytes */
    int  status; /* FT_STATE_... */
};

/* This is used to pass information about both uploading and downloading files.
   If downloading more than one transfer can be downloading into the same file, hence
   paths, librarymixer_names, and orig_item_ids are lists.
   If uploading, then those elements will only have a single list item each. */
class FileInfo {
    /* old BaseInfo Entries */
public:

    FileInfo() :flags(0), mId(0) {
        return;
    }
    std::string id; /* key for matching everything */
    int flags; /* INFO_TAG above */

    /* allow this to be tweaked by the GUI Model */
    mutable unsigned int mId; /* (GUI) Model Id -> unique number */

    /* Old FileInfo Entries */
public:

    static const int kRsFiStatusNone = 0;
    static const int kRsFiStatusStall = 1;
    static const int kRsFiStatusProgress = 2;
    static const int kRsFiStatusDone = 2;

    /* FileInfo(); */

    int searchId;      /* 0 if none */

    //These items are synchronized, in that the first item of each refer to one unified set, the 2nd another, etc.
    QStringList paths;
    QStringList librarymixer_names;
    QList<int> orig_item_ids;

    std::string hash;
    std::string ext;

    uint64_t size; // In bytes
    uint64_t avail; /* how much we have */
    int status;


    /* Transfer Stuff */
    uint64_t transfered; //Amount transferred in bytes
    double   tfRate; /* transfer rate total of all peers in kbytes */
    uint32_t  downloadStatus; /* 0 = Err, 1 = Ok, 2 = Done */
    std::list<TransferInfo> peers;

    time_t lastTS;
};

//a pending LibraryMixer request that has not yet been turned into a download
class pendingRequest {
public:
    pendingRequest() :status(REPLY_NONE) {}
    int librarymixer_id; //librarymixer id of the user that is our source
    int item_id; //librarymixer item id
    QString name; //librarymixer name

    enum requestStatus {
        REPLY_NONE, //When no reply has been received yet
        REPLY_INTERNAL_ERROR //When response has totally failed for unknown reasons
    };
    requestStatus status;


    uint32_t timeOfLastTry;
};

std::ostream &operator<<(std::ostream &out, const FileInfo &info);


/* matched to the uPnP states */
#define UPNP_STATE_UNINITIALISED  0
#define UPNP_STATE_UNAVAILABILE   1
#define UPNP_STATE_READY          2
#define UPNP_STATE_FAILED_TCP     3
#define UPNP_STATE_FAILED_UDP     4
#define UPNP_STATE_ACTIVE         5

class NetConfig {
public:
    std::string     localAddr;
    int         localPort;
    std::string     extAddr;
    int         extPort;
    std::string     extName;

    /* older data types */
    bool            DHTActive;
    bool            uPnPActive;

    int         uPnPState;

    /* Flags for Network Status */
    bool            netOk;     /* That we've talked to someone! */
    bool            netUpnpOk; /* upnp is enabled and active */
    bool            netDhtOk;  /* response from dht */
    bool            netExtOk;  /* know our external address */
    bool            netUdpOk;  /* recvd stun / udp packets */
    bool            netTcpOk;  /* recvd incoming tcp */
    bool            netResetReq;
};

#endif


