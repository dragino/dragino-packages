/*
 * transport.c
 *
 *  Created on: Feb 10, 2017
 *      Author: Jac Kersing
 */

#include <stdint.h>				/* C99 types */
#include <stdbool.h>			/* bool type */
#include <stdio.h>				/* printf, fprintf, snprintf, fopen, fputs */

#include <string.h>				/* memset */
#include <signal.h>				/* sigaction */
#include <time.h>				/* time, clock_gettime, strftime, gmtime */
#include <sys/time.h>			/* timeval */
#include <unistd.h>				/* getopt, access */
#include <stdlib.h>				/* atoi, exit */
#include <errno.h>				/* error messages */
#include <math.h>				/* modf */
#include <assert.h>

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

#include "transport.h"

// Initialize all data structures
void transport_init(Server *server) {
	memset(server, 0, sizeof(Server));
	server->type = semtech;
	server->enabled = false;
	server->upstream = true;
	server->downstream = true;
	server->statusstream = true;
	server->live = false;
	server->connecting = false;
	server->critical = true;
    server->stall_time = 0;
	server->sock_up = -1;
	server->sock_down = -1;
	server->queue = NULL;
	server->ttn = NULL;
	sem_init(servers->send_sem, 0, 0);
}

void transport_start(Server *server) {
	if (server->enabled == true) {
		switch (server->type) {
		case semtech:
			semtech_init(server);
			break;
		case ttn_gw_bridge:
			ttn_init(server);
			break;
		case gwtraf:
			gwtraf_init(server);
			break;
		}
	}
}

void transport_stop(Server *server) {
	if (server->enabled == true) {
		switch (server->type) {
		case semtech:
			semtech_stop(server);
			break;
		case ttn_gw_bridge:
			ttn_stop(server);
			break;
		case gwtraf:
			gwtraf_stop(server);
			break;
		}
	}
}

void transport_status_up(Server *server, uint32_t rx_rcv, uint32_t rx_ok, uint32_t tx_tot, uint32_t tx_ok) {

	if (server->enabled == true && 
           server->type == ttn_gw_bridge && 
           server->statusstream == true) {
		ttn_status_up(server, i, rx_rcv, rx_ok, tx_tot, tx_ok);
	}
}

void transport_status(Server *server) {
	if (server->enabled == true && server->type == ttn_gw_bridge) {
		ttn_status(server);
	}
}

void transport_send_downtraf(Server *server, char *json, int len) {
	if (server->enabled == true && server->type == gwtraf) {
		gwtraf_downtraf(server, i, json, len);
	}
}

int init_sock(const char *addr, const char *port, const char *timeout, int size) {
	int i;
    int sockfd;
	/* network socket creation */
	struct addrinfo hints;
	struct addrinfo *result;	/* store result of getaddrinfo */
	struct addrinfo *q;			/* pointer to move into *result data */

	char host_name[64];
	char port_name[64];

	/* prepare hints to open network sockets */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;	/* WA: Forcing IPv4 as AF_UNSPEC makes connection on localhost to fail */
	hints.ai_socktype = SOCK_DGRAM;

	/* look for server address w/ upstream port */
	i = getaddrinfo(addr, port, &hints, &result);
	if (i != 0) {
		MSG_DEBUG(DEBUG_ERROR,
				  "ERROR~ [init_sock] getaddrinfo on address %s (PORT %s) returned %s\n",
				  addr, port, gai_strerror(i));
		return -1;
	}

	/* try to open socket for upstream traffic */
	for (q = result; q != NULL; q = q->ai_next) {
		sockfd = socket(q->ai_family, q->ai_socktype, q->ai_protocol);
		if (sockfd == -1)
			continue;			/* try next field */
		else
			break;				/* success, get out of loop */
	}

	if (q == NULL) {
		MSG("ERROR~ [init_sock] failed to open socket to any of server %s addresses (port %s)\n", addr, port);
		i = 1;
		for (q = result; q != NULL; q = q->ai_next) {
			getnameinfo(q->ai_addr, q->ai_addrlen, host_name, sizeof host_name,
						port_name, sizeof port_name, NI_NUMERICHOST);
			MSG("INFO~ [init_sock] result %i host:%s service:%s\n", i, host_name, port_name);
			++i;
		}

		return -1;
	}

	/* connect so we can send/receive packet with the server only */
	i = connect(sockfd, q->ai_addr, q->ai_addrlen);
	if (i != 0) {
		MSG("ERROR~ [init_socke] connect returned %s\n", strerror(errno));
		return -1;
	}

	freeaddrinfo(result);

	if ((setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, timeout, size)) != 0) {
		MSG("ERROR~ [init_sock] setsockopt returned %s\n", strerror(errno));
		return -1;
	}

	MSG("INFO~ [init_sock] sockfd=%d\n", sockfd);

	return sockfd;
}

// vi: ts=4 sw=4
