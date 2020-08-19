/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino Forward -- An opensource lora gateway forward 
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
 * \brief FWD main include file . File version handling , generic functions.
 */

#ifndef _DR_PKT_FWD_H_
#define _DR_PKT_FWD_H_


#include "compiler.h"
#include "logger.h"
#include "linkedlists.h"
#include "endianext.h"
#include "utilities.h"
#include "lgwmm.h"
#include "gwcfg.h"

#define PROTOCOL_VERSION                    2	        /* v1.3 */
#define PROTOCOL_JSON_RXPK_FRAME_FORMAT     1

#define MIN_LORA_PREAMB                     6		    /* minimum Lora preamble length for this application */
#define STD_LORA_PREAMB                     8
#define MIN_FSK_PREAMB                      3		    /* minimum FSK preamble length for this application */
#define STD_FSK_PREAMB                      4

#define DEFAULT_TRY_TIMES                   5           /* 某尝试失败充许继续尝试运行的次数 */

#define PATH_LEN                            64          /* no use PATH_MAX */

#define DEFAULT_GS_SERVER                   127.0.0.1	/* hostname also supported */

#define DEFAULT_GS_PORT_UP                  1780

#define DEFAULT_GS_PORT_DW                  1782

#define DEFAULT_KEEPALIVE                   5	        /* default time interval for downstream keep-alive packet */

#define DEFAULT_PULL_INTERVAL               5	        /* default time interval for send pull request */

#define DEFAULT_STAT_INTERVAL               30	        /* default time interval for statistics */

#define DEFAULT_PUSH_TIMEOUT_MS             100

#define DEFAULT_PULL_TIMEOUT_MS             200

#define DEFAULT_FETCH_SLEEP_MS              10	        /* number of ms waited when a fetch return no packets */

#define DEFAULT_BEACON_POLL_MS              50	        /* time in ms between polling of beacon TX status */

#define DEFAULT_RXPKTS_LIST_SIZE            8           

#define PKT_PUSH_DATA                       0
#define PKT_PUSH_ACK                        1
#define PKT_PULL_DATA                       2
#define PKT_PULL_RESP                       3
#define PKT_PULL_ACK                        4
#define PKT_TX_ACK                          5

#define TX_BUFF_SIZE                        ((540 * NB_PKT_MAX) + 30 + STATUS_SIZE)
#define ACK_BUFF_SIZE                       64

#define LGW_DB_PATH                         "/etc/config/lgwdb.sqlite"

#define UNIX_GPS_EPOCH_OFFSET               315964800 

#define XERR_INIT_AVG       128	/* number of measurements the XTAL correction is averaged on as initial value */
#define XERR_FILT_COEF      256	/* coefficient for low-pass XTAL error tracking */

#define GPS_REF_MAX_AGE     30	/* maximum admitted delay in seconds of GPS loss before considering latest GPS sync unusable */
/* Number of seconds ellapsed between 01.Jan.1970 00:00:00 and 06.Jan.1980 00:00:00 */
                                                                          

/*!
 * \brief Register a function to be executed before Asterisk exits.
 * \param func The callback function to use.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 *
 * \note This function should be rarely used in situations where
 * something must be shutdown to avoid corruption, excessive data
 * loss, or when external programs must be stopped.  All other
 * cleanup in the core should use lgw_register_cleanup.
 */
int lgw_register_atexit(void (*func)(void));

/*!
 * \brief Register a function to be executed before FWD gracefully exits.
 *
 * If FWD is immediately shutdown (core stop now, or sending the TERM
 * signal), the callback is not run. When the callbacks are run, they are run in
 * sequence with lgw_register_atexit() callbacks, in the reverse order of
 * registration.
 *
 * \param func The callback function to use.
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
int lgw_register_cleanup(void (*func)(void));

/*!
 * \brief Unregister a function registered with ast_register_atexit().
 * \param func The callback function to unregister.
 */
void lgw_unregister_atexit(void (*func)(void));

/*!
 * \brief 
 */
int send_tx_ack(serv_s* serv, uint8_t token_h, uint8_t token_l, enum jit_error_e error, int32_t error_value);

/*!
 * \brief 
 */
void print_tx_status(uint8_t tx_status);

/*!
 * \brief 
 */
double difftimespec(struct timespec end, struct timespec beginning); 

/*!
 * \brief 
 */
int get_tx_gain_lut_index(uint8_t rf_chain, int8_t rf_power, uint8_t * lut_index);

#endif							/* _DR_PKT_FWD_H_ */
