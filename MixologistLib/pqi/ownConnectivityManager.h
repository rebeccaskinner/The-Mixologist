/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2007-8, Robert Fernie
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

#ifndef OWN_CONNECTIVITY_MANAGER_H
#define OWN_CONNECTIVITY_MANAGER_H

#include <pqi/authmgr.h>
#include <pqi/pqinetwork.h>
#include <interface/peers.h>

#include <QObject>
#include <QMutex>
#include <QMap>
#include <QStringList>

class OwnConnectivityManager;
class upnpHandler;
class UdpSorter;

extern OwnConnectivityManager *ownConnectivityManager;
/* This UdpSorter is the main UDP socket that is used for all data transfers over UDP. */
extern UdpSorter* udpMainSocket;

/**********************************************************************************
 * This is the master connectivity manager for a user's own connection.
 * Handles initial set up own connection such as ports and UPNP mappings.
 **********************************************************************************/

class OwnConnectivityManager: public QObject {
    Q_OBJECT

public:
    OwnConnectivityManager();

    /* Ticked from the main server thread. */
    void tick();

    /**********************************************************************************
     * Setup
     **********************************************************************************/

    /* Sets our own initial IP address and port network interface that will be used by the Mixologist.
       Opens the UDP and TCP ports that will be used by the Mixologist.
       Called once on init, must be called before the main server begins calling tick(). */
    void select_NetInterface_OpenPorts();

    /* Blocking call that shuts down our connection.
       Clears our address and port information.
       Closes the UDP and TCP ports.
       Shuts down UPNP if applicable.
       Resets all variables to initial state. */
    void shutdown();

    /* In the case of a total failure to get our address info ourself, instead of uploading our address,
       we will request that LibraryMixer sets our shared external IP to be whatever it sees.
       It then returns this to us so we can set it here as well.
       This is fine for the address, as that will be reliable, but if we reach this point, that means we have totally failed to verify our port. */
    void setFallbackExternalIP(QString address);

    /**********************************************************************************
     * MixologistLib functions
     **********************************************************************************/

    /* Returns our own local address. */
    struct sockaddr_in* getOwnLocalAddress() {return &ownLocalAddress;}

    /* Returns our own local address. */
    struct sockaddr_in* getOwnExternalAddress() {return &ownExternalAddress;}

    /* Returns the state of our connection set up. */
    ConnectionStatus getConnectionStatus() {return connectionStatus;}

signals:
    /* Used to inform the GUI of changes to the current ConnectionStatus.
       All values of newStatus should be members of ConnectionStatus. */
    void connectionStateChanged(int newStatus);

private slots:
    /* Connected to the UDP Sorter for when we receive a STUN packet. */
    void receivedStunBindingResponse(QString transactionId, QString mappedAddress, ushort mappedPort, ushort receivedOnPort, QString receivedFromAddress);

    /* Connected to the LibraryMixerConnect so we know when we have updated LibraryMixer with our new address. */
    void addressUpdatedOnLibraryMixer();

private:
    /**********************************************************************************
     * Setup Helpers
     **********************************************************************************/
    /* Handles setting up our own connection, and maintains it after set up. */
    void netTick();

    /* Handles when we need to upload updated address info to LibraryMixer.
       Returns true when there's nothing to do, false when we're trying to contact LibraryMixer. */
    void contactLibraryMixerTick();

    /* Makes sure that we haven't moved to a different network, invalidating our old network interfaces. */
    void checkNetInterfacesTick();

    /* If port randomization is not set, returns the saved port if it is valid.
       If port randomization is set, then generates a random port.
       If there is no saved port, generates a random port and saves it. */
    int getLocalPort();

    /* Returns a random number that is suitable for use as a port. */
    int getRandomPortNumber() const;

    /* Sets the connectionStatus to newStatus and clears out any state-dependent variables and emits the signal. */
    void setNewConnectionStatus(ConnectionStatus newStatus);

    /* Integrated method that handles basically an entire connection set up step that is based around a STUN request.
       If we haven't sent a stun packet to this stunServer yet, sends one using sendSocket and marks it sent.
       If we have sent a stun packet, checks if we have hit the timeout.
       If we have hit the timeout, returns -1, if we sent a STUN packet returns 1, otherwise if we're waiting, returns 0.
       If a return port is provided then the STUN packet will include that as a Response-Port attribute. */
    int handleStunStep(const QString& stunServerName, const struct sockaddr_in *stunServer, UdpSorter *sendSocket, ushort returnPort = 0);

    /* Sends a STUN packet.
       If a return port is provided then the STUN packet will include that as a Response-Port attribute. */
    bool sendStunPacket(const QString& stunServerName, const struct sockaddr_in *stunServer, UdpSorter* sendSocket, ushort returnPort = 0);

    /* Checks and makes sure UPNP is still properly functioning periodically.
       Called from netTick.
       Can be called any number of times between interval periods and will not fire more than once per interval. */
    void upnpMaintenance();

    mutable QMutex ownConMtx;

    /* Information about our own connection. */
    struct sockaddr_in ownLocalAddress;
    struct sockaddr_in ownExternalAddress;

    std::list<std::string> networkInterfaceList;

    /* The current state of the connection's setup.
       The ConnectionStatus enum is defined in the peers interface and shared with the GUI. */
    ConnectionStatus connectionStatus;

    /* Whether we need to contact LibraryMixer with our address */
    enum ContactLibraryMixerState {
        /* Nothing needs to be done */
        CONTACT_LIBRARYMIXER_IDLE,
        /* Our own connection state is discovered, so update LibraryMixer with that information. */
        CONTACT_LIBRARYMIXER_UPDATE,
        /* Our own connection state discovery was a total failure, as a fallback have LibraryMixer set our IP information itself. */
        CONTACT_LIBRARYMIXER_GET_ADDRESS_FROM_LIBRARYMIXER
    };
    ContactLibraryMixerState contactLibraryMixerState;
    /* Whether we should enable FriendsConnectivityManager to begin attempting connections to friends after we finish updating LibraryMixer. */
    bool enableFriendsConnectAfterUpdate;

    /* For each step in our connectionStatus that involves sending STUN requests, this contains when we will consider that step to be timed out and failed. */
    time_t connectionSetupStepTimeOutAt;

    /* The udpTestSocket is a random port that we will use only for testing in our auto-connection configuration. */
    UdpSorter* udpTestSocket;

    /* Interface to UPNP connectivity. */
    upnpHandler *mUpnpMgr;

    /* Initially NULL, but we will step through our list of friends, and then fallback STUN servers is necessary,
       and identify two STUN servers that we will use. */
    struct sockaddr_in* stunServer1;
    struct sockaddr_in* stunServer2;

    /* STUN servers that will be used if we can't get two peers to act as STUN servers. */
    QStringList fallbackStunServers;

    /* Map by STUN transaction ID, these are STUN requests we have sent for our current connection configuration step. */
    struct pendingStunTransaction {
        /* If we set a STUN Response-Port attribute, then this will contain the port we expect to hear back on.
           Otherwise, this will be set to 0. */
        ushort returnPort;

        /* The address of the server we sent this to request to.
           This is used during the initial phases where we are attempting to discover STUN servers so that when we hear back, we know which server it was. */
        struct sockaddr_in serverAddress;

        /* The name to display in the logs for this server. */
        QString serverName;
    };
    QMap<QString, pendingStunTransaction> pendingStunTransactions;
};

#endif // OWN_CONNECTIVITY_MANAGER_H
