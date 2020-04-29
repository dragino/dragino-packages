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
 * \brief GW service process 
 *      
*/

#include "fwd.h"
#include "service.h"
#include "semtect_service.h"

int init_sock(const char *addr, const char *port, const void *timeout, int size) {
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

	return sockfd;
}

int service_handle_rxpkt(gw_s* gw, rxpkts_s* rxpkt) {
    serv_s* serv_entry;
    LGW_LIST_TRAVERSE(gw->serv_list, serv_entry, list) { 
        serv_entry->rxpkt_set = rxpkt;
        sem_post(serv_entry->pthread.sema);
    }
}

int service_start(gw_s* gw) {

}

void service_stop(gw_s* gw) {
}


// vi: ts=4 sw=4
