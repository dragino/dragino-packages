/*
 * transport.h
 * 
 */

#ifndef _TRANSPORT_H
#define _TRANSPORT_H

#include "linkedlists.h"

typedef enum {
	semtech,
	ttn_gw_bridge,
	gwtraf
} server_type;

typedef enum {     /* thread type for control thread head */
    rxpkts,
    semtech_up,
    semtech_down,
    ttn_up,
    ttn_down,
    gwtraf,
    gps,
    t_valid,
    jit,
    timersync,
    watchdog
} thread_type;

typedef struct _thread_info {
    LGW_LIST_ENTRY(struct _thread_info) list;
    pthread_t *thrid;
    thread_type thrtype;
    bool runing;
    time_t dog;
    sem_t *sema;
} thread_info;

typedef struct _rxpkts {   /* rx packages receive from radio or socke */
    LGW_LIST_ENTRY(struct _rxpkts) list;
    bool deal;
    uint8_t nb_pkt;
    uint8_t bind;
    struct lgw_pkt_rx_s rxpkt[NB_PKT_MAX];
} rxpkts;

typedef struct _serv_rxpkts {  /* pkts elem of server */
    LGW_LIST_ENTRY(struct _serv_rxpkts) list;
    uint8_t nb_pkt;
    struct lgw_pkt_rx_s *rxpkt;
} serv_rxpkts;

LGW_LIST_HEAD(SVRXPKTS, serv_rxpkts);  /* pkts list head of rxpkts for server */

typedef struct {
	char addr[64];				// server address
	char port_up[8];			// uplink port for semtech proto
	char port_down[8];			// downlink port for semtech proto
	int sock_up;				// Semtech up socket
	int sock_down;				// Semtech down socket
	bool upstream;				// upstream enabled
	bool downstream;			// downstream enabled
	bool statusstream;			// status stream enabled
} server_net;

typedef struct {
	char gw_id[64];				// gateway ID for TTN
	char gw_key[200];			// gateway key to connect to TTN
	int gw_port;				// gateway port
	time_t contact;				// time of last contact
	bool critical;				// Transport critical? Should connect at startup?
    uint8_t filter_fport;       // except or include ( 0 or 1) if except, forward except 
    uint8_t filter_addrdev;     // data save in database       if include, forward include
} gw_info;

typedef struct {
	int max_stall;				// max number of missed responses
    int stall_time;
    char stat_format[16];       // format for json statistics
    char status_report[STATUS_SIZE];
    bool report_ready;
    Stat_up stat_up;
    Stat_down stat_down;
	pthread_mutex_t mx_stat_rep;	// control access to the queue for each server
} gw_status;

typedef struct {
	pthread_t t_down;			// semtech down thread
	pthread_t t_up;				// upstream thread
} serv_thread;

typedef struct _server {
    LGW_LIST_ENTRY(struct _server) list;
    struct SVRXPKTS rxpkts;     // rxpkts list head 
	server_type type;		// type of server
    char server_name[32];        // identify of server
	bool enabled;				// server enabled
	bool live;					// Server is life?
	bool connecting;			// Connection setup in progress
	sem_t sema;				    // semaphore for sending data
    gw_info gwinfo;
	TTN *ttn;					// TTN connection object
} Server;


//LGW_LIST_HEAD_NOLOCK(onservice, Server);  /* servers link which are on service */
//LGW_LIST_HEAD(svpkts, Pkt_entry);     /* pkts link which on server recive from nodes */
//LGW_LIST_HEAD_NOLOCK(joblist, Threads);  /* threads link */

bool transport_init(Server *server);
bool transport_start(Server *server);
bool transport_stop(Server *server);
bool transport_status(Server *server);
bool transport_data_up(Server *server, int nb_pkt, struct lgw_pkt_rx_s *rxpkt, bool send_report);
bool transport_status_up(Server *server, uint32_t, uint32_t, uint32_t, uint32_t);
bool transport_send_downtraf(Server *server, char *json, int len);

int init_sock(const char *addr, const char *port, const char *timeout, int size);
#endif							// _TRANSPORT_H
