/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Functions used to handle a single LoRa concentrator.
    Registers are addressed by name.
    Multi-bytes registers are handled automatically.
    Read-modify-write is handled automatically.

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


#ifndef _LORAGW_REG_H
#define _LORAGW_REG_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>        /* C99 types */
#include <stdbool.h>    /* bool type */

#include "config.h"    /* library configuration options (dynamically generated) */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define LGW_REG_SUCCESS  0
#define LGW_REG_ERROR    -1

/* -------------------------------------------------------------------------- */
/* --- INTERNAL SHARED TYPES ------------------------------------------------ */

struct lgw_reg_s {
    int8_t  page;        /*!< page containing the register (-1 for all pages) */
    uint16_t addr;        /*!< base address of the register (7 bit) */
    uint8_t offs;        /*!< position of the register LSB (between 0 to 7) */
    bool    sign;        /*!< 1 indicates the register is signed (2 complem.) */
    uint8_t leng;        /*!< number of bits in the register */
    bool    rdon;        /*!< 1 indicates a read-only register */
    bool    chck;        /*!< register can be checked or not: (pulse, w0clr, w1clr) */
    int32_t dflt;        /*!< register default value */
};

/* -------------------------------------------------------------------------- */
/* --- INTERNAL SHARED FUNCTIONS -------------------------------------------- */

int reg_w_align32(void *spi_target, uint8_t spi_mux_target, struct lgw_reg_s r, int32_t reg_value);
int reg_r_align32(void *spi_target, uint8_t spi_mux_target, struct lgw_reg_s r, int32_t *reg_value);

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
 *  TODO
 */
int lgw_mem_wb(uint16_t mem_addr, const uint8_t *data, uint16_t size);
int lgw_mem_rb(uint16_t mem_addr, uint8_t *data, uint16_t size, bool fifo_mode);

#endif

/* --- EOF ------------------------------------------------------------------ */
