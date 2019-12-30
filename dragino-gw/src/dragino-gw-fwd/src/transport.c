/*
 * transport.c
 *
 *  Created on: Feb 10, 2017
 *      Author: Jac Kersing
 */

#include <stdint.h>		/* C99 types */
#include <stdbool.h>		/* bool type */
#include <stdio.h>		/* printf, fprintf, snprintf, fopen, fputs */

#include <string.h>		/* memset */
#include <signal.h>		/* sigaction */
#include <time.h>		/* time, clock_gettime, strftime, gmtime */
#include <sys/time.h>		/* timeval */
#include <unistd.h>		/* getopt, access */
#include <stdlib.h>		/* atoi, exit */
#include <errno.h>		/* error messages */
#include <math.h>		/* modf */
#include <assert.h>

#include <sys/socket.h>		/* socket specific definitions */
#include <netinet/in.h>		/* INET constants and stuff */
#include <arpa/inet.h>		/* IP address conversion stuff */
#include <netdb.h>		/* gai_strerror */

#include <pthread.h>
#include <semaphore.h>

#include "dragino_gw_fwd.h"
#include "trace.h"
#include "loragw_hal.h"
#include "connector.h"
#include "transport.h"
#include "semtech_transport.h"
#include "ttn_transport.h"
#include "gwtraf_transport.h"

extern uint8_t serv_count;
extern Server servers[];

void transport_init();
void transport_start();
void transport_stop();
void transport_data_up(int nb_pkt, struct lgw_pkt_rx_s *rxpkt, bool send_report);

// Initialize all data structures
void transport_init() {
    int i;
    for (i = 0; i < MAX_SERVERS; i++) {
	memset(&servers[i], 1, sizeof(Server));
	servers[i].type = semtech;
	servers[i].enabled = false;
	servers[i].upstream = true;
	servers[i].downstream = true;
	servers[i].statusstream = true;
	servers[i].live = false;
	servers[i].connecting = false;
	servers[i].critical = true;
	servers[i].sock_up = -1;
	servers[i].sock_down = -1;
	servers[i].queue = NULL;
        pthread_mutex_init(&servers[i].mx_queue, NULL); /* if !=0 ERROR */
	sem_init(&servers[i].send_sem, 0, 0);
    }
}

void transport_start() {
    int i;

    MSG("INFO: [Transports] Initializing protocol for %d servers\n", serv_count);
    for (i = 0; i < MAX_SERVERS; i++) {
	if (servers[i].enabled == true) {
	    switch (servers[i].type) {
                case semtech:
                    semtech_init(i);
                    break;
                case ttn_gw_bridge:
                    ttn_init(i);
                    break;
                case gwtraf:
                    gwtraf_init(i);
                    break;
                case mqtt:
                    break;
	    }
	}
    }
}

void transport_stop() {
    int i;

    for (i = 0; i < MAX_SERVERS; i++) {
	if (servers[i].enabled == true) {
	    switch (servers[i].type) {
	    case semtech:
		semtech_stop(i);
		break;
	    case ttn_gw_bridge:
		ttn_stop(i);
		break;
	    case gwtraf:
		gwtraf_stop(i);
		break;
            case mqtt:
                break;
	    }
	}
    }
}

void transport_status_up(uint32_t rx_rcv, uint32_t rx_ok, uint32_t tx_tot, uint32_t tx_ok) {
    int i;

    for (i = 0; i < MAX_SERVERS; i++) {
	if (servers[i].enabled == true && servers[i].type == ttn_gw_bridge
	    && servers[i].statusstream == true) {
	    ttn_status_up(i, rx_rcv, rx_ok, tx_tot, tx_ok);
	}
    }
}

void transport_status() {
    int i;

    for (i = 0; i < MAX_SERVERS; i++) {
	if (servers[i].enabled == true && servers[i].type == ttn_gw_bridge) {
	    ttn_status(i);
	}
    }
}

void transport_send_downtraf(char *json, int len) {
    int i;

    for (i = 0; i < MAX_SERVERS; i++) {
	if (servers[i].enabled == true && servers[i].type == gwtraf) {
	    gwtraf_downtraf(i, json, len);
	}
    }
}

int init_socket(const char *servaddr, const char *servport, const char *rectimeout, int len) {
    int i, sockfd;
    /* network socket creation */
    struct addrinfo hints;
    struct addrinfo *result; /* store result of getaddrinfo */
    struct addrinfo *q; /* pointer to move into *result data */

    char host_name[64];
    char port_name[64];

    /* prepare hints to open network sockets */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; /* WA: Forcing IPv4 as AF_UNSPEC makes connection on localhost to fail */
    hints.ai_socktype = SOCK_DGRAM;

    /* look for server address w/ upstream port */
    i = getaddrinfo(servaddr, servport, &hints, &result);
    if (i != 0) {
        MSG("ERROR~ [up] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr, servport, gai_strerror(i));
        return -1;
    }

    /* try to open socket for upstream traffic */
    for (q = result; q != NULL; q = q->ai_next) {
        sockfd = socket(q->ai_family, q->ai_socktype, q->ai_protocol);
        if (sockfd == -1) continue; /* try next field */
        else break; /* success, get out of loop */
    }

    if (q == NULL) {
        MSG("ERROR~ [up] failed to open socket to any of server %s addresses (port %s)\n", servaddr, servport);
        i = 1;
        for (q=result; q!=NULL; q=q->ai_next) {
            getnameinfo(q->ai_addr, q->ai_addrlen, host_name, sizeof host_name, port_name, sizeof port_name, NI_NUMERICHOST);
            MSG("INFO~ [up] result %i host:%s service:%s\n", i, host_name, port_name);
            ++i;
        }

        return -1;
    }

    /* connect so we can send/receive packet with the server only */
    i = connect(sockfd, q->ai_addr, q->ai_addrlen);
    if (i != 0) {
        MSG("ERROR~ [up] connect returned %s\n", strerror(errno));
        return -1;
    }

    freeaddrinfo(result);

    if ((setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, rectimeout, len)) != 0) {
        MSG("ERROR~ [up] setsockopt returned %s\n", strerror(errno));
        return -1;
    }

    MSG("INFO~ init_sock get sockfd=%d\n", sockfd);

    return sockfd;
}

// vi: ts=4 sw=4
