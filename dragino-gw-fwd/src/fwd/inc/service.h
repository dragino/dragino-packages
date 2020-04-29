/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino_gw_fwd -- An opensource lora gateway forward 
 *
 * See http://www.dragino.com for more information about
 * the lora gateway project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Maintainer: skerlan
 *
 */

/*!
 * \file
 * \brief Persistant data storage 
 */

#ifndef _SERVICE_H
#define _SERVICE_H

#include "fwd.h"
#include "gwcfg.h"

typedef enum {
	semtech,
	ttn,
    ghost,
    mqtt,
	gwtraf
} serv_type;

typedef struct _rxpkts {   /* rx packages receive from radio or socke */
    LGW_LIST_ENTRY(struct _rxpkts) list;
    bool deal;
    uint8_t nb_pkt;
    uint8_t bind;
    struct lgw_pkt_rx_s rxpkt[NB_PKT_MAX];
} rxpkts_s;

typedef struct _serv_rxpkts {  /* pkts elem of server */
    LGW_LIST_ENTRY(struct _serv_rxpkts) list;
    uint8_t nb_pkt;
    struct lgw_pkt_rx_s** rxpkt;
} serv_rxpkts_s;

LGW_LIST_HEAD(pkts_list, serv_rxpkts_s);           // pkts list head of rxpkts for server 

/*!
 * \brief server是一个描述什么样服务的数据结构
 * 
 */
typedef struct _server {
    LGW_LIST_ENTRY(struct _server) list;
    rxpkts_s* rxpkt_set;                     // rxpkts list head 

    struct {
	    server_type type;		    // type of server
        char  serv_name[32];        // identify of server
        char* serv_id;		        // gateway ID for service
        char* serv_key;			    // gateway key to connect to  service
    } info;

    struct {
        char addr[64];				// server address
        char port_up[8];			// uplink port 
        char port_down[8];			// downlink port 
        int  sock_up;				// up socket
        int  sock_down;				// down socket
        int  pull_interval;	        // send a PULL_DATA request every X seconds 
        struct timeval push_timeout_half;       /* time-out value (in ms) for upstream datagrams */
        struct timeval pull_timeout;
    } net;

    struct {
        uint8_t fport;              /* 0/1/2, 0不处理，1如果过滤匹配数据库的，2转发匹配数据库的 */
        uint8_t devaddr;            /* 和fport相同 */
        bool fwd_valid_pkt;         /* packets with PAYLOAD CRC OK are forwarded */
        bool fwd_error_pkt;         /* packets with PAYLOAD CRC ERROR are NOT forwarded */
        bool fwd_nocrc_pkt;         /* packets with NO PAYLOAD CRC are NOT forwarded */
    } filter;

    struct {
        bool live;					// Server is life?
        bool connecting;			// Connection setup in progress
        int  max_stall;				// max number of missed responses
        int  stall_time;
        time_t contact;				// time of last contact
    } state;
    
    struct {
        char* stat_format;               // format for json statistics
        char* status_report;
        bool report_ready;
        unsigned stat_interval; 	     // time interval (in sec) at which statistics are collected and displayed
        stat_up_s*   stat_up;
        stat_down_s* stat_down;
        pthread_mutex_t* mx_report;	 // control access to the queue for each server
    } report;

    struct {
        pthread_t t_down;			// semtech down thread
        pthread_t t_up;				// upstream thread
        sem_t sema;				    // semaphore for sending data
    } thread;

    gw_s* gw;
} serv_s;

LGW_LIST_HEAD_NOLOCK(serv_list, serv_s);           // pkts list head of rxpkts for server 


/*!
 * \brief rxpkt分发处理
 * 
 */
int service_handle_rxpkt(gw_s* gw, rxpkts_s* rxpkt);

/*!
 * \brief rxpkt分发处理
 * 
 */
int service_start(gw_s* gw);

/*!
 * \brief rxpkt分发处理
 * 
 */
void service_stop(gw_s* gw);

/*!
 * \brief 准备一个网络文件描述符，用于后续的连接
 * \param timeout 是一个连接的超时时间，用来中断连续，避免长时间阻塞
 */
int init_sock(const char* addr, const char* port, const void* timeout, int size);

#endif							
