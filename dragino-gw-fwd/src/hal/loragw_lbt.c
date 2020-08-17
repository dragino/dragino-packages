/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Functions used to handle the Listen Before Talk feature

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Michael Coracin
*/

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */
#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif 

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf */
#include <stdlib.h>     /* abs, labs, llabs */
#include <string.h>     /* memset */
#include <time.h>      
#include <sys/time.h>

#include "gpio.h"
#include "loragw_aux.h"
#include "loragw_lbt.h"
#include "loragw_spi.h"

#include "loragw_sx127x_radio.h"
#include "loragw_sx1272_regs.h"
#include "loragw_sx1276_regs.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_LBT == 1
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

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- INTERNAL SHARED VARIABLES -------------------------------------------- */

extern void *lgw_spi_target; /*! generic pointer to the SPI device */

void *lgw_lbt_target = NULL; /*! generic pointer to the LBT device */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

static bool lbt_enable;
static bool lbt_isftdi;
static char lbt_spi_path[32];
static uint8_t lbt_nb_active_channel;
static int8_t lbt_rssi_target_dBm;
static int8_t lbt_rssi_offset_dB;
static uint32_t lbt_start_freq;
static struct lgw_conf_lbt_chan_s lbt_channel_cfg[LBT_CHANNEL_FREQ_NB];

//static uint32_t lbt_timestamp[LBT_CHANNEL_FREQ_NB];
static bool lbt_chan_free[LBT_CHANNEL_FREQ_NB];

/* -------------------------------------------------------------------------- */
/* --- PUBLIC VARIABLES ------------------------------------------ */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS ---------------------------------------------------- */

bool is_equal_freq(uint32_t a, uint32_t b);

enum lgw_sx127x_rxbw_e get_rxbw_index(uint32_t hz);

/* return us */
static uint64_t difftimespec(struct timespec* end, struct timespec* begin);

