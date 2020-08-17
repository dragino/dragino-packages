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

/*! \file
 *
 * \brief radio 
 *
 */

#ifndef _LORAGW_SX127X_RADIO_H
#define _LORAGW_SX127X_RADIO_H

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */

#define SX127X_REG_SUCCESS     0
#define SX127X_REG_ERROR       -1

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */
enum lgw_sx127x_rxbw_e {
    LGW_SX127X_RXBW_2K6_HZ,
    LGW_SX127X_RXBW_3K1_HZ,
    LGW_SX127X_RXBW_3K9_HZ,
    LGW_SX127X_RXBW_5K2_HZ,
    LGW_SX127X_RXBW_6K3_HZ,
    LGW_SX127X_RXBW_7K8_HZ,
    LGW_SX127X_RXBW_10K4_HZ,
    LGW_SX127X_RXBW_12K5_HZ,
    LGW_SX127X_RXBW_15K6_HZ,
    LGW_SX127X_RXBW_20K8_HZ,
    LGW_SX127X_RXBW_25K_HZ,
    LGW_SX127X_RXBW_31K3_HZ,
    LGW_SX127X_RXBW_41K7_HZ,
    LGW_SX127X_RXBW_50K_HZ,
    LGW_SX127X_RXBW_62K5_HZ,
    LGW_SX127X_RXBW_83K3_HZ,
    LGW_SX127X_RXBW_100K_HZ,
    LGW_SX127X_RXBW_125K_HZ,
    LGW_SX127X_RXBW_166K7_HZ,
    LGW_SX127X_RXBW_200K_HZ,
    LGW_SX127X_RXBW_250K_HZ
};

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

int reset_sx127x(bool isftdi, int invert);

int lgw_setup_sx127x(bool isftdi, uint32_t frequency, uint8_t modulation, enum lgw_sx127x_rxbw_e rxbw_khz, int8_t rssi_offset);

int lgw_sx127x_reg_w(bool isftdi, uint8_t address, uint8_t reg_value);

int lgw_sx127x_reg_r(bool isftdi, uint8_t address, uint8_t *reg_value);

int lg02_reg_r(void *target, bool isftdi, uint8_t address, uint8_t *reg_value);

int lg02_reg_w(void *target, bool isftdi, uint8_t address, uint8_t reg_value);

#endif
