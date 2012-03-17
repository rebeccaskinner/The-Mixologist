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

#include "tcponudp/udplayer.h"
#include "tcponudp/tou_net.h"
#include "util/debug.h"
#include "pqi/pqinotify.h"

static const int UDP_DEF_TTL = 64;

UdpLayer::UdpLayer(UdpReceiver *udpr, struct sockaddr_in &local)
    :udpReceiver(udpr), errorState(0), ttl(UDP_DEF_TTL) {
    if (!openSocket(local)) {
        getPqiNotify()->AddSysMessage(SYS_ERROR,
                                      "Network failure",
                                      QString("Unable to open UDP port ") + inet_ntoa(local.sin_addr) +
                                      ":" + QString::number(local.sin_port));
    }
    return;
}

UdpLayer::~UdpLayer() {
    close();
}

void UdpLayer::close() {
    QMutexLocker stack(&sockMtx);
    if (sockfd > 0) tounet_close(sockfd);
}

void UdpLayer::run() {
    int maxsize = 16000;
    void *receivedData = malloc(maxsize);

    int selectStatus;
    struct timeval timeout;

    while (true) {
        /* Repeatedly loop on select until a packet is available to read, and then break and read it. */
        fd_set rset;
        while (true) {
            if (sockfd < 0) break;
            FD_ZERO(&rset);
            FD_SET((unsigned int)sockfd, &rset);
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000; /* 500 ms timeout */
            selectStatus = select(sockfd+1, &rset, NULL, NULL, &timeout);
            if (selectStatus > 0) {
                break;  /* data available, go read it */
            } else if (selectStatus < 0) {
                log(LOG_DEBUG_ALERT, UDPLAYERZONE, QString("UdpLayer::recv_loop() Error: ") + QString::number(tounet_errno()));
            }
        }

        int nsize = maxsize;
        struct sockaddr_in from;
        if (0 < receiveUdpPacket(receivedData, &nsize, from)) {
            udpReceiver->recvPkt(receivedData, nsize, from);
        }
    }
}

int UdpLayer::sendPkt(void *data, int size, const sockaddr_in *to, int ttl) {
    /* If ttl is different then set it. */
    if (ttl != getTTL()) setTTL(ttl);

    sendUdpPacket(data, size, to);
    return size;
}

bool UdpLayer::openSocket(struct sockaddr_in &laddr) {
    {
        QMutexLocker stack(&sockMtx);

        /* Create the UDP socket. */
        sockfd = tounet_socket(PF_INET, SOCK_DGRAM, 0);

        /* Bind a listener to address. */
        if (0 != tounet_bind(sockfd, (struct sockaddr *) (&laddr), sizeof(laddr))) {
            errorState = EADDRINUSE;
            return false;
        }

        /* Set the socket to non-blocking. */
        if (-1 == tounet_fcntl(sockfd, F_SETFL, O_NONBLOCK)) {
            return false;
        }

        errorState = 0;
    }

    /* Set the TTL. */
    setTTL(UDP_DEF_TTL);

    return true;
}

int UdpLayer::setTTL(int newTTL) {
    QMutexLocker stack(&sockMtx);

    int err = tounet_setsockopt(sockfd, IPPROTO_IP, IP_TTL, &newTTL, sizeof(int));
    ttl = newTTL;

    return err;
}

int UdpLayer::getTTL() {
    QMutexLocker stack(&sockMtx);
    return ttl;
}

/* monitoring / updates */
bool UdpLayer::okay() {
    QMutexLocker stack(&sockMtx);
    bool thingsAreOkay = ((errorState == 0) ||
                          (errorState == EAGAIN) ||
                          (errorState == EINPROGRESS));

    if (!thingsAreOkay) log(LOG_DEBUG_ALERT, UDPLAYERZONE, QString("UdpLayer::okay() Error: ") + QString::number(errorState));
    return thingsAreOkay;
}

int UdpLayer::receiveUdpPacket(void *data, int *size, struct sockaddr_in &from) {
    struct sockaddr_in fromaddr;
    socklen_t fromsize = sizeof(fromaddr);
    int insize = *size;

    {
        QMutexLocker stack(&sockMtx);
        insize = tounet_recvfrom(sockfd, data, insize, 0, (struct sockaddr *)&fromaddr, &fromsize);
    }

    if (insize < 1) return -1;

    *size = insize;
    from = fromaddr;
    return insize;
}

int UdpLayer::sendUdpPacket(const void *data, int size, const struct sockaddr_in *to) {

    struct sockaddr_in toaddr = *to;

    QMutexLocker stack(&sockMtx);
    return tounet_sendto(sockfd, data, size, 0, (struct sockaddr *) &(toaddr), sizeof(toaddr));

//    QMutexLocker stack(&sockMtx);
//    return tounet_sendto(sockfd, data, size, 0, (struct sockaddr *) to, sizeof(*to));
}

#ifdef false
std::string printPkt(void *d, int size) {
    std::ostringstream out;
    out << "Packet:" << "**********************";
    for (int i = 0; i < size; i++) {
        if (i % 16 == 0)
            out << std::endl;
        out << std::hex << std::setw(2) << (unsigned int) ((unsigned char *) d)[i] << " ";
    }
    out << std::endl << "**********************";
    out << std::endl;
    return out.str();
}


std::string printPktOffset(unsigned int offset, void *d, unsigned int size) {
    std::ostringstream out;
    out << "Packet:" << "**********************";
    out << std::endl;
    out << "Offset: " << std::hex << offset << " -> " << offset + size;
    out << std::endl;
    out << "Packet:" << "**********************";

    unsigned int j = offset % 16;
    if (j != 0) {
        out << std::endl;
        out << std::hex << std::setw(6) << (unsigned int) offset - j;
        out << ": ";
        for (unsigned int i = 0; i < j; i++) {
            out << "xx ";
        }
    }
    for (unsigned int i = offset; i < offset + size; i++) {
        if (i % 16 == 0) {
            out << std::endl;
            out << std::hex << std::setw(6) << (unsigned int) i;
            out << ": ";
        }
        out << std::hex << std::setw(2) << (unsigned int) ((unsigned char *) d)[i-offset] << " ";
    }
    out << std::endl << "**********************";
    out << std::endl;
    return out.str();
}
#endif
