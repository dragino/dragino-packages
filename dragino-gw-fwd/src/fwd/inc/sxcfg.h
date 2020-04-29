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
 * \brief semtech radio modules parameter
 */

#ifndef _SXCFG_H
#define _SXCFG_H

#include <stdint.h>
#include "loragw_hal.h"

typedef struct {
    struct lgw_conf_board_s  boardconf;
    struct lgw_tx_gain_lut_s txlut;
    struct lgw_conf_rxrf_s   rfconf[LGW_RF_CHAIN_NB];
    struct lgw_conf_rxif_s   ifconf[LGW_IF_CHAIN_NB];
    struct lgw_conf_lbt_s    lbt;
    int16_t  txpowAdjust;   // assuming there is only one TX path / SX1301 (scaled by TXPOW_SCALE)
    uint8_t  pps;           // enable PPS latch of trigger count
    uint8_t  antennaType;   // type of antenna
    char  device[MAX_DEVICE_LEN];   // SPI device, FTDI spec etc.
} sx1301conf;

typedef struct {
    struct lgw_conf_board_s     boardconf;
    struct lgw_tx_gain_lut_s    txlut;
    struct lgw_conf_rxrf_s      rfconf[LGW_RF_CHAIN_NB];
    struct lgw_conf_rxif_s      ifconf[LGW_IF_CHAIN_NB];
    struct lgw_conf_timestamp_s tsconf;
    struct lgw_conf_lbt_s       lbt;
    int16_t  txpowAdjust;   // assuming there is only one TX path / SX1302 (scaled by TXPOW_SCALE)
    uint8_t  pps;           // enable PPS latch of trigger count
    uint8_t  antennaType;   // type of antenna
    char  device[MAX_DEVICE_LEN];   // SPI device, FTDI spec etc.
} sx1302conf;

typedef struct {
    struct lgw_conf_board_s  boardconf;
    struct lgw_tx_gain_lut_s txlut;
    struct lgw_conf_rxrf_s   rfconf[LGW_RF_CHAIN_NB];
    struct lgw_conf_rxif_s   ifconf[LGW_IF_CHAIN_NB];
    struct lgw_conf_lbt_s    lbt;
    int16_t  txpowAdjust;   // assuming there is only one TX path / SX1308 (scaled by TXPOW_SCALE)
    uint8_t  pps;           // enable PPS latch of trigger count
    uint8_t  antennaType;   // type of antenna
    char  device[MAX_DEVICE_LEN];   // SPI device, FTDI spec etc.
} sx1308conf;

int  sx1301conf_start (struct sx1301conf* sx1301conf, uint32_t region);
int  sx1302conf_start (struct sx1301conf* sx1302conf, uint32_t region);

/* \brief reference
 *********************************************************
 *        Basicstation Concentrator Design v2
 *********************************************************
typedef struct {
  "device"        : STRING   // station specific
  "pps"           : BOOL     // station specific
  "loramac_public": BOOL
  "clksrc"        : INT
  "board_rx_freq" : INT
  "board_rx_bw"   : INT
  "full_duplex"   : BOOL
  "board_type"    : "master" | "slave"
  "FSK_sync"      : STRING     // hexdigits encoding 1-8 bytes
  "calibration_temperature_celsius_room": INT(-128..127)
  "calibration_temperature_code_ad9361" : INT(0..255)
  "dsp_stat_interval" : INT(0..255)
  "nb_dsp"        : INT
  "aes_key"       : STRING     // hexdigits encoding 16 bytes
  "rf_chain_conf" : RFCHAINCONF
  "SX1301_conf"   : [ SX1301CONF, .. ]
  "lbt_conf"      : LBTCONF
} radio_conf;

typedef struct {
  "chip_enable"      : BOOL
  "chip_center_freq" : INT
  "chip_rf_chain"    : INT
  "chan_multiSF_X"   : CHANCONF   // where X in {0..7}
  "chan_LoRa_std"    : CHANCONF
  "chan_FSK"         : CHANCONF
} sx1301_conf;

typedef struct {
  "chan_rx_freq"  : INT
  "bandwidth"     : INT(125000,250000,50000)
  // if under fields "chan_multiSF_0".."chan_multiSF_7"
  "spread_factor" : INT(7..12)
  // if under field "chan_LoRa_std"
  "spread_factor" : "N-M"        // N,M in {7..12} and N <= M
  // "spread_factor" must be absent if under field "chan_FSK"
  // valid only if under field "chan_FSK"
  "bit_rate"      : INT
} chan_conf;

typedef struct {
  "enable" : BOOL
  "rssi_target" : INT(-128..127)
  "rssi_shift"  : INT(0..255)
  "chan_cfg"    : [ .. ]
} lbt_conf;

typedef struct {
  "rx_enable"    : BOOL
  "tx_enable"    : BOOL
  "rssi_offset"  : INT
  "rssi_offset_coeff_a"  : INT
  "rssi_offset_coeff_b"  : INT
  "tx_freq_min"  : INT
  "tx_freq_max"  : INT
  "tx_lut"       : [ TXLUT, .. ]
  "txpow_adjust" : INT
  "antenna_type" : "omni" | "sector"
} rfchain_conf;

typedef struct {
  "rf_power"      : INT(-128..127)
  "fpga_dig_gain" : INT(0..255)
  "ad9361_atten"  : INT(0..65535)
  "ad9361_auxdac_vref"   : INT(0..255)
  "ad9361_auxdac_word"   : INT(0..65535)
  "ad9361_tcomp_coeff_a" : INT(-32768..32767)
  "ad9361_tcomp_coeff_b" : INT(-32768..32767)
} txlut_conf;
*************************************************
*************************************************/


#endif //END OF _SXCFG_H
