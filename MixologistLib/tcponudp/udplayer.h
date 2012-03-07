/****************************************************************
 *  Copyright 2010, Fair Use, Inc.
 *  Copyright 2004-6, Robert Fernie
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

#ifndef TOU_UDP_LAYER_H
#define TOU_UDP_LAYER_H

/* universal networking functions */

#include <QThread>
#include <QMutex>

class UdpReceiver;

/**********************************************************************************
 * UdpLayer represents the UDP layer, which just sends and receives UDP packets.
 * On create, it will create a UDP socket and begin listening on the specified address.
 * It provides an interface on top of the tou_net.
 **********************************************************************************/

class UdpLayer: public QThread {
public:
    /* Creates a new UdpLayer, which will bind a new listening socket onto local. */
    UdpLayer(UdpReceiver *udpr, struct sockaddr_in &local);
    virtual ~UdpLayer() {}

    /* The thread loop, which constantly waits for incoming data,
       and then reports it to the UdpReceiver when it arrives. */
    virtual void run();

    /* Sends the specified packet. */
    int sendPkt(void *data, int size, struct sockaddr_in &to, int ttl);

    /* Returns whether any errors have occurred, true on okay. */
    bool okay();

    /* Closes the listener socket. */
    void close();

protected:
    /* Sets up the socket and starts listening on it. */
    int openSocket(struct sockaddr_in &local);

    /* Calls the tou_net to read in a UDP packet from the socket.
       Returns the amount of data read on success, or -1 on failure. */
    virtual int receiveUdpPacket(void *data, int *size, struct sockaddr_in &from);

    /* Calls the tou_net to send the specified packet. */
    virtual void sendUdpPacket(const void *data, int size, struct sockaddr_in &to);

    /* Sets a new TTL. */
    int setTTL(int newTTL);

    /* Returns the current set TTL. */
    int getTTL();

private:
    UdpReceiver *udpReceiver;

    /* Whether we have any errors, or 0 when no errors. */
    int errorState;

    /* The socket we are listening on. */
    int sockfd;

    /* The TTL for this socket. */
    int ttl;

    mutable QMutex sockMtx;
};

/**********************************************************************************
 * Interface class for classes that wish to be able to receive incoming packets from
 * the UdpLayer.
 **********************************************************************************/
class UdpReceiver {
private:
    /* Called when there is an incoming packet by UdpLayer. */
    virtual void recvPkt(void *data, int size, struct sockaddr_in &from) = 0;

    friend class UdpLayer;
};

//std::string printPkt(void *d, int size);
//std::string printPktOffset(unsigned int offset, void *d, unsigned int size);

/* This testing class is useful for making sure higher level functions work okay when UDP is losing packets. */
#ifdef false
class LossyUdpLayer: public UdpLayer {
public:

    LossyUdpLayer(UdpReceiver *udpr, struct sockaddr_in &local, double frac)
        :UdpLayer(udpr, local), lossFraction(frac) {
        return;
    }
    virtual ~LossyUdpLayer() {
        return;
    }

protected:

    virtual int receiveUdpPacket(void *data, int *size, struct sockaddr_in &from) {
        double prob = (1.0 * (rand() / (RAND_MAX + 1.0)));

        if (prob < lossFraction) {
            /* but discard */
            if (0 < UdpLayer::receiveUdpPacket(data, size, from)) {
                std::cerr << "LossyUdpLayer::receiveUdpPacket() Dropping packet!";
                std::cerr << std::endl;
                std::cerr << printPkt(data, *size);
                std::cerr << std::endl;
                std::cerr << "LossyUdpLayer::receiveUdpPacket() Packet Dropped!";
                std::cerr << std::endl;
            }

            size = 0;
            return -1;

        }

        // otherwise read normally;
        return UdpLayer::receiveUdpPacket(data, size, from);
    }


    virtual void sendUdpPacket(const void *data, int size, struct sockaddr_in &to) {
        double prob = (1.0 * (rand() / (RAND_MAX + 1.0)));

        if (prob < lossFraction) {
            /* discard */

            std::cerr << "LossyUdpLayer::sendUdpPacket() Dropping packet!";
            std::cerr << std::endl;
            std::cerr << printPkt((void *) data, size);
            std::cerr << std::endl;
            std::cerr << "LossyUdpLayer::sendUdpPacket() Packet Dropped!";
            std::cerr << std::endl;

            return size;
        }

        // otherwise read normally;
        return UdpLayer::sendUdpPacket(data, size, to);
    }

    double lossFraction;
};
#endif //false

#endif //TOU_UDP_LAYER_H
