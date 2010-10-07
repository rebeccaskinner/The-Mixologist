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

#ifndef TOU_TCP_PROTO_H
#define TOU_TCP_PROTO_H


/* so the packet will contain
 * a tcp header + data.
 *
 * This is done simplistically for speed.
 */

#include "tcppacket.h"
#include "udpsorter.h"

#define MAX_SEG         1500
#define TCP_MAX_SEQ         UINT_MAX
#define TCP_MAX_WIN     65500
#define TCP_ALIVE_TIMEOUT   15      /* 15 sec ... < 20 sec UDP state limit on some firewalls */
#define TCP_RETRANS_TIMEOUT 1   /* 1 sec (Initial value) */
#define kNoPktTimeout       60  /* 1 min */


#define TCP_CLOSED  0
#define TCP_LISTEN  1
#define TCP_SYN_SENT    2
#define TCP_SYN_RCVD    3
#define TCP_ESTABLISHED 4
#define TCP_FIN_WAIT_1  5
#define TCP_FIN_WAIT_2  6
#define TCP_TIMED_WAIT  7
#define TCP_CLOSING     8
#define TCP_CLOSE_WAIT  9
#define TCP_LAST_ACK    10

class dataBuffer {
public:
    uint8 data[MAX_SEG];
};

#include <list>
#include <deque>


class TcpStream: public UdpPeer {
public:
    /* Top-Level exposed */

    TcpStream(UdpSorter *lyr);
    virtual ~TcpStream() {
        return;
    }

    /* user interface */
    int     status(std::ostream &out);
    int     connect(const struct sockaddr_in &raddr, uint32_t conn_period);
    int     listenfor(const struct sockaddr_in &raddr);
    bool    isConnected();

    /* get tcp information */
    bool    getRemoteAddress(struct sockaddr_in &raddr);
    uint8   TcpState();
    int TcpErrorState();

    /* stream Interface */
    int write(char *dta, int size); /* write -> pkt -> net */
    int read(char *dta, int size); /* net ->   pkt -> read */

    /* check ahead for allowed bytes */
    int write_allowed();
    int read_pending();

    int closeWrite(); /* non-standard, but for clean exit */
    int close(); /* standard unix behaviour */

    int tick(); /* check iface etc */

    /* Callback Funcion from UDP Layers */
    virtual void recvPkt(void *data, int size); /* overloaded */



    /* Exposed Data Counting */
    bool    widle(); /* write idle */
    bool    ridle(); /* read idle */
    uint32  wbytes();
    uint32  rbytes();

private:

    /* Internal Functions - use the Mutex (not reentrant) */
    /* Internal Functions - that don't need mutex protection */

    uint32  genSequenceNo();
    bool    isOldSequence(uint32 tst, uint32 curr);

    MixMutex tcpMtx;

    /* Internal Functions - only called inside mutex protection */

    int cleanup();

    /* incoming data */
    int     recv_check();
    int     handleIncoming(TcpPacket *pkt);
    int     incoming_Closed(TcpPacket *pkt);
    int     incoming_SynSent(TcpPacket *pkt);
    int     incoming_SynRcvd(TcpPacket *pkt);
    int     incoming_Established(TcpPacket *pkt);
    int     incoming_FinWait1(TcpPacket *pkt);
    int     incoming_FinWait2(TcpPacket *pkt);
    int     incoming_TimedWait(TcpPacket *pkt);
    int     incoming_Closing(TcpPacket *pkt);
    int     incoming_CloseWait(TcpPacket *pkt);
    int     incoming_LastAck(TcpPacket *pkt);
    int     check_InPkts();
    int     UpdateInWinSize();
    int int_read_pending();

    /* outgoing data */
    int send();
    int     toSend(TcpPacket *pkt, bool retrans = true);
    void    acknowledge();
    int retrans();
    int sendAck();
    void    setRemoteAddress(const struct sockaddr_in &raddr);

    int getTTL() {
        return ttl;
    }
    void    setTTL(int t) {
        ttl = t;
    }

    /* data counting */
    uint32  int_wbytes();
    uint32  int_rbytes();

    /* Internal Data - must have mutex to access! */

    /* data (in -> pkts) && (pkts -> out) */

    /* for small amounts of data */
    uint8 inData[MAX_SEG];
    uint32 inSize;


    /* two variable sized buffers required here */
    uint8 outDataRead[MAX_SEG];
    uint32 outSizeRead;
    uint8 outDataNet[MAX_SEG];
    uint32 outSizeNet;

    /* get packed into here as size increases */
    std::deque<dataBuffer *>   inQueue, outQueue;

    /* packets waiting for acks */
    std::list<TcpPacket *> inPkt, outPkt;


    uint8  state; /* stream state */
    bool   inStreamActive;
    bool   outStreamActive;

    uint32 outSeqno; /* next out */
    uint32 outAcked; /* other size has received */
    uint32 outWinSize; /* we allowed to send */

    uint32 inAckno; /* next expected */
    uint32 inWinSize; /* allowing other to send */
    uint32 rrt;

    /* some (initially) consts */
    uint32 maxWinSize;
    uint32 keepAliveTimeout;
    double retransTimeout;

    /* some timers */
    double keepAliveTimer;
    double lastIncomingPkt;

    /* tracking */
    uint32 lastSentAck;
    uint32 lastSentWinSize;
    uint32 initOurSeqno;
    uint32 initPeerSeqno;

    uint32 lastWriteTF,lastReadTF;
    uint16 wcount, rcount;

    int errorState;

    /* RoundTripTime estimations */
    double rtt_est;
    double rtt_dev;

    /* congestion limits */
    uint32 congestThreshold;
    uint32 congestWinSize;
    uint32 congestUpdate;

    /* existing TTL for this stream (tweaked at startup) */
    int ttl;

    double mTTL_period;
    double mTTL_start;
    double mTTL_end;

    struct sockaddr_in  peeraddr;
    bool            peerKnown;

    /* UdpSorter (has own Mutex!) */
    UdpSorter *udp;

};


/* for debugging */

#ifdef TCP_DEBUG_STREAM_EXTRA /* for extra checking! */
int     setupBinaryCheck(std::string fname);
#endif



#endif

