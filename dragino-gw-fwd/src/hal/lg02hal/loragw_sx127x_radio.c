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

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */
#include <stdio.h>      /* printf fprintf */

#include "gpio.h"
#include "loragw_hal.h"
#include "loragw_spi.h"
#include "loragw_aux.h"

#include "loragw_sx1272_regs.h"
#include "loragw_sx1276_regs.h"
#include "loragw_sx127x_radio.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_REG == 1
    #define DEBUG_MSG(str)              fprintf(stderr, str)
    #define DEBUG_PRINTF(fmt, args...)  fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
    #define CHECK_NULL(a)               if(a==NULL){fprintf(stderr,"%s:%d: ERROR~ NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return SX127X_REG_ERROR;}
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
    #define CHECK_NULL(a)               if(a==NULL){return SX127X_REG_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE TYPES -------------------------------------------------------- */

/**
@struct lgw_radio_FSK_bandwidth_s
@brief Associate a bandwidth in kHz with its corresponding register values
*/
struct lgw_sx127x_FSK_bandwidth_s {
    uint32_t    RxBwKHz;
    uint8_t     RxBwMant;
    uint8_t     RxBwExp;
};

/**
@struct lgw_radio_type_version_s
@brief Associate a radio type with its corresponding expected version value
        read in the radio version register.
*/
struct lgw_radio_type_version_s {
    enum lgw_radio_type_e type;
    uint8_t reg_version;
};

/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

