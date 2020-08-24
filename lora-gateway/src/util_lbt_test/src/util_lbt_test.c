/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Listen Before Talk basic test application

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Michael Coracin
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf sprintf fopen fputs */

#include <signal.h>     /* sigaction */
#include <unistd.h>     /* getopt access */
#include <stdlib.h>     /* rand */

#include <time.h>      
#include <sys/time.h>

#include "loragw_aux.h"
#include "loragw_reg.h"
#include "loragw_spi.h"
#include "loragw_hal.h"
#include "loragw_radio.h"
#include "loragw_sx1276_fsk.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS & CONSTANTS ------------------------------------------- */

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#define MSG(args...)    fprintf(stderr, args) /* message that is destined to the user */

#define DEFAULT_SX127X_RSSI_OFFSET -1

#define LGW_MIN_NOTCH_FREQ      126000U /* 126 KHz */
#define LGW_MAX_NOTCH_FREQ      250000U /* 250 KHz */
#define LGW_DEFAULT_NOTCH_FREQ  129000U /* 129 KHz */

#define SPI_LBT_PATH    "/dev/spidev2.0"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

void *lgw_lbt_target = NULL; /*! generic pointer to the LBT device */

/* signal handling variables */
struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void sig_handler(int sigio);

void usage (void);

static uint64_t difftimespec(struct timespec* end, struct timespec* begin);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        quit_sig = 1;;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = 1;
    }
}