//static int is_lbt_chan(int step);

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lbt_setconf(struct lgw_conf_lbt_s * conf) {
    int i;

    /* Check input parameters */
    if (conf == NULL) {
        return LGW_LBT_ERROR;
    }
    if ((conf->nb_channel < 1) || (conf->nb_channel > LBT_CHANNEL_FREQ_NB)) {
        DEBUG_PRINTF("ERROR~ Number of defined LBT channels is out of range (%u)\n", conf->nb_channel);
        return LGW_LBT_ERROR;
    }

    /* Initialize LBT channels configuration */
    memset(lbt_channel_cfg, 0, sizeof lbt_channel_cfg);

    /* Set internal LBT config according to parameters */
    lbt_enable = conf->enable;
    lbt_isftdi = conf->isftdi;
    strncpy(lbt_spi_path, conf->spi_path, sizeof(lbt_spi_path));
    lbt_nb_active_channel = conf->nb_channel;
    lbt_rssi_target_dBm = conf->rssi_target;
    lbt_rssi_offset_dB = conf->rssi_offset;

    for (i=0; i<lbt_nb_active_channel; i++) {
        lbt_channel_cfg[i].freq_hz = conf->channels[i].freq_hz;
        lbt_channel_cfg[i].scan_time_us = conf->channels[i].scan_time_us;
    }

    return LGW_LBT_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lbt_setup(void) {
    int x, i;
    /*
    if (lbt_channel_cfg[0].freq_hz > 915000000UL)
        lbt_start_freq = 915000000;
    else
        lbt_start_freq = 863000000;
    */
    lbt_start_freq = lbt_channel_cfg[0].freq_hz;

    /* Configure FPGA for LBT */
    //val = -2*lbt_rssi_target_dBm; /* Convert RSSI target in dBm to FPGA register format */
    
    /* Set default values for non-active LBT channels */
    for (i=lbt_nb_active_channel; i<LBT_CHANNEL_FREQ_NB; i++) {
        lbt_channel_cfg[i].freq_hz = lbt_start_freq;
        lbt_channel_cfg[i].scan_time_us = 128; /* fastest scan for non-active channels */
    }

    /* Configure FPGA for both active and non-active LBT channels */
    for (i=0; i<LBT_CHANNEL_FREQ_NB; i++) {
        /* Check input parameters */
        /*
        if (lbt_channel_cfg[i].freq_hz < lbt_start_freq) {
            DEBUG_PRINTF("ERROR~ LBT channel frequency is out of range (%u)\n", lbt_channel_cfg[i].freq_hz);
            return LGW_LBT_ERROR;
        }
        */
        if ((lbt_channel_cfg[i].scan_time_us != 128) && (lbt_channel_cfg[i].scan_time_us != 5000)) {
            DEBUG_PRINTF("ERROR~ LBT channel scan time is not supported (%u)\n", lbt_channel_cfg[i].scan_time_us);
            return LGW_LBT_ERROR;
        }

    }

    /* Configure SX127x for FSK */

    x = lgw_setup_sx127x(lbt_isftdi, lbt_start_freq, MOD_FSK, LGW_SX127X_RXBW_100K_HZ, lbt_rssi_offset_dB); /* 200KHz LBT channels */
    if (x != LGW_LBT_SUCCESS) {
        DEBUG_MSG("ERROR~ Failed to configure SX127x for LBT\n");
        return LGW_LBT_ERROR;
    }

    DEBUG_MSG("\n-------------------------------------------------\n");
    DEBUG_MSG("Note: LBT configuration:\n");
    DEBUG_PRINTF("\tlbt_enable: %d\n", lbt_enable );
    DEBUG_PRINTF("\tlbt_nb_active_channel: %d\n", lbt_nb_active_channel );
    DEBUG_PRINTF("\tlbt_start_freq: %d\n", lbt_start_freq);
    DEBUG_PRINTF("\tlbt_rssi_target: %d\n", lbt_rssi_target_dBm );
    for (i=0; i<LBT_CHANNEL_FREQ_NB; i++) {
        DEBUG_PRINTF("\tlbt_channel_cfg[%d].freq_hz: %u\n", i, lbt_channel_cfg[i].freq_hz );
        DEBUG_PRINTF("\tlbt_channel_cfg[%d].scan_time_us: %u\n", i, lbt_channel_cfg[i].scan_time_us );
    }
    DEBUG_MSG("-------------------------------------------------\n");

    return LGW_LBT_SUCCESS;

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lbt_start(void) {
    int spi_stat = LGW_SPI_SUCCESS;
    /* check LBT link status */
    if (lgw_lbt_target != NULL) {
        DEBUG_MSG("WARNING: LBT device was already connected\n");
        lgw_spi_close(lgw_lbt_target);
    }

    if (lbt_isftdi)
        spi_stat = lgw_ft_spi_open(&lgw_lbt_target);
    else
        spi_stat = lgw_spi_open(lbt_spi_path, &lgw_lbt_target);

    if (spi_stat != LGW_SPI_SUCCESS) {
        DEBUG_MSG("ERROR CONNECTING LBT TARGET\n");
        lbt_enable = false;
        return LGW_LBT_ERROR;
    }

    return lbt_setup();

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool lbt_single_scan(uint64_t frequency, uint16_t scan_time_us) {
    int x;
    uint8_t rssival = 0;
    int16_t rssi = 0;
    uint8_t PllHop = 1;
    struct timespec start;
    struct timespec end;
    uint64_t freq_reg;

    x = lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_PLLHOP, PllHop << 7); 
    freq_reg = ((uint64_t)frequency << 19) / (uint64_t)32000000;
    x |= lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_FRFMSB, (freq_reg >> 16) & 0xFF);
    x |= lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_FRFMID, (freq_reg >> 8) & 0xFF);
    x |= lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_FRFLSB, (freq_reg >> 0) & 0xFF);

    if (x != SX127X_REG_SUCCESS) {
        DEBUG_PRINTF("ERROR: can't set freq=%llu\n", frequency);
        return false;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    end = start;
    
    while (difftimespec(&end, &start) < scan_time_us) {
        clock_gettime(CLOCK_MONOTONIC, &end);
        x = lgw_sx127x_reg_r(lbt_isftdi, SX1276_REG_RSSIVALUE, &rssival);
        if (x != SX127X_REG_SUCCESS)
            continue;
        rssi = -(rssival >> 1);
        if( rssi > lbt_rssi_target_dBm ) {
            DEBUG_PRINTF("INFO: %llu ReadRssi=%d, targe=%d\n", frequency, rssi, lbt_rssi_target_dBm);
            return false;
        }
    }

    DEBUG_PRINTF("INFO: %llu ReadRssi=%d, targe=%d\n", frequency, rssi, lbt_rssi_target_dBm);

    return true;
}

