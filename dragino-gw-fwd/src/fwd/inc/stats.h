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

// Definitions
//
#define STATUS_SIZE     200

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
} type_dw_e;

typedef enum {
	RX_RCV,
	RX_OK,
	RX_BAD,
	RX_NOCRC,
	PKT_FWD
} type_up_e;

typedef struct {
	uint32_t meas_nb_tx_ok;
	uint32_t meas_nb_tx_fail;
	uint32_t meas_nb_tx_requested;
	uint32_t meas_nb_tx_rejected_collision_packet;
	uint32_t meas_nb_tx_rejected_collision_beacon;
	uint32_t meas_nb_tx_rejected_too_late;
	uint32_t meas_nb_tx_rejected_too_early;
	uint32_t meas_dw_pull_sent;
	uint32_t meas_dw_ack_rcv;
	uint32_t meas_dw_dgram_acp;
	uint32_t meas_dw_dgram_rcv;
	uint32_t meas_dw_network_byte;
	uint32_t meas_dw_payload_byte;
} stat_dw_s;

typedef struct {
	uint32_t meas_nb_rx_rcv;
	uint32_t meas_nb_rx_ok;
	uint32_t meas_nb_rx_bad;
	uint32_t meas_nb_rx_nocrc;
	uint32_t meas_up_pkt_fwd;
    uint32_t meas_up_network_byte;
    uint32_t meas_up_payload_byte;
    uint32_t meas_up_dgram_sent;
    uint32_t meas_up_ack_rcv;
} stat_up_s;

typedef struct {
    bool statusstream;
    bool report_ready;
    char stat_format[16];               // format for json statistics
    char status_report[STATUS_SIZE];
    uint16_t stat_interval; 	     // time interval (in sec) at which statistics are collected and displayed
    uint32_t meas_nb_beacon_queued;
    uint32_t meas_nb_beacon_sent;
    uint32_t meas_nb_beacon_rejected;
    stat_up_s   stat_up;
    stat_dw_s stat_down;
    pthread_mutex_t mx_report;	 // control access to the queue for each server
} report_s;

void report_start();

#endif							// _LORA_PKTFWD_STATS_H
