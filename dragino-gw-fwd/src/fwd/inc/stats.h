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
 * \brief gateway forward statics struct define
 */

#ifndef _LORA_PKTFWD_STATS_H
#define _LORA_PKTFWD_STATS_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>				/* C99 types */
#include <stdbool.h>			/* bool type */
#include "loragw_hal.h"
#include "loragw_gps.h"

// Definitions

typedef enum {
	TX_OK,
	TX_FAIL,
	TX_REQUESTED,
	TX_REJ_COLL_PACKET,
	TX_REJ_COLL_BEACON,
	TX_REJ_TOO_LATE,
	TX_REJ_TOO_EARLY,
	BEACON_QUEUED,
	BEACON_SENT,
	BEACON_REJECTED
} typedw;

typedef struct {
    pthread_mutex_t mx_meas_dw;
	uint32_t meas_nb_tx_ok;
	uint32_t meas_nb_tx_fail;
	uint32_t meas_nb_tx_requested;
	uint32_t meas_nb_tx_rejected_collision_packet;
	uint32_t meas_nb_tx_rejected_collision_beacon;
	uint32_t meas_nb_tx_rejected_too_late;
	uint32_t meas_nb_tx_rejected_too_early;
	uint32_t meas_nb_beacon_queued;
	uint32_t meas_nb_beacon_sent;
	uint32_t meas_nb_beacon_rejected;
	uint32_t meas_dw_pull_sent;
	uint32_t meas_dw_ack_rcv;
	uint32_t meas_dw_dgram_acp;
	uint32_t meas_dw_dgram_rcv;
	uint32_t meas_dw_network_byte;
	uint32_t meas_dw_payload_byte;
} statdw;

typedef enum {
	RX_RCV,
	RX_OK,
	RX_BAD,
	RX_NOCRC,
	PKT_FWD
} typeup;

typedef struct {
    pthread_mutex_t mx_meas_up;
	uint32_t meas_nb_rx_rcv;
	uint32_t meas_nb_rx_ok;
	uint32_t meas_nb_rx_bad;
	uint32_t meas_nb_rx_nocrc;
	uint32_t meas_up_pkt_fwd;
    uint32_t meas_up_network_byte;
    uint32_t meas_up_payload_byte;
    uint32_t meas_up_dgram_sent;
    uint32_t meas_up_ack_rcv;
} statup;

// Function prototypes
void stats_init();
void increment_down(enum stats_down type);
void increment_up(enum stats_up type);
void stats_data_up(int nb_pkt, struct lgw_pkt_rx_s *rxpkt);
void stats_report();
#endif							// _LORA_PKTFWD_STATS_H