void lbt_run_rssi_scan(void* exitsig) {
    int i, x;
    bool* stopsig = (bool*) exitsig;
    uint8_t rssival = 0;
    int16_t rssi = 0;
    struct timespec start;
    struct timespec end;
    //uint32_t time_us;
    //struct timeval current_unix_time;
    bool lbt_free;
    uint32_t freq_reg;

    for (i = 0; i < LBT_CHANNEL_FREQ_NB; i++) {
        lbt_chan_free[i] = false;
        //freq_offset[i] = (lbt_channel_cfg[i].freq_hz - lbt_start_freq) / 100E3;
    }

    while (!*stopsig) {
        for (i = 0; i < LBT_CHANNEL_FREQ_NB; i++) {
        //for (i = 0; i < 0xFF; i++) {
            lbt_free = true;
            x = lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_PLLHOP, 1 << 7);  //FastHopOn
            //freq_reg = ((uint64_t)lbt_start_freq << 19) / (uint64_t)32000000;
            freq_reg = ((uint64_t)lbt_channel_cfg[i].freq_hz << 19) / (uint64_t)32000000;
            x |= lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_FRFMSB, (freq_reg >> 16) & 0xFF);
            x |= lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_FRFMID, (freq_reg >> 8) & 0xFF);
            x |= lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_FRFLSB, freq_reg & 0xFF);
            if (x != SX127X_REG_SUCCESS) {
                DEBUG_PRINTF("ERROR: can't set freq=%u\n", lbt_channel_cfg[i].freq_hz);
                wait_us(TS_HOP);
            }
            wait_ms(30);

            clock_gettime(CLOCK_MONOTONIC, &start);
            end = start;
            while (difftimespec(&end, &start) < lbt_channel_cfg[i].scan_time_us) {
            //while (difftimespec(&end, &start) < 128) {
                x = lgw_sx127x_reg_r(lbt_isftdi, SX1276_REG_RSSIVALUE, &rssival);
                if (x != SX127X_REG_SUCCESS)
                    continue;
                rssi = -(rssival >> 1);
                //gettimeofday(&current_unix_time, NULL);
                //time_us = current_unix_time.tv_sec * 1000000UL + current_unix_time.tv_usec;
                //DEBUG_PRINTF("%d: %u => chan = %u, rssi = %d\n", i, time_us, i*100000 + lbt_start_freq, rssi);
                if( rssi > lbt_rssi_target_dBm ) {
                    lbt_free = false;
                    lbt_chan_free[i] = false;
                    break;
                }

                clock_gettime(CLOCK_MONOTONIC, &end);
            }

            if (lbt_free) 
               lbt_chan_free[i] = true;
        }
    }
}

