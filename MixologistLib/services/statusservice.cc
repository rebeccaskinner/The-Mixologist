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

#include <services/statusservice.h>
#include <serialiser/statusitems.h>
#include <interface/peers.h>

/*Time between each wave of status updates
  150 seconds (two and a half minutes)
  Note that this is exactly half of the timeout period, so we should
  have two opportunities per timeout period to keep alive. */
#define RETRY_DELAY 150

StatusService::StatusService()
    :p3Service(SERVICE_TYPE_STATUS) {
    addSerialType(new StatusSerialiser());
    timeOfLastTry = time(NULL);
}

int StatusService::tick() {
    if (time(NULL) - timeOfLastTry > RETRY_DELAY) {
        std::list<int> friends;
        std::list<int>::iterator it;
        peers->getOnlineList(friends);
        for (it = friends.begin(); it != friends.end(); it++) {
            StatusItem *item = new StatusItem();
            item->PeerId(peers->findCertByLibraryMixerId(*it));
            sendItem(item);
        }
        timeOfLastTry = time(NULL);
    }
    NetItem *netitem ;
    while ((netitem=recvItem()) != NULL) {
        StatusItem *status = dynamic_cast<StatusItem *>(netitem);
        if (status != NULL) {
            delete status;
        }
    }
    return 1;
}
