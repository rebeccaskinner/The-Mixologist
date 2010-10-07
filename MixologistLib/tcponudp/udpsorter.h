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

#ifndef TOU_UDP_SORTER_H
#define TOU_UDP_SORTER_H

/* universal networking functions */
#include "tou_net.h"

#include <iosfwd>
#include <map>

#include "udplayer.h"
/* UdpSorter ..... filters the UDP packets.
 */

class UdpPeer {
public:
    virtual void recvPkt(void *data, int size) = 0;
};



class TouStunPeer {
public:
    TouStunPeer()
        :response(false), lastsend(0), failCount(0) {
        return;
    }

    TouStunPeer(std::string id_in, const struct sockaddr_in &addr)
        :id(id_in), remote(addr), response(false), lastsend(0), failCount(0) {
        return;
    }

    std::string id;
    struct sockaddr_in remote, eaddr;
    bool response;
    time_t lastsend;
    uint32_t failCount;
};


class UdpSorter: public UdpReceiver {
public:

    UdpSorter(struct sockaddr_in &local);
    virtual ~UdpSorter() {
        return;
    }

    /* add a TCPonUDP stream */
    int addUdpPeer(UdpPeer *peer, const struct sockaddr_in &raddr);
    int     removeUdpPeer(UdpPeer *peer);

    bool    setStunKeepAlive(uint32_t required);
    bool    addStunPeer(const struct sockaddr_in &remote, const char *peerid);
    bool    checkStunKeepAlive();

    bool    externalAddr(struct sockaddr_in &remote, uint8_t &stable);

    /* Packet IO */
    /* pass-through send packets */
    int  sendPkt(void *data, int size, struct sockaddr_in &to, int ttl);
    /* callback for recved data (overloaded from UdpReceiver) */
    virtual void recvPkt(void *data, int size, struct sockaddr_in &from);

    int     status(std::ostream &out);

    /* setup connections */
    int openSocket();

    /* monitoring / updates */
    int okay();
    int tick();

    int close();

private:

    /* STUN handling */
    bool    locked_handleStunPkt(void *data, int size, struct sockaddr_in &from);

    int     doStun(struct sockaddr_in stun_addr);


    /* stun keepAlive */
    bool    locked_printStunList();
    bool    locked_recvdStun(const struct sockaddr_in &remote, const struct sockaddr_in &extaddr);
    bool    locked_checkExternalAddress();

    bool    storeStunPeer(const struct sockaddr_in &remote, const char *peerid);

    UdpLayer *udpLayer;

    MixMutex sortMtx; /* for all class data (below) */

    struct sockaddr_in laddr; /* local addr */

    struct sockaddr_in eaddr; /* external addr */
    bool eaddrKnown;
    bool eaddrStable; /* if true then usable. if false -> Symmettric NAT */

    bool mStunKeepAlive;
    time_t mStunLastRecv;
    time_t mStunLastSend;

    std::list<TouStunPeer> mStunList; /* potentials */

    std::map<struct sockaddr_in, UdpPeer *> streams;



};

/* generic stun functions */

bool    UdpStun_isStunPacket(void *data, int size);
bool    UdpStun_response(void *stun_pkt, int size, struct sockaddr_in &addr);
void   *UdpStun_generate_stun_reply(struct sockaddr_in *stun_addr, int *len);
bool    UdpStun_generate_stun_pkt(void *stun_pkt, int *len);

#endif