int lbt_chan_is_free(struct lgw_pkt_tx_s * pkt_data, uint16_t tx_start_delay, bool * tx_allowed) {
    int i;
    int lbt_channel_ind = -1;
    uint32_t time_us;
    struct timeval current_unix_time;

    /* Check input parameters */
    if ((pkt_data == NULL) || (tx_allowed == NULL)) {
        return LGW_LBT_ERROR;
    }

    /* Check if TX is allowed */
    if (lbt_enable == true) {
        /* TX allowed for LoRa only */
        if (pkt_data->modulation != MOD_LORA) {
            *tx_allowed = false;
            DEBUG_PRINTF("INFO: TX is not allowed for this modulation (%x)\n", pkt_data->modulation);
            return LGW_LBT_SUCCESS;
        }

        /* Select LBT Channel corresponding to required TX frequency */
        DEBUG_MSG("\n############################(LBT INFO)#############################\n");
        lbt_channel_ind = -1;
        for (i=0; i<lbt_nb_active_channel; i++) {
            if (is_equal_freq(pkt_data->freq_hz, lbt_channel_cfg[i].freq_hz) == true) {
                DEBUG_PRINTF("LBT: select channel %d (%u Hz)\n", i, lbt_channel_cfg[i].freq_hz);
                lbt_channel_ind = i;
                break;
            }
         }

        /* send data if allowed */
        if (lbt_channel_ind >= 0) {
            *tx_allowed = lbt_chan_free[lbt_channel_ind];
             DEBUG_MSG("INFO~ TX request allowed (LBT)\n");
        } else {
            DEBUG_MSG("WARNING~ TX request rejected (LBT)\n");
            *tx_allowed = false;
        }
        gettimeofday(&current_unix_time, NULL);
        time_us = current_unix_time.tv_sec * 1000000UL + current_unix_time.tv_usec;
        DEBUG_MSG("=============================================================\n");
        DEBUG_PRINTF("TIME: %u\n", time_us);
        for (i = 0; i < lbt_nb_active_channel; i++) {
            DEBUG_PRINTF("%u chan is %s\n", lbt_channel_cfg[i].freq_hz, lbt_chan_free[i] ? "Free" : "Busy"); 

        }
        DEBUG_MSG("=============================================================\n");
        DEBUG_MSG("######################################################################\n\n");
         
    } else {
        /* Always allow if LBT is disabled */
        *tx_allowed = true;
    }

    return LGW_LBT_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool lbt_is_enabled(void) {
    return lbt_enable;
}

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

/* As given frequencies have been converted from float to integer, some aliasing
issues can appear, so we can't simply check for equality, but have to take some
margin */
bool is_equal_freq(uint32_t a, uint32_t b) {
    int64_t diff;
    int64_t a64 = (int64_t)a;
    int64_t b64 = (int64_t)b;

    /* Calculate the difference */
    diff = llabs(a64 - b64);

    /* Check for acceptable diff range */
    if( diff <= 10000 )
    {
        return true;
    }

    return false;
}

enum lgw_sx127x_rxbw_e get_rxbw_index(uint32_t hz) {
    switch (hz) {
        case 2600:
            return LGW_SX127X_RXBW_2K6_HZ;
        case 3100:
            return LGW_SX127X_RXBW_3K1_HZ;
        case 3900:
            return LGW_SX127X_RXBW_3K9_HZ;
        case 5200:
            return LGW_SX127X_RXBW_5K2_HZ;
        case 6300:
            return LGW_SX127X_RXBW_6K3_HZ;
        case 7800:
            return LGW_SX127X_RXBW_7K8_HZ;
        case 10400:
            return LGW_SX127X_RXBW_10K4_HZ;
        case 12500:
            return LGW_SX127X_RXBW_12K5_HZ;
        case 15600:
            return LGW_SX127X_RXBW_15K6_HZ;
        case 20800:
            return LGW_SX127X_RXBW_20K8_HZ;
        case 25000:
            return LGW_SX127X_RXBW_25K_HZ;
        case 31300:
            return LGW_SX127X_RXBW_31K3_HZ;
        case 41700:
            return LGW_SX127X_RXBW_41K7_HZ;
        case 50000:
            return LGW_SX127X_RXBW_50K_HZ;
        case 62500:
            return LGW_SX127X_RXBW_62K5_HZ;
        case 83333:
            return LGW_SX127X_RXBW_83K3_HZ;
        case 100000:
            return LGW_SX127X_RXBW_100K_HZ;
        case 125000:
            return LGW_SX127X_RXBW_125K_HZ;
        case 166700:
            return LGW_SX127X_RXBW_166K7_HZ;
        case 200000:
            return LGW_SX127X_RXBW_200K_HZ;
        case 250000:
            return LGW_SX127X_RXBW_250K_HZ;
        default:
            return LGW_SX127X_RXBW_200K_HZ;
    }
}

static uint64_t difftimespec(struct timespec* end, struct timespec* begin) {
    uint64_t x;   
    x = 1E-3 * (end->tv_nsec - begin->tv_nsec);
    x += 1E6 * (end->tv_sec - begin->tv_sec);
    return x;
}

/* --- EOF ------------------------------------------------------------------ */
