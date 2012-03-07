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

#include "stun/stunhandler.h"
#include "util/debug.h"
#include <iostream>

#include <pjlib.h>
#include <pjnath.h>

#ifdef false
//pj_caching_pool	cp;
/pj_pool_t *pool;
pj_stun_config stun_cfg;

int stunHandler::findExternalAddress(const QString &sourceAddress, unsigned short sourcePort) {
    /* Initialize the libraries before anything else */
    pj_init();
    pjlib_util_init();
    pjnath_init();

    /* Silence pjsip debug output.
       Level 1 = errors only. */
    //TODO pj_log_set_level(1);

    /* Must create pool factory, where memory allocations come from */
    //pj_caching_pool_init(&cp, NULL, 0);

    /* Init our ICE settings with null values */
    pj_stun_config_init(&stun_cfg, NULL, 0, NULL, NULL);

    //stun_cfg.pf = &cp.factory;

    /* Create transmit data */
    //pj_stun_tx_data *tdata = NULL;
    pj_status_t status;

    /* Create pool and initialize basic tdata attributes */
    pool = pj_pool_create(stun_cfg->pf, "tdata%p",  PJNATH_POOL_LEN_STUN_TDATA, PJNATH_POOL_INC_STUN_TDATA, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    //tdata = PJ_POOL_ZALLOC_T(pool, pj_stun_tx_data);
    //tdata->pool = pool;
    //tdata->sess = sess;

    //pj_list_init(tdata);

    /* Create STUN message */
    pj_stun_msg *stun_msg;
    status = pj_stun_msg_create(pool, PJ_STUN_BINDING_REQUEST,  PJ_STUN_MAGIC, NULL, &stun_msg);
    if (status != PJ_SUCCESS) {
        pj_pool_release(pool);
        return status;
    }

    return PJ_SUCCESS;
    return 1;
}
#endif

/* stun_on_status is a callback that handles when we recieve the stun response. */
static pj_bool_t stun_on_status(pj_stun_sock *stun_sock, pj_stun_sock_op op, pj_status_t status);

/* worker_thread() and handle_events() are used by pjsip to handle its sending and receiving. */
static int worker_thread(void *unused);
static pj_status_t handle_events(unsigned max_msec, unsigned *p_count);

/* When set to true, the worker_thread will quit. */
pj_bool_t thread_quit_flag;

pj_caching_pool	cp;
pj_pool_t *pool;
pj_stun_config stun_cfg;

int stunHandler::findExternalAddress(const QString &sourceAddress, unsigned short sourcePort) {
    int status;

    /* Initialize the libraries before anything else */
    pj_init();
    pjlib_util_init();
    pjnath_init();

    /* Silence pjsip debug output.
       Level 1 = errors only. */
    //TODO pj_log_set_level(1);

    /* Must create pool factory, where memory allocations come from */
    pj_caching_pool_init(&cp, NULL, 0);

    /* Init our ICE settings with null values */
    pj_stun_sock_cfg stun_sock_cfg;

    pj_stun_config_init(&stun_cfg, NULL, 0, NULL, NULL);
    pj_stun_sock_cfg_default(&stun_sock_cfg);

    stun_cfg.pf = &cp.factory;

    /* Convert the sourceAddress into a writeable char* */
    std::string stringAddress = sourceAddress.toStdString();
    char *charAddress = new char[stringAddress.size() + 1];
    std::copy(stringAddress.begin(), stringAddress.end(), charAddress);
    charAddress[stringAddress.size()] = '\0';
    pj_str_t address = pj_str(charAddress);

    /* Set the source address and port to the socket config. */
    stun_sock_cfg.bound_addr.ipv4.sin_family = pj_AF_INET();
    pj_inet_aton(&address, &stun_sock_cfg.bound_addr.ipv4.sin_addr);
    stun_sock_cfg.bound_addr.ipv4.sin_port = pj_htons(sourcePort);

    delete[] charAddress;

    /* Create application memory pool */
    pool = pj_pool_create(&cp.factory, "Mixologist-PJNATH", 512, 512, NULL);

    /* Create timer heap for timer stuff */
    pj_timer_heap_create(pool, 100, &stun_cfg.timer_heap);

    /* and create ioqueue for network I/O stuff */
    pj_ioqueue_create(pool, 16, &stun_cfg.ioqueue);

    /* something must poll the timer heap and ioqueue,
     * unless we're on Symbian where the timer heap and ioqueue run
     * on themselves.
     */
    pj_thread_t *thread;
    pj_thread_create(pool, "Mixologist-PJNATH", &worker_thread, NULL, 0, 0, &thread);

    pj_stun_sock_cb stun_sock_cb;
    //pj_ice_sess_cand *cand;

    pj_bzero(&stun_sock_cb, sizeof(stun_sock_cb));
    stun_sock_cb.on_status = &stun_on_status;

    /* Create the STUN transport */
    pj_stun_sock *stun_sock;
    status = pj_stun_sock_create(&stun_cfg, NULL,
                                 pj_AF_INET(), &stun_sock_cb,
                                 &stun_sock_cfg,
                                 NULL, &stun_sock);
    if (status != PJ_SUCCESS) return status;

    /* Start STUN Binding resolution and add srflx candidate
     */

    /* Start Binding resolution */
    pj_str_t server = pj_str("stun.selbie.com");
    status = pj_stun_sock_start(stun_sock,
                                &server,
                                PJ_STUN_PORT,
                                NULL);
    if (status != PJ_SUCCESS) {
        ///sess_dec_ref(ice_st);
        return status;
    }

    /* Enumerate addresses */
    pj_stun_sock_info stun_sock_info;
    status = pj_stun_sock_get_info(stun_sock, &stun_sock_info);
    if (status != PJ_SUCCESS) {
        return status;
    }

    /* Add local addresses to host candidates, unless max_host_cands
     * is set to zero.
     */
    unsigned i;
    for (i=0; i<stun_sock_info.alias_cnt; ++i)
    {
        char addrinfo[PJ_INET6_ADDRSTRLEN+10];
        const pj_sockaddr *addr = &stun_sock_info.aliases[i];


        /* Ignore loopback addresses unless cfg->stun.loop_addr
         * is set
         */
        if ((pj_ntohl(addr->ipv4.sin_addr.s_addr)>>24)==127) {
            continue;
        }

        /*
        cand = &comp->cand_list[comp->cand_cnt++];

        cand->type = PJ_ICE_CAND_TYPE_HOST;
        cand->status = PJ_SUCCESS;
        cand->local_pref = 0;
        cand->transport_id = TP_STUN;
        cand->comp_id = (pj_uint8_t) comp_id;
        pj_sockaddr_cp(&cand->addr, addr);
        pj_sockaddr_cp(&cand->base_addr, addr);
        pj_bzero(&cand->rel_addr, sizeof(cand->rel_addr));
        pj_ice_calc_foundation(ice_st->pool, &cand->foundation,
                               cand->type, &cand->base_addr);

        PJ_LOG(4,(ice_st->obj_name,
                  "Comp %d: host candidate %s added",
                  comp_id, pj_sockaddr_print(&cand->addr, addrinfo,
                                             sizeof(addrinfo), 3)));*/
    }

    std::cerr << "Init returning!\n";

    return PJ_SUCCESS;
}

/* Notification when the status of the STUN transport has changed. */
static pj_bool_t stun_on_status(pj_stun_sock *stun_sock, pj_stun_sock_op op, pj_status_t status) {
    if (op == PJ_STUN_SOCK_BINDING_OP) {
        
        pj_stun_sock_info stun_sock_info;
        status = pj_stun_sock_get_info(stun_sock, &stun_sock_info);
        char txt[PJ_INET_ADDRSTRLEN];
        pj_inet_ntop(pj_AF_INET(), pj_sockaddr_get_addr(&stun_sock_info.bound_addr), txt, sizeof(txt));
        QString boundAddress(txt);
        pj_inet_ntop(pj_AF_INET(), pj_sockaddr_get_addr(&stun_sock_info.mapped_addr), txt, sizeof(txt));
        QString mappedAddress(txt);

        std::cerr << (QString("Bound ") +
                     boundAddress +
                     " : " +
                     QString::number(pj_ntohs(stun_sock_info.bound_addr.ipv4.sin_port)) +
                     " on " +
                     mappedAddress +
                     " : " +
                     QString::number(pj_ntohs(stun_sock_info.mapped_addr.ipv4.sin_port)) +
                     "\n").toStdString();
        thread_quit_flag = true;

    }
    return PJ_TRUE;
}

/* handle_events and worker_thread are copied almost verbatim from the pjsip example file, icedemo.c */
static pj_status_t handle_events(unsigned max_msec, unsigned *p_count) {
    enum { MAX_NET_EVENTS = 1 };
    pj_time_val max_timeout = {0, 0};
    pj_time_val timeout = { 0, 0};
    unsigned count = 0, net_event_count = 0;
    int c;

    max_timeout.msec = max_msec;

    /* Poll the timer to run it and also to retrieve the earliest entry. */
    timeout.sec = timeout.msec = 0;
    c = pj_timer_heap_poll(stun_cfg.timer_heap, &timeout );
    if (c > 0) count += c;

    /* timer_heap_poll should never ever returns negative value, or otherwise
     * ioqueue_poll() will block forever!
     */
    pj_assert(timeout.sec >= 0 && timeout.msec >= 0);
    if (timeout.msec >= 1000) timeout.msec = 999;

    /* compare the value with the timeout to wait from timer, and use the
     * minimum value.
    */
    if (PJ_TIME_VAL_GT(timeout, max_timeout)) timeout = max_timeout;

    /* Poll ioqueue.
     * Repeat polling the ioqueue while we have immediate events, because
     * timer heap may process more than one events, so if we only process
     * one network events at a time (such as when IOCP backend is used),
     * the ioqueue may have trouble keeping up with the request rate.
     *
     * For example, for each send() request, one network event will be
     *   reported by ioqueue for the send() completion. If we don't poll
     *   the ioqueue often enough, the send() completion will not be
     *   reported in timely manner.
     */
    do {
        c = pj_ioqueue_poll(stun_cfg.ioqueue, &timeout);
        if (c < 0) {
            pj_status_t err = pj_get_netos_error();
            pj_thread_sleep(PJ_TIME_VAL_MSEC(timeout));
            if (p_count) *p_count = count;
            return err;
        } else if (c == 0) {
            break;
        } else {
            net_event_count += c;
            timeout.sec = timeout.msec = 0;
        }
    } while (c > 0 && net_event_count < MAX_NET_EVENTS);

    count += net_event_count;
    if (p_count) *p_count = count;

    return PJ_SUCCESS;

}

static int worker_thread(void *unused) {
    /* I worked on converting all of this over to using QThreads, turning this method into run().
       In order to do so, it was necessary to call pj_thread_register() here to be able to call pjsip functions in handle_events.
       However, after doing so, whenever the thread stopped (i.e. run exited) it would cause a segmentation fault.
       For now, until we stop using the pjsip sockets for sending stun, and send stun directly,
       it seems easier rather than fighting weird bugs like this, to simply use the pj_threads. */
    PJ_UNUSED_ARG(unused);

    while (!thread_quit_flag) {
        handle_events(500, NULL);
    }

    return 0;
}
