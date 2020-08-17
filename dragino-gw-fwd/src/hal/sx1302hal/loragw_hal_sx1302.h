/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    LoRa concentrator Hardware Abstraction Layer

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


#ifndef _LORAGW_SX1302_HAL_H
#define _LORAGW_SX1302_HAL_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */


#include "loragw_hal.h"    

/* -------------------------------------------------------------------------- */
/* --- PUBLIC TYPES --------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */
int lgw_board_sx1302_setconf(struct lgw_conf_board_s *conf);
int lgw_rxrf_sx1302_setconf(uint8_t rf_chain, struct lgw_conf_rxrf_s *conf);
int lgw_rxif_sx1302_setconf(uint8_t if_chain, struct lgw_conf_rxif_s *conf);
int lgw_txgain_sx1302_setconf(uint8_t rf_chain, struct lgw_tx_gain_lut_s *conf);
int lgw_debug_sx1302_setconf(struct lgw_conf_debug_s *conf);

/**
@brief Configure the precision timestamp
@param pointer to structure defining the config to be applied
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_timestamp_sx1302_setconf(struct lgw_conf_timestamp_s *conf);

/**
@brief Connect to the LoRa concentrator, reset it and configure it according to previously set parameters
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_sx1302_start(void);

/**
@brief Stop the LoRa concentrator and disconnect it
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_sx1302_stop(void);

/**
@brief A non-blocking function that will fetch up to 'max_pkt' packets from the LoRa concentrator FIFO and data buffer
@param max_pkt maximum number of packet that must be retrieved (equal to the size of the array of struct)
@param pkt_data pointer to an array of struct that will receive the packet metadata and payload pointers
@return LGW_HAL_ERROR id the operation failed, else the number of packets retrieved
*/
int lgw_sx1302_receive(uint8_t max_pkt, struct lgw_pkt_rx_s * pkt_data);

/**
@brief Schedule a packet to be send immediately or after a delay depending on tx_mode
@param pkt_data structure containing the data and metadata for the packet to send
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else

/!\ When sending a packet, there is a delay (approx 1.5ms) for the analog
circuitry to start and be stable. This delay is adjusted by the HAL depending
on the board version (lgw_i_tx_start_delay_us).

In 'timestamp' mode, this is transparent: the modem is started
lgw_i_tx_start_delay_us microseconds before the user-set timestamp value is
reached, the preamble of the packet start right when the internal timestamp
counter reach target value.

In 'immediate' mode, the packet is emitted as soon as possible: transferring the
packet (and its parameters) from the host to the concentrator takes some time,
then there is the lgw_i_tx_start_delay_us, then the packet is emitted.

In 'triggered' mode (aka PPS/GPS mode), the packet, typically a beacon, is
emitted lgw_i_tx_start_delay_us microsenconds after a rising edge of the
trigger signal. Because there is no way to anticipate the triggering event and
start the analog circuitry beforehand, that delay must be taken into account in
the protocol.
*/
int lgw_sx1302_send(struct lgw_pkt_tx_s * pkt_data);

/**
@brief Give the the status of different part of the LoRa concentrator
@param select is used to select what status we want to know
@param code is used to return the status code
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_sx1302_status(uint8_t rf_chain, uint8_t select, uint8_t * code);

/**
@brief Abort a currently scheduled or ongoing TX
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_abort_sx1302_tx(uint8_t rf_chain);

/**
@brief Return value of internal counter when latest event (eg GPS pulse) was captured
@param trig_cnt_us pointer to receive timestamp value
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_get_sx1302_trigcnt(uint32_t * trig_cnt_us);

/**
@brief Return instateneous value of internal counter
@param inst_cnt_us pointer to receive timestamp value
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_get_sx1302_instcnt(uint32_t * inst_cnt_us);

/**
@brief Return the LoRa concentrator EUI
@param eui pointer to receive eui
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_get_sx1302_eui(uint64_t * eui);

/**
@brief Return the temperature measured by the LoRa concentrator sensor
@param temperature The temperature measured, in degree celcius
@return LGW_HAL_ERROR id the operation failed, LGW_HAL_SUCCESS else
*/
int lgw_get_sx1302_temperature(float * temperature);

#endif

/* --- EOF ------------------------------------------------------------------ */