const struct lgw_sx127x_FSK_bandwidth_s sx127x_FskBandwidths[] =
{
    { 2600  , 2, 7 },   /* LGW_SX127X_RXBW_2K6_HZ */
    { 3100  , 1, 7 },   /* LGW_SX127X_RXBW_3K1_HZ */
    { 3900  , 0, 7 },   /* ... */
    { 5200  , 2, 6 },
    { 6300  , 1, 6 },
    { 7800  , 0, 6 },
    { 10400 , 2, 5 },
    { 12500 , 1, 5 },
    { 15600 , 0, 5 },
    { 20800 , 2, 4 },
    { 25000 , 1, 4 },   /* ... */
    { 31300 , 0, 4 },
    { 41700 , 2, 3 },
    { 50000 , 1, 3 },
    { 62500 , 0, 3 },
    { 83333 , 2, 2 },
    { 100000, 1, 2 },
    { 125000, 0, 2 },
    { 166700, 2, 1 },
    { 200000, 1, 1 },   /* ... */
    { 250000, 0, 1 }    /* LGW_SX127X_RXBW_250K_HZ */
};

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */
extern void *lgw_spi_target; /*! generic pointer to the SPI device */
extern void *lgw_lbt_target; /*! generic pointer to the SPI device */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */
int setup_sx1272_FSK(bool isftdi, uint32_t frequency, enum lgw_sx127x_rxbw_e rxbw_khz, int8_t rssi_offset);
int setup_sx1276_FSK(bool isftdi, uint32_t frequency, enum lgw_sx127x_rxbw_e rxbw_khz, int8_t rssi_offset);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int setup_sx1272_FSK(bool isftdi, uint32_t frequency, enum lgw_sx127x_rxbw_e rxbw_khz, int8_t rssi_offset) {
    uint64_t freq_reg;
    uint8_t ModulationShaping = 0;
    uint8_t PllHop = 1;
    uint8_t LnaGain = 1;
    uint8_t LnaBoost = 3;
    uint8_t AdcBwAuto = 0;
    uint8_t AdcBw = 7;
    uint8_t AdcLowPwr = 0;
    uint8_t AdcTrim = 6;
    uint8_t AdcTest = 0;
    uint8_t RxBwExp = sx127x_FskBandwidths[rxbw_khz].RxBwExp;
    uint8_t RxBwMant = sx127x_FskBandwidths[rxbw_khz].RxBwMant;
    uint8_t RssiSmoothing = 5;
    uint8_t RssiOffsetReg;
    uint8_t reg_val;
    int x;

    /* Set in FSK mode */
    x = lgw_sx127x_reg_w(isftdi, SX1272_REG_OPMODE, 0);
    wait_ms(100);
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_OPMODE, 0 | (ModulationShaping << 3)); /* Sleep mode, no FSK shaping */
    wait_ms(100);
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_OPMODE, 1 | (ModulationShaping << 3)); /* Standby mode, no FSK shaping */
    wait_ms(100);

    /* Set RF carrier frequency */
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_PLLHOP, PllHop << 7);
    freq_reg = ((uint64_t)frequency << 19) / (uint64_t)32000000;
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_FRFMSB, (freq_reg >> 16) & 0xFF);
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_FRFMID, (freq_reg >> 8) & 0xFF);
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_FRFLSB, (freq_reg >> 0) & 0xFF);

    /* Config */
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_LNA, LnaBoost | (LnaGain << 5)); /* Improved sensitivity, highest gain */
    x |= lgw_sx127x_reg_w(isftdi, 0x68, AdcBw | (AdcBwAuto << 3));
    x |= lgw_sx127x_reg_w(isftdi, 0x69, AdcTest | (AdcTrim << 4) | (AdcLowPwr << 7));

    /* set BR and FDEV for 200 kHz bandwidth*/
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_BITRATEMSB, 125);
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_BITRATELSB, 0);
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_FDEVMSB, 2);
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_FDEVLSB, 225);

    /* Config continues... */
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_RXCONFIG, 0); /* Disable AGC */
    RssiOffsetReg = (rssi_offset >= 0) ? (uint8_t)rssi_offset : (uint8_t)(~(-rssi_offset)+1); /* 2's complement */
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_RSSICONFIG, RssiSmoothing | (RssiOffsetReg << 3)); /* Set RSSI smoothing to 64 samples, RSSI offset to given value */
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_RXBW, RxBwExp | (RxBwMant << 3));
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_RXDELAY, 2);
    x |= lgw_sx127x_reg_w(isftdi, SX1272_REG_PLL, 0x10); /* PLL BW set to 75 KHz */
    x |= lgw_sx127x_reg_w(isftdi, 0x47, 1); /* optimize PLL start-up time */

    if (x != SX127X_REG_SUCCESS) {
        DEBUG_MSG("ERROR~ Failed to configure SX1272\n");
        return x;
    }

    /* set Rx continuous mode */
    x = lgw_sx127x_reg_w(isftdi, SX1272_REG_OPMODE, 5 | (ModulationShaping << 3)); /* Receiver Mode, no FSK shaping */
    wait_ms(500);
    x |= lgw_sx127x_reg_r(isftdi, SX1272_REG_IRQFLAGS1, &reg_val);
    /* Check if RxReady and ModeReady */
    if ((TAKE_N_BITS_FROM(reg_val, 6, 1) == 0) || (TAKE_N_BITS_FROM(reg_val, 7, 1) == 0) || (x != SX127X_REG_SUCCESS)) {
        DEBUG_MSG("ERROR~ SX1272 failed to enter RX continuous mode\n");
        return SX127X_REG_ERROR;
    }
    wait_ms(500);

    DEBUG_PRINTF("INFO: Successfully configured SX1272 for FSK modulation (rxbw=%d)\n", rxbw_khz);

    return SX127X_REG_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int setup_sx1276_FSK(bool isftdi, uint32_t frequency, enum lgw_sx127x_rxbw_e rxbw_khz, int8_t rssi_offset) {
    uint32_t freq_reg;
    uint8_t ModulationShaping = 0;
    uint8_t PllHop = 1;
    uint8_t LnaGain = 1;
    uint8_t LnaBoost = 3;
    uint8_t AdcBwAuto = 0;
    uint8_t AdcBw = 7;
    uint8_t AdcLowPwr = 0;
    uint8_t AdcTrim = 6;
    uint8_t AdcTest = 0;
    uint8_t RxBwExp = sx127x_FskBandwidths[rxbw_khz].RxBwExp;
    uint8_t RxBwMant = sx127x_FskBandwidths[rxbw_khz].RxBwMant;
    uint8_t RssiSmoothing = 5;
    uint8_t RssiOffsetReg;
    uint8_t reg_val;
    int x;

    /* Set in FSK mode */
    x = lgw_sx127x_reg_w(isftdi, SX1276_REG_OPMODE, 0);
    wait_ms(100);
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_OPMODE, 0 | (ModulationShaping << 3)); /* Sleep mode, no FSK shaping */
    wait_ms(100);
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_OPMODE, 1 | (ModulationShaping << 3)); /* Standby mode, no FSK shaping */
    wait_ms(100);

    /* Set RF carrier frequency */
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_PLLHOP, PllHop << 7);
    freq_reg = ((uint64_t)frequency << 19) / (uint64_t)32000000;
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_FRFMSB, (freq_reg >> 16) & 0xFF);
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_FRFMID, (freq_reg >> 8) & 0xFF);
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_FRFLSB, (freq_reg >> 0) & 0xFF);

    /* Config */
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_LNA, LnaBoost | (LnaGain << 5)); /* Improved sensitivity, highest gain */
    x |= lgw_sx127x_reg_w(isftdi, 0x57, AdcBw | (AdcBwAuto << 3));
    x |= lgw_sx127x_reg_w(isftdi, 0x58, AdcTest | (AdcTrim << 4) | (AdcLowPwr << 7));

    /* set BR and FDEV for 200 kHz bandwidth*/
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_BITRATEMSB, 125);
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_BITRATELSB, 0);
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_FDEVMSB, 2);
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_FDEVLSB, 225);

    /* Config continues... */
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_RXCONFIG, 0); /* Disable AGC */
    RssiOffsetReg = (rssi_offset >= 0) ? (uint8_t)rssi_offset : (uint8_t)(~(-rssi_offset)+1); /* 2's complement */
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_RSSICONFIG, RssiSmoothing | (RssiOffsetReg << 3)); /* Set RSSI smoothing to 64 samples, RSSI offset 3dB */
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_RXBW, RxBwExp | (RxBwMant << 3));
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_RXDELAY, 2);
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_PLL, 0x10); /* PLL BW set to 75 KHz */
    x |= lgw_sx127x_reg_w(isftdi, 0x43, 1); /* optimize PLL start-up time */

    /* set DIOMAPING DIO for rssi interrupt */
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_DIOMAPPING1, (1 << 6) & 0xFF);
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_DIOMAPPING2, 0);

    /*set rssithresh RSSI = -RssiThreshold/2[dBm] */
    x |= lgw_sx127x_reg_w(isftdi, SX1276_REG_RSSITHRESH, 160);

    if (x != SX127X_REG_SUCCESS) {
        DEBUG_MSG("ERROR~ Failed to configure SX1276\n");
        return x;
    }

    /* set Rx continuous mode */
    x = lgw_sx127x_reg_w(isftdi, SX1276_REG_OPMODE, 5 | (ModulationShaping << 3)); /* Receiver Mode, no FSK shaping */
    wait_ms(500);
    x |= lgw_sx127x_reg_r(isftdi, SX1276_REG_IRQFLAGS1, &reg_val);
    /* Check if RxReady and ModeReady */
    if ((TAKE_N_BITS_FROM(reg_val, 6, 1) == 0) || (TAKE_N_BITS_FROM(reg_val, 7, 1) == 0) || (x != SX127X_REG_SUCCESS)) {
        DEBUG_MSG("ERROR~ SX1276 failed to enter RX continuous mode\n");
        return SX127X_REG_ERROR;
    }
    wait_ms(500);

    DEBUG_PRINTF("INFO: Successfully configured SX1276 for FSK modulation (rxbw=%d)\n", rxbw_khz);

    return SX127X_REG_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int reset_sx127x(bool isftdi, int invert) {
    if (isftdi) {
        return ftdi_sx127x_reset(lgw_lbt_target, invert);
    } else {
        if (invert == 0) {
            digital_write(SX127X_RESET_PIN, 0);
            wait_ms(10);
            digital_write(SX127X_RESET_PIN, 1);
            wait_ms(10);
        } else {
            digital_write(SX127X_RESET_PIN, 1);
            wait_ms(10);
            digital_write(SX127X_RESET_PIN, 0);
            wait_ms(10);
        }
    }
    return SX127X_REG_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_sx127x_reg_w(bool isftdi, uint8_t address, uint8_t reg_value) {
    if (isftdi)
        return lgw_ft_spi_w(lgw_lbt_target, LGW_SPI_MUX_TARGET_SX127X, address, reg_value);
    else
        return lgw_spi_w(lgw_lbt_target, LGW_SPI_MUX_TARGET_SX127X, address, reg_value);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_reg_w(void *target, bool isftdi, uint8_t address, uint8_t reg_value) {
    if (isftdi)
        return lgw_ft_spi_w(target, LGW_SPI_MUX_TARGET_SX127X, address, reg_value);
    else
        return lgw_spi_w(target, LGW_SPI_MUX_TARGET_SX127X, address, reg_value);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_sx127x_reg_r(bool isftdi, uint8_t address, uint8_t *reg_value) {
    if (isftdi)
        return lgw_ft_spi_r(lgw_lbt_target, LGW_SPI_MUX_TARGET_SX127X, address, reg_value);
    else
        return lgw_spi_r(lgw_lbt_target, LGW_SPI_MUX_TARGET_SX127X, address, reg_value);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_reg_r(void *target, bool isftdi, uint8_t address, uint8_t *reg_value) {
    if (isftdi)
        return lgw_ft_spi_r(target, LGW_SPI_MUX_TARGET_SX127X, address, reg_value);
    else
        return lgw_spi_r(target, LGW_SPI_MUX_TARGET_SX127X, address, reg_value);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_setup_sx127x(bool isftdi, uint32_t frequency, uint8_t modulation, enum lgw_sx127x_rxbw_e rxbw_khz, int8_t rssi_offset) {
    int x, i;
    uint8_t version;
    enum lgw_radio_type_e radio_type = LGW_RADIO_TYPE_NONE;
    struct lgw_radio_type_version_s supported_radio_type[2] = {
        {LGW_RADIO_TYPE_SX1276, 0x12},
        {LGW_RADIO_TYPE_SX1272, 0x22}
    };

    /* Check parameters */
    if (modulation != MOD_FSK) {
        DEBUG_PRINTF("ERROR~ modulation not supported for SX127x (%u)\n", modulation);
        return SX127X_REG_ERROR;
    }
    if (rxbw_khz > LGW_SX127X_RXBW_250K_HZ) {
        DEBUG_PRINTF("ERROR~ RX bandwidth not supported for SX127x (%u)\n", rxbw_khz);
        return SX127X_REG_ERROR;
    }

    /* Probing radio type */
    for (i = 0; i < (int)(sizeof supported_radio_type); i++) {
        /* Reset the radio */
        x = reset_sx127x(isftdi, i);
        if (x != LGW_SPI_SUCCESS) {
            DEBUG_MSG("ERROR~ Failed to reset sx127x\n");
            return x;
        }
        /* Read version register */
        x = lgw_sx127x_reg_r(isftdi, 0x42, &version);
        if (x != LGW_SPI_SUCCESS) {
            DEBUG_MSG("ERROR~ Failed to read sx127x version register\n");
            return x;
        }
        /* Check if we got the expected version */
        if (version != supported_radio_type[i].reg_version) {
            DEBUG_PRINTF("INFO: sx127x version register - read:0x%02x, expected:0x%02x\n", version, supported_radio_type[i].reg_version);
            continue;
        } else {
            DEBUG_PRINTF("INFO: sx127x radio has been found (type:%d, version:0x%02x)\n", supported_radio_type[i].type, version);
            radio_type = supported_radio_type[i].type;
            break;
        }
    }
    if (radio_type == LGW_RADIO_TYPE_NONE) {
        DEBUG_MSG("ERROR~ sx127x radio has not been found\n");
        return SX127X_REG_ERROR;
    }

    /* Setup the radio */
    switch (modulation) {
        case MOD_FSK:
            if (radio_type == LGW_RADIO_TYPE_SX1272) {
                x = setup_sx1272_FSK(isftdi, frequency, rxbw_khz, rssi_offset);
            } else {
                x = setup_sx1276_FSK(isftdi, frequency, rxbw_khz, rssi_offset);
            }
            break;
        default:
            /* Should not happen */
            break;
    }
    if (x != SX127X_REG_SUCCESS) {
        DEBUG_MSG("ERROR~ failed to setup SX127x\n");
        return x;
    }

    return SX127X_REG_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
