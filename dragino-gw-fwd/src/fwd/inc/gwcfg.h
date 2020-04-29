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
 * \brief gateway forward data struct define
 */

#ifndef _GW_H
#define _GW_H

#include <pthread.h>
#include "linkedlists.h"
#include "stats.h"
#include "sxcfg.h"

typedef enum {     /* thread type for control thread head */
    rxpkts,
    stats,
    semtech_up,
    semtech_down,
    ttn_up,
    ttn_down,
    gwtraf,
    gps,
    jit,
    timersync,
    watchdog
} thread_type;

typedef struct _thread_info {
    LGW_LIST_ENTRY(struct _thread_info) list;
    char*       name;
    pthread_t*  id;
    thread_type type;
    bool        runing;
    time_t      dog;
    sem_t*      sema;
} thread_info;

typedef struct {
    struct {
        char gateway_id[17];	/* string form of gateway mac address */
        char platform[24];	    /* platform definition */
        char email[40];			/* used for contact email */
        char description[64];	/* used for free form description */
        //uint64_t  lgwm;			/* Lora gateway MAC address */
        static uint32_t net_mac_h;
        static uint32_t net_mac_l;
    } info;

    struct {
        char board[16];         // SX1301(LG301) / SX1308(LG308) / SX1302(LG302) / SX1276 (LG02)
        bool xtal_correct_ok;
        double xtal_correct;
        /*
        union {
            struct sx1301_conf*  sx1301conf;
            struct sx1302_conf*  sx1302conf;
            struct sx1308_conf*  sx1308conf;
        } sxmt;
        union {
            struct sx1276_conf*  sx1276conf;
            struct sx1261_conf*  sx1261conf;
        } sxsg;
        */
        pthread_mutex_t mx_xcorr;
        pthread_mutex_t mx_concent;
    } hal;

    struct {
        bool     radiostream_enabled;	
        bool     ghoststream_enabled;	
        bool     wd_enabled;		    /* watchdog enabled   */
        time_t   last_loop;             /* timestamp for watchdog */
        uint32_t time_interval;
        uint32_t autoquit_threshold;/* enable auto-quit after a number of non-acknowledged PULL_DATA (0 = disabled) */
        struct lgw_conf_debug_s* debugconf;
    } cfg;

    /* GPS configuration and synchronization */
    struct {
        char   gps_tty_path[64];        /* path of the TTY port GPS is connected on */
        int    gps_tty_fd;
        bool   gps_enabled;		        /* controls the use of the GPS                      */
        bool   gps_ref_valid;           /* is GPS reference acceptable (ie. not too old) */
        struct tref time_reference_gps; /* time reference used for UTC <-> timestamp conversion */
        struct coord_s reference_coord; /* Reference coordinates, for broadcasting (beacon) */
        bool   gps_coord_valid;         /* could we get valid GPS coordinates? */
        struct coord_s meas_gps_coord;  /* GPS position of the gateway */
        struct coord_s meas_gps_err;    /* GPS position of the gateway */
        /* GPS time reference */
        pthread_mutex_t mx_timeref;	    /* control access to GPS time reference */
        pthread_mutex_t mx_meas_gps;	/* control access to the GPS statistics */
    } gps;

    struct {
        uint32_t beacon_period;    /* set beaconing period, must be a sub-multiple of 86400, the nb of sec in a day */
        uint32_t beacon_freq_hz;   /* set beacon TX frequency, in Hz */
        uint8_t  beacon_freq_nb;   /* set number of beaconing channels beacon */
        uint32_t beacon_freq_step; /* set frequency step between beacon channels, in Hz */
        uint8_t  beacon_datarate;  /* set beacon datarate (SF) */
        uint32_t beacon_bw_hz;     /* set beacon bandwidth, in Hz */
        int8_t   beacon_power;     /* set beacon TX power, in dBm */
        uint8_t  beacon_infodesc;  /* set beacon information descriptor */
    } beacon;

    struct {
        bool logger_enabled;       /* controls the activation of more logging */
        int  debug_mask;           /* enabled debugging options */
        char *logfile;             /* path to logfile */
        uint32_t nb_pkt_log[LGW_IF_CHAIN_NB];  /* [CH][SF] */
        uint32_t nb_pkt_received_lora;
        uint32_t nb_pkt_received_fsk;
        uint32_t nb_pkt_received_ref[16];
    } log;

    struct serv_list serv_list;
} gw_s;

#define INIT_GW gw_s GW = {   .info.lgwm = 0;                                        \
                              .hal.mx_concent = PTHREAD_MUTEX_INIT_VALUE;            \
                              .hal.mx_xcorr   = PTHREAD_MUTEX_INIT_VALUE;            \
                              .hal.xtal_correct_ok = false;                          \
                              .hal.xtal_correct = 1.0;                               \
                              .cfg.wd_enabled = false;                               \
                              .cfg.radiostream_enabled = true;                       \
                              .cfg.ghoststream_enabled = false;                      \
                              .cfg.autoquit_threshold = 0;                           \
                              .cfg.time_interval = 30;                               \
                              .gps_tty_path[0] = 0;                                  \
                              .gps.mx_timeref  = PTHREAD_MUTEX_INIT_VALUE;           \
                              .gps.mx_meas_gps = PTHREAD_MUTEX_INIT_VALUE;           \
                              .beacon.beacon_period    = 0;                          \ 
                              .beacon.beacon_freq_hz   = DEFAULT_BEACON_FREQ_HZ;     \ 
                              .beacon.beacon_freq_nb   = DEFAULT_BEACON_FREQ_NB;     \ 
                              .beacon.beacon_freq_step = DEFAULT_BEACON_FREQ_STEP;   \ 
                              .beacon.beacon_datarate  = DEFAULT_BEACON_DATARATE;    \ 
                              .beacon.beacon_bw_hz     = DEFAULT_BEACON_FREQ_HZ;     \ 
                              .beacon.beacon_power     = DEFAULT_BEACON_POWER;       \ 
                              .beacon.beacon_infodesc  = DEFAULT_BEACON_INFODESC;    \ 
                              .log.logger_enabled  = 0;                              \ 
                              .log.debug_mask  = 0;                                  \ 
                              .log.nb_pkt_log[LGW_IF_CHAIN_NB]  = {0};               \ 
                              .log.nb_pkt_received_lora  = 0;                        \ 
                              .log.nb_pkt_received_fsk   = 0;                        \ 
                              .log.nb_pkt_received_ref[16] = {0};                    \ 
                              .serv_list = LGW_LIST_HEAD_INIT_NOLOCK_VALUE;          \
                          }

#define DECLARE_GW extern gw_s GW

int parse_cfg(const char* cfgfile, gw_s* gw);

#endif							// _GW_H
