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
 * \brief SX127x of LG02 radio interface
 *
 */

#ifndef _LGW_HAL_SX127X_
#define _LGW_HAL_SX127X_

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "loragw_hal.h"

#define ASSERT(cond) if(!(cond)) {printf("%s:%d\n", __FILE__, __LINE__); exit(1);}

// LOW NOISE AMPLIFIER
#define REG_LNA                     0x0C
#define LNA_MAX_GAIN                0x23
#define LNA_OFF_GAIN                0x00
#define LNA_LOW_GAIN                0x20

// ----------------------------------------
// Constants for radio registers
#define OPMODE_LORA                 0x80
#define OPMODE_MASK                 0x07
#define OPMODE_SLEEP                0x00
#define OPMODE_STANDBY              0x01
#define OPMODE_FSTX                 0x02
#define OPMODE_TX                   0x03
#define OPMODE_FSRX                 0x04
#define OPMODE_RX                   0x05
#define OPMODE_RX_SINGLE            0x06
#define OPMODE_CAD                  0x07

// ----------------------------------------
// Bits masking the corresponding IRQs from the radio
#define IRQ_LORA_RXTOUT_MASK        0x80
#define IRQ_LORA_RXDONE_MASK        0x40
#define IRQ_LORA_CRCERR_MASK        0x20
#define IRQ_LORA_HEADER_MASK        0x10
#define IRQ_LORA_TXDONE_MASK        0x08
#define IRQ_LORA_CDDONE_MASK        0x04
#define IRQ_LORA_FHSSCH_MASK        0x02
#define IRQ_LORA_CDDETD_MASK        0x01

// ----------------------------------------
// DIO function mappings                D0D1D2D3
#define MAP_DIO0_LORA_RXDONE        0x00  // 00------
#define MAP_DIO0_LORA_TXDONE        0x40  // 01------
#define MAP_DIO1_LORA_RXTOUT        0x00  // --00----
#define MAP_DIO1_LORA_NOP           0x30  // --11----
#define MAP_DIO2_LORA_NOP           0xC0  // ----11--


// ----------------------------------------
// RegInvertIQ                                                                                
#define INVERTIQ_RX_MASK            0xBF                                                
#define INVERTIQ_RX_OFF             0x00                                                
#define INVERTIQ_RX_ON              0x40                                                
#define INVERTIQ_TX_MASK            0xFE                                                
#define INVERTIQ_TX_OFF             0x01                                                
#define INVERTIQ_TX_ON              0x00                                                
                                                                                                        
// ----------------------------------------
// RegInvertIQ2
#define INVERTIQ2_ON                0x19
#define INVERTIQ2_OFF               0x1D

enum { RXMODE_SINGLE, RXMODE_SCAN, RXMODE_RSSI };

enum sf_t { SF7=7, SF8, SF9, SF10, SF11, SF12 };

typedef struct {
    char spipath[32];
    char* desc;
    void* spiport;
    bool isftdi;
    uint8_t nss;
    uint8_t rst;
    uint8_t dio[3];
    uint32_t freq;
    uint32_t bw;
    uint8_t sf;
    uint8_t cr;
    uint8_t nocrc;
    uint8_t prlen;
    uint8_t syncword;
    uint8_t invertio;
    uint8_t power;
} sx127x_dev; 

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
// Lora configure : Freq, SF, BW
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_setpower(void*, bool, uint8_t);

int lg02_setfreq(void*, bool, long);

int lg02_setsf(void*, bool, int);

int lg02_setsbw(void*, bool, long);

int lg02_setcr(void*, bool, int);

int lg02_setprlen(void*, bool, long);

int lg02_setsyncword(void*, bool, int);

int lg02_crccheck(void*, bool, uint8_t);

bool lg02_detect(sx127x_dev*);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_setup(sx127x_dev*, uint8_t);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_start_rx(sx127x_dev*, uint8_t, uint8_t);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_receive(void*, bool, struct lgw_pkt_rx_s*); 

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_send(sx127x_dev*, struct lgw_pkt_tx_s*); 

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_single_tx(sx127x_dev*, uint8_t*, int); 

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#endif   