/* describe command line options */
void usage(void) {
    printf("Available options:\n");
    printf(" -h print this help\n");
    printf(" -f <float> frequency in MHz of the first LBT channel\n");
    printf(" -o <int>   offset in dB to be applied to the SX127x RSSI [-128..127]\n");
    printf(" -r <int>   target RSSI: signal strength target used to detect if the channel is clear or not [-128..0]\n");
    printf(" -d <int>   use ftdi device or not [0, 1]\n");
    printf(" -s <uint>  scan time in µs for all 8 LBT channels [128,5000]\n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char **argv)
{
    int i;
    int xi = 0;

    /* in/out variables */
    double f1 = 0.0;
    uint32_t f_init = 0; /* in Hz */
    uint32_t f_start = 0; /* in Hz */
    int8_t rssi_target_dBm = -80;
    uint16_t scan_time_us = 128;
    uint8_t rssi_value;
    int16_t rssi;
    int8_t rssi_offset = DEFAULT_SX127X_RSSI_OFFSET;
    uint64_t freq_reg;
    bool lbt_isftdi = true;
    struct timespec start;
    struct timespec end;
    uint32_t time_us;
    struct timeval current_unix_time;

    /* parse command line options */
    while ((i = getopt (argc, argv, "h:f:s:r:o:d:")) != -1) {
        switch (i) {
            case 'h':
                usage();
                return EXIT_FAILURE;
                break;

            case 'f':
                i = sscanf(optarg, "%lf", &f1);
                if ((i != 1) || (f1 < 30.0) || (f1 > 3000.0)) {
                    MSG("ERROR: Invalid LBT start frequency\n");
                    usage();
                    return EXIT_FAILURE;
                } else {
                    f_start = (uint32_t)((f1*1e6) + 0.5);/* .5 Hz offset to get rounding instead of truncating */
                }
                break;
            case 's':
                i = sscanf(optarg, "%i", &xi);
                if ((i != 1) || ((xi != 128) && (xi != 5000))) {
                    MSG("ERROR: scan_time_us must be 128 or 5000 \n");
                    usage();
                    return EXIT_FAILURE;
                } else {
                    scan_time_us = xi;
                }
                break;
            case 'r':
                i = sscanf(optarg, "%i", &xi);
                if ((i != 1) || ((xi < -128) && (xi > 0))) {
                    MSG("ERROR: rssi_target must be b/w -128 & 0 \n");
                    usage();
                    return EXIT_FAILURE;
                } else {
                    rssi_target_dBm = xi;
                }
                break;
            case 'o': /* -o <int>  SX127x RSSI offset [-128..127] */
                i = sscanf(optarg, "%i", &xi);
                if((i != 1) || (xi < -128) || (xi > 127)) {
                    MSG("ERROR: rssi_offset must be b/w -128 & 127\n");
                    usage();
                    return EXIT_FAILURE;
                } else {
                    rssi_offset = (int8_t)xi;
                }
                break;
            case 'd':
                i = sscanf(optarg, "%i", &xi);
                if (i != 1) {
                    MSG("ERROR: rssi_target must be 0 or 1\n");
                    usage();
                    return EXIT_FAILURE;
                } else {
                    if (xi > 0)
                        lbt_isftdi = true;
                    else
                        lbt_isftdi = false;
                }
                break;
            default:
                MSG("ERROR: argument parsing use -h option for help\n");
                usage();
                return EXIT_FAILURE;
        }
    }

    MSG("INFO: Starting LoRa Gateway v1.5 LBT test\n");

    /* configure signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    /* Connect to concentrator */
    i = lgw_connect(false, 126000);
    if (i != LGW_REG_SUCCESS) {
        MSG("ERROR: lgw_connect() did not return SUCCESS\n");
        return EXIT_FAILURE;
    }

    f_init = 915000000;

    /* Initialize 1st LBT channel freq if not given by user */
    if (f_start == 0) {
        f_start = f_init;
    } else if (f_start < f_init) {
        MSG("ERROR: LBT start frequency %u is not supported (f_init=%u)\n", f_start, f_init);
        return EXIT_FAILURE;
    }
    MSG("INFO: CUR_FREQ = %u, target_rssi = %d\n", f_start, rssi_target_dBm);

    MSG("lbt_isftdi = %s\n", lbt_isftdi ? "YES" : "NO");

    if (lbt_isftdi)
        i = lgw_ft_spi_open(&lgw_lbt_target);
    else
        i = lgw_spi_open(&lgw_lbt_target, SPI_LBT_PATH);

    if (i != LGW_SPI_SUCCESS) {
        MSG("ERROR CONNECTING LBT TARGET\n");
        return EXIT_FAILURE;
    }

    /* Configure SX127x and read few RSSI points */
    xi = lgw_setup_sx127x(lbt_isftdi, f_start, MOD_FSK, LGW_SX127X_RXBW_100K_HZ, rssi_offset); /* 200KHz LBT channels */
    if (xi != LGW_REG_SUCCESS) {
        MSG("ERROR~ Failed to configure SX127x for LBT\n");
        return -1;
    }
    /*
    for (i = 0; i < 100; i++) {
        lgw_sx127x_reg_r(isftdi, 0x11, &rssi_value); 
        MSG("SX127x RSSI:%i dBm\n", -(rssi_value/2));
        wait_ms(10);
    }
    */

    while (!exit_sig && !quit_sig) {
        for (i = 0; i < 8; i++) {
        //for (i = 0; i < 0xFF; i++) {
            xi = lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_PLLHOP, 1 << 7);  //FastHopOn
            //freq_reg = ((uint64_t)lbt_start_freq << 19) / (uint64_t)32000000;
            freq_reg = ((uint64_t)(f_start + i*200000) << 19) / (uint64_t)32000000;
            xi |= lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_FRFMSB, (freq_reg >> 16) & 0xFF);
            xi |= lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_FRFMID, (freq_reg >> 8) & 0xFF);
            xi |= lgw_sx127x_reg_w(lbt_isftdi, SX1276_REG_FRFLSB, freq_reg & 0xFF);
            if (xi != LGW_REG_SUCCESS) {
                MSG("ERROR: can't set freq=%u\n", f_start + i*200000);
                wait_us(20);
            }
            wait_ms(1000);

            clock_gettime(CLOCK_MONOTONIC, &start);
            end = start;
            while (difftimespec(&end, &start) < scan_time_us) {
                xi = lgw_sx127x_reg_r(lbt_isftdi, SX1276_REG_RSSIVALUE, &rssi_value);
                if (xi != LGW_REG_SUCCESS)
                    continue;
                rssi = -(rssi_value >> 1);
                gettimeofday(&current_unix_time, NULL);
                time_us = current_unix_time.tv_sec * 1000000UL + current_unix_time.tv_usec;
                MSG("%d: %u => chan = %u, rssi = %d\n", i, time_us, i*200000 + f_start, rssi);
                MSG("********************************************************************\n");
                clock_gettime(CLOCK_MONOTONIC, &end);
            }
        }
    }

    /* close SPI link */
    i = lgw_disconnect();
    if (i != LGW_REG_SUCCESS) {
        MSG("ERROR: lgw_disconnect() did not return SUCCESS\n");
        return EXIT_FAILURE;
    }

    MSG("INFO: Exiting LoRa Gateway v1.5 LBT test successfully\n");
    return EXIT_SUCCESS;
}

static uint64_t difftimespec(struct timespec* end, struct timespec* begin) {
    uint64_t x;   
    x = 1E-3 * (end->tv_nsec - begin->tv_nsec);
    x += 1E6 * (end->tv_sec - begin->tv_sec);
    return x;
}

/* --- EOF ------------------------------------------------------------------ */

