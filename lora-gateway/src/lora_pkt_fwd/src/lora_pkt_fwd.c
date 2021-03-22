/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Configure Lora concentrator and forward packets to a server
    Use GPS for packet timestamping.
    Send a becon at a regular interval without server intervention

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

#include <stdint.h>         /* C99 types */
#include <stdbool.h>        /* bool type */
#include <stdio.h>          /* printf, fprintf, snprintf, fopen, fputs */

#include <string.h>         /* memset */
#include <signal.h>         /* sigaction */
#include <time.h>           /* time, clock_gettime, strftime, gmtime */
#include <sys/time.h>       /* timeval */
#include <unistd.h>         /* getopt, access */
#include <stdlib.h>         /* atoi, exit */
#include <errno.h>          /* error messages */
#include <math.h>           /* modf */
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <sys/socket.h>     /* socket specific definitions */
#include <netinet/in.h>     /* INET constants and stuff */
#include <arpa/inet.h>      /* IP address conversion stuff */
#include <netdb.h>          /* gai_strerror */

#include <pthread.h>
#include <semaphore.h>

#include <uci.h>

#include "trace.h"
#include "jitqueue.h"
#include "timersync.h"
#include "parson.h"
#include "base64.h"
#include "radio.h"
#include "loragw_hal.h"
#include "loragw_lbt.h"
#include "loragw_gps.h"
#include "loragw_aux.h"
#include "loragw_reg.h"
#include "mac-header-decode.h"
#include "db.h"
#include "loramac-crypto.h"
#include "utilities.h"

/* ------------------------------------------------------- */
/* --- PUBLIC VARIABLE ----------------------------------- */
/* --- debug info: DEBUG level --------------------------- */

uint8_t DEBUG_PKT_FWD    = 0;  
uint8_t DEBUG_REPORT     = 0;
uint8_t DEBUG_JIT        = 1;
uint8_t DEBUG_JIT_ERROR  = 1;
uint8_t DEBUG_TIMERSYNC  = 0;
uint8_t DEBUG_BEACON     = 0;
uint8_t DEBUG_INFO       = 1;
uint8_t DEBUG_WARNING    = 1;
uint8_t DEBUG_ERROR      = 1;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#define STRINGIFY(x)    #x
#define STR(x)          STRINGIFY(x)

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#ifndef VERSION_STRING
  #define VERSION_STRING "undefined"
#endif

#define DEFAULT_SERVER      127.0.0.1   /* hostname also supported */
#define DEFAULT_PORT_UP     1780
#define DEFAULT_PORT_DW     1782
#define DEFAULT_KEEPALIVE   5           /* default time interval for downstream keep-alive packet */
#define DEFAULT_STAT        30          /* default time interval for statistics */
#define PUSH_TIMEOUT_MS     100
#define PULL_TIMEOUT_MS     200
#define GPS_REF_MAX_AGE     30          /* maximum admitted delay in seconds of GPS loss before considering latest GPS sync unusable */
#define FETCH_SLEEP_MS      10          /* nb of ms waited when a fetch return no packets */
#define BEACON_POLL_MS      50          /* time in ms between polling of beacon TX status */

#define PROTOCOL_VERSION    2           /* v1.3 */

#define XERR_INIT_AVG       128         /* nb of measurements the XTAL correction is averaged on as initial value */
#define XERR_FILT_COEF      256         /* coefficient for low-pass XTAL error tracking */

#define PKT_PUSH_DATA   0
#define PKT_PUSH_ACK    1
#define PKT_PULL_DATA   2
#define PKT_PULL_RESP   3
#define PKT_PULL_ACK    4
#define PKT_TX_ACK      5

#define NB_PKT_MAX      8 /* max number of packets per fetch/send cycle */

#define MIN_LORA_PREAMB 6 /* minimum Lora preamble length for this application */
#define STD_LORA_PREAMB 8
#define MIN_FSK_PREAMB  3 /* minimum FSK preamble length for this application */
#define STD_FSK_PREAMB  5

#define STATUS_SIZE     200
#define STAT_BUFF_SIZE  256
#define TX_BUFF_SIZE    ((540 * NB_PKT_MAX) + 30 + STATUS_SIZE)

#define UNIX_GPS_EPOCH_OFFSET 315964800 /* Number of seconds ellapsed between 01.Jan.1970 00:00:00
                                                                          and 06.Jan.1980 00:00:00 */

#define DEFAULT_BEACON_FREQ_HZ      869525000
#define DEFAULT_BEACON_FREQ_NB      1
#define DEFAULT_BEACON_FREQ_STEP    0
#define DEFAULT_BEACON_DATARATE     9
#define DEFAULT_BEACON_BW_HZ        125000
#define DEFAULT_BEACON_POWER        14
#define DEFAULT_BEACON_INFODESC     0

/*stream direction*/
#define UP                          0
#define DOWN                        1

#define FCNT_GAP                    9

#define MAXPAYLOAD                  512

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

/* signal handling variables */
volatile bool exit_sig = false; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
volatile bool quit_sig = false; /* 1 -> application terminates without shutting down the hardware */

/* packets filtering configuration variables */
static bool fwd_valid_pkt = true; /* packets with PAYLOAD CRC OK are forwarded */
static bool fwd_error_pkt = false; /* packets with PAYLOAD CRC ERROR are NOT forwarded */
static bool fwd_nocrc_pkt = false; /* packets with NO PAYLOAD CRC are NOT forwarded */

/* network configuration variables */
static uint64_t lgwm = 0; /* Lora gateway MAC address */
static char serv_addr[64] = STR(DEFAULT_SERVER); /* address of the server (host name or IPv4/IPv6) */
static char serv_port_up[8] = STR(DEFAULT_PORT_UP); /* server port for upstream traffic */
static char serv_port_down[8] = STR(DEFAULT_PORT_DW); /* server port for downstream traffic */
static int keepalive_time = DEFAULT_KEEPALIVE; /* send a PULL_DATA request every X seconds, negative = disabled */

/* statistics collection configuration variables */
static unsigned stat_interval = DEFAULT_STAT; /* time interval (in sec) at which statistics are collected and displayed */

/* gateway <-> MAC protocol variables */
static uint32_t net_mac_h; /* Most Significant Nibble, network order */
static uint32_t net_mac_l; /* Least Significant Nibble, network order */

/* network sockets */
static int sock_up; /* socket for upstream traffic */
static int sock_down; /* socket for downstream traffic */

/* network protocol variables */
static struct timeval push_timeout_half = {0, (PUSH_TIMEOUT_MS * 500)}; /* cut in half, critical for throughput */
static struct timeval pull_timeout = {0, (PULL_TIMEOUT_MS * 1000)}; /* non critical for throughput */

/* hardware access control and correction */
pthread_mutex_t mx_concent = PTHREAD_MUTEX_INITIALIZER; /* control access to the concentrator */
static pthread_mutex_t mx_xcorr = PTHREAD_MUTEX_INITIALIZER; /* control access to the XTAL correction */
//pthread_mutex_t mx_sx1276 = PTHREAD_MUTEX_INITIALIZER; /* control access to the sx1276 */
static bool xtal_correct_ok = false; /* set true when XTAL correction is stable enough */
static double xtal_correct = 1.0;

/* GPS configuration and synchronization */
static char gps_tty_path[64] = "\0"; /* path of the TTY port GPS is connected on */
static int gps_tty_fd = -1; /* file descriptor of the GPS TTY port */
static bool gps_enabled = false; /* is GPS enabled on that gateway ? */

/* GPS time reference */
static pthread_mutex_t mx_timeref = PTHREAD_MUTEX_INITIALIZER; /* control access to GPS time reference */
static bool gps_ref_valid; /* is GPS reference acceptable (ie. not too old) */
static struct tref time_reference_gps; /* time reference used for GPS <-> timestamp conversion */

/* Reference coordinates, for broadcasting (beacon) */
static struct coord_s reference_coord;

/* Enable faking the GPS coordinates of the gateway */
static bool gps_fake_enable; /* enable the feature */

/* measurements to establish statistics */
static pthread_mutex_t mx_meas_up = PTHREAD_MUTEX_INITIALIZER; /* control access to the upstream measurements */
static uint32_t total_pkt_up = 0;
static uint32_t total_pkt_dw = 0;
static uint32_t meas_nb_rx_rcv = 0; /* count packets received */
static uint32_t meas_nb_rx_ok = 0; /* count packets received with PAYLOAD CRC OK */
static uint32_t meas_nb_rx_bad = 0; /* count packets received with PAYLOAD CRC ERROR */
static uint32_t meas_nb_rx_nocrc = 0; /* count packets received with NO PAYLOAD CRC */
static uint32_t meas_up_pkt_fwd = 0; /* number of radio packet forwarded to the server */
static uint32_t meas_up_network_byte = 0; /* sum of UDP bytes sent for upstream traffic */
static uint32_t meas_up_payload_byte = 0; /* sum of radio payload bytes sent for upstream traffic */
static uint32_t meas_up_dgram_sent = 0; /* number of datagrams sent for upstream traffic */
static uint32_t meas_up_ack_rcv = 0; /* number of datagrams acknowledged for upstream traffic */

static pthread_mutex_t mx_meas_dw = PTHREAD_MUTEX_INITIALIZER; /* control access to the downstream measurements */
static uint32_t meas_dw_pull_sent = 0; /* number of PULL requests sent for downstream traffic */
static uint32_t meas_dw_ack_rcv = 0; /* number of PULL requests acknowledged for downstream traffic */
static uint32_t meas_dw_dgram_rcv = 0; /* count PULL response packets received for downstream traffic */
static uint32_t meas_dw_network_byte = 0; /* sum of UDP bytes sent for upstream traffic */
static uint32_t meas_dw_payload_byte = 0; /* sum of radio payload bytes sent for upstream traffic */
static uint32_t meas_nb_tx_ok = 0; /* count packets emitted successfully */
static uint32_t meas_nb_tx_fail = 0; /* count packets were TX failed for other reasons */
static uint32_t meas_nb_tx_requested = 0; /* count TX request from server (downlinks) */
static uint32_t meas_nb_tx_rejected_collision_packet = 0; /* count packets were TX request were rejected due to collision with another packet already programmed */
static uint32_t meas_nb_tx_rejected_collision_beacon = 0; /* count packets were TX request were rejected due to collision with a beacon already programmed */
static uint32_t meas_nb_tx_rejected_too_late = 0; /* count packets were TX request were rejected because it is too late to program it */
static uint32_t meas_nb_tx_rejected_too_early = 0; /* count packets were TX request were rejected because timestamp is too much in advance */
static uint32_t meas_nb_beacon_queued = 0; /* count beacon inserted in jit queue */
static uint32_t meas_nb_beacon_sent = 0; /* count beacon actually sent to concentrator */
static uint32_t meas_nb_beacon_rejected = 0; /* count beacon rejected for queuing */

static pthread_mutex_t mx_meas_gps = PTHREAD_MUTEX_INITIALIZER; /* control access to the GPS statistics */
static bool gps_coord_valid; /* could we get valid GPS coordinates ? */
static struct coord_s meas_gps_coord; /* GPS position of the gateway */
static struct coord_s meas_gps_err; /* GPS position of the gateway */

static pthread_mutex_t mx_sockup = PTHREAD_MUTEX_INITIALIZER; /* control access to the sock_up reconnect */
static pthread_mutex_t mx_sockdn= PTHREAD_MUTEX_INITIALIZER; /* control access to the sock_down reconnect */

/* beacon parameters */
static uint32_t beacon_period = 0; /* set beaconing period, must be a sub-multiple of 86400, the nb of sec in a day */
static uint32_t beacon_freq_hz = DEFAULT_BEACON_FREQ_HZ; /* set beacon TX frequency, in Hz */
static uint8_t beacon_freq_nb = DEFAULT_BEACON_FREQ_NB; /* set number of beaconing channels beacon */
static uint32_t beacon_freq_step = DEFAULT_BEACON_FREQ_STEP; /* set frequency step between beacon channels, in Hz */
static uint8_t beacon_datarate = DEFAULT_BEACON_DATARATE; /* set beacon datarate (SF) */
static uint32_t beacon_bw_hz = DEFAULT_BEACON_BW_HZ; /* set beacon bandwidth, in Hz */
static int8_t beacon_power = DEFAULT_BEACON_POWER; /* set beacon TX power, in dBm */
static uint8_t beacon_infodesc = DEFAULT_BEACON_INFODESC; /* set beacon information descriptor */

/* auto-quit function */
static uint32_t autoquit_threshold = 0; /* enable auto-quit after a number of non-acknowledged PULL_DATA (0 = disabled)*/

/* Just In Time TX scheduling */
static struct jit_queue_s jit_queue;

/* Gateway specificities */
static int8_t antenna_gain = 0;

/* TX capabilities */
static struct lgw_tx_gain_lut_s txlut; /* TX gain table */
static uint32_t tx_freq_min[LGW_RF_CHAIN_NB]; /* lowest frequency supported by TX chain */
static uint32_t tx_freq_max[LGW_RF_CHAIN_NB]; /* highest frequency supported by TX chain */

/* TX RADIO sx1276 */
radiodev *sxradio;
static bool sx1276 = false;
static char server_type[16] = "server_type";

/* --- debuglevel option ----------------------*/
static char debug_level_char[16] = "debug_level";
static uint8_t debug_level_uint = 1;

/* fport option for filter upmsg */
static char fportnum[16] = "fport_filter";
static int fport_num = 0;

/* devaddr option for filter upmsg */
static char devaddr_mask[32] = "devaddr_filter";
static uint32_t dev_addr_mask = 0;

/* Decryption loramac payload */
static char maccrypto[16] = "maccrypto";
static int maccrypto_num = 0;
static char dbpath[32] = "/tmp/db.sqlite";

/* default value of rx2 */
static char gwcfg[8] = "gwcfg"; /*gw Regional*/
static uint8_t rx2bw;
static uint8_t rx2dr;
static uint32_t rx2freq;

/* context of sqlite database */
static struct context cntx = {'\0'};

/* queue about data down */

#define MAX_DNLINK_PKTS   32  /* MAX number of dnlink pkts */
#define DNFPORT           2   /* default fport for downlink */
#define DNPATH      "/var/iot/push"   /* default path for downlink */

static uint32_t dwfcnt = 0;  /* for local downlink frame counter */

typedef struct dnlink {
    char devaddr[16];
    char txmode[8];
    char pdformat[8];
    uint8_t payload[512];
    uint8_t psize;
    int txpw;
    int txbw;
    int txdr;
    int rxwindow;
    uint32_t txfreq;
    struct dnlink *pre;
    struct dnlink *next;
} DNLINK;
static DNLINK *dn_link = NULL;

typedef struct pkts {
    int nb_pkt;
    struct lgw_pkt_rx_s rxpkt[NB_PKT_MAX];
    struct pkts *next;
} PKTS;
static PKTS *rxpkts_link = NULL; /* save the payload receive from radio */

static pthread_mutex_t mx_dnlink = PTHREAD_MUTEX_INITIALIZER; 
static pthread_mutex_t mx_rxpkts_link = PTHREAD_MUTEX_INITIALIZER; 
static sem_t rxpkt_rec_sem;  /* sem for alarm process upload message */

static void prepare_frame(uint8_t type, struct devinfo *devinfo, uint32_t downcnt, const uint8_t* payload, int payload_size, uint8_t* frame, int* frame_size) ;

static DNLINK* search_dnlink(char *addr);

static enum jit_error_e custom_rx2dn(DNLINK *dnelem, struct devinfo *devinfo, uint32_t us, uint8_t txmode);

/* -------------------------------------------------------------------------- */

/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */
static void lgw_exit_fail();

static int init_socket(const char *servaddr, const char *servport, const char *rectimeout, int len);

static void sigusr_handler(int sigio);

static char uci_config_file[32] = "/etc/config/gateway";

static struct uci_context * ctx = NULL; 

static bool get_config(const char *section, char *option, int len);

static int parse_SX1301_configuration(const char * conf_file);

static int parse_gateway_configuration(const char * conf_file);

static uint16_t crc16(const uint8_t * data, unsigned size);

static double difftimespec(struct timespec end, struct timespec beginning);

static void gps_process_sync(void);

static void gps_process_coords(void);

/* threads */
void thread_up(void);
void thread_down(void);
void thread_gps(void);
void thread_valid(void);
void thread_jit(void);
void thread_timersync(void);
void thread_proc_rxpkt(void);
void thread_ent_dnlink(void);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */
void payload_deal(struct lgw_pkt_rx_s* p);

static int strcpypt(char* dest, const char* src, int* start, int size, int len);

static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        quit_sig = true;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = true;
    }
    return;
}

static void sigusr_handler(int sigio) {
    if (sigio == SIGUSR1) {
        printf("INFO~ catch the SIGUSR1, starting reconnet the server ...\n");
        if (sock_up) close(sock_up);
        if (sock_down) close(sock_down);
        if ((sock_up = init_socket(serv_addr, serv_port_up,
                       (void *)&push_timeout_half, sizeof(push_timeout_half))) == -1)
            printf("ERROR~ (up)reconnet the server error, try again!\n");
        if ((sock_down = init_socket(serv_addr, serv_port_down,
                         (void *)&pull_timeout, sizeof(pull_timeout))) == -1)
            printf("ERROR~ (down)reconnet the server error, try again!\n");
    }
}

static bool get_config(const char *section, char *option, int len) {
    struct uci_package * pkg = NULL;
    struct uci_element *e;
    const char *value;
    bool ret = false;

    ctx = uci_alloc_context(); 
    if (UCI_OK != uci_load(ctx, uci_config_file, &pkg))  
        goto cleanup;   /* load uci conifg failed*/

    uci_foreach_element(&pkg->sections, e)
    {
        struct uci_section *st = uci_to_section(e);

        if(!strcmp(section, st->e.name))  /* compare section name */ {
            if (NULL != (value = uci_lookup_option_string(ctx, st, option))) {
	         memset(option, 0, len);
                 strncpy(option, value, len); 
                 ret = true;
                 break;
            }
        }
    }
    uci_unload(ctx, pkg); /* free pkg which is UCI package */
cleanup:
    uci_free_context(ctx);
    ctx = NULL;
    return ret;
}

static int parse_SX1301_configuration(const char * conf_file) {
    int i;
    char param_name[32]; /* used to generate variable parameter names */
    const char *str; /* used to store string value from JSON object */
    const char conf_obj_name[] = "SX130x_conf";
    JSON_Value *root_val = NULL;
    JSON_Object *conf_obj = NULL;
    JSON_Object *conf_lbt_obj = NULL;
    JSON_Object *conf_lbtchan_obj = NULL;
    JSON_Value *val = NULL;
    JSON_Array *conf_array = NULL;
    struct lgw_conf_board_s boardconf;
    struct lgw_conf_lbt_s lbtconf;
    struct lgw_conf_rxrf_s rfconf;
    struct lgw_conf_rxif_s ifconf;
    uint32_t sf, bw, fdev;

    /* try to parse JSON */
    root_val = json_parse_file_with_comments(conf_file);
    if (root_val == NULL) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ %s is not a valid JSON file\n", conf_file);
        exit(EXIT_FAILURE);
    }

    /* point to the gateway configuration object */
    conf_obj = json_object_get_object(json_value_get_object(root_val), conf_obj_name);
    if (conf_obj == NULL) {
        MSG_DEBUG(DEBUG_INFO, "INFO~ %s does not contain a JSON object named %s\n", conf_file, conf_obj_name);
        return -1;
    } else {
        MSG_DEBUG(DEBUG_INFO, "INFO~ %s does contain a JSON object named %s, parsing SX1301 parameters\n", conf_file, conf_obj_name);
    }

    /* set board configuration */
    memset(&boardconf, 0, sizeof boardconf); /* initialize configuration structure */
    val = json_object_get_value(conf_obj, "lorawan_public"); /* fetch value (if possible) */
    if (json_value_get_type(val) == JSONBoolean) {
        boardconf.lorawan_public = (bool)json_value_get_boolean(val);
    } else {
        MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for lorawan_public seems wrong, please check\n");
        boardconf.lorawan_public = false;
    }
    val = json_object_get_value(conf_obj, "clksrc"); /* fetch value (if possible) */
    if (json_value_get_type(val) == JSONNumber) {
        boardconf.clksrc = (uint8_t)json_value_get_number(val);
    } else {
        MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for clksrc seems wrong, please check\n");
        boardconf.clksrc = 0;
    }
    MSG_DEBUG(DEBUG_INFO, "INFO~ lorawan_public %d, clksrc %d\n", boardconf.lorawan_public, boardconf.clksrc);
    /* all parameters parsed, submitting configuration to the HAL */
    if (lgw_board_setconf(boardconf) != LGW_HAL_SUCCESS) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ Failed to configure board\n");
        return -1;
    }

    /* set LBT configuration */
    memset(&lbtconf, 0, sizeof lbtconf); /* initialize configuration structure */
    conf_lbt_obj = json_object_get_object(conf_obj, "lbt_cfg"); /* fetch value (if possible) */
    if (conf_lbt_obj == NULL) {
        MSG_DEBUG(DEBUG_INFO, "INFO~ no configuration for LBT\n");
    } else {
        val = json_object_get_value(conf_lbt_obj, "enable"); /* fetch value (if possible) */
        if (json_value_get_type(val) == JSONBoolean) {
            lbtconf.enable = (bool)json_value_get_boolean(val);
        } else {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for lbt_cfg.enable seems wrong, please check\n");
        }
        if (lbtconf.enable == true) {
            val = json_object_get_value(conf_lbt_obj, "isftdi"); /* fetch value (if possible) */
            if (json_value_get_type(val) == JSONBoolean) {
                lbtconf.isftdi = (bool)json_value_get_boolean(val);
                MSG_DEBUG(DEBUG_WARNING, "INFO~ using FTDI device for lbt scan\n");
            } else {
                MSG_DEBUG(DEBUG_WARNING, "INFO~ using local device for lbt scan\n");
            }
            val = json_object_get_value(conf_lbt_obj, "rssi_target"); /* fetch value (if possible) */
            if (json_value_get_type(val) == JSONNumber) {
                lbtconf.rssi_target = (int8_t)json_value_get_number(val);
            } else {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for lbt_cfg.rssi_target seems wrong, please check\n");
                lbtconf.rssi_target = 0;
            }
            val = json_object_get_value(conf_lbt_obj, "sx127x_rssi_offset"); /* fetch value (if possible) */
            if (json_value_get_type(val) == JSONNumber) {
                lbtconf.rssi_offset = (int8_t)json_value_get_number(val);
            } else {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for lbt_cfg.sx127x_rssi_offset seems wrong, please check\n");
                lbtconf.rssi_offset = 0;
            }
            /* set LBT channels configuration */
            conf_array = json_object_get_array(conf_lbt_obj, "chan_cfg");
            if (conf_array != NULL) {
                lbtconf.nb_channel = json_array_get_count( conf_array );
                MSG_DEBUG(DEBUG_INFO, "INFO~ %u LBT channels configured\n", lbtconf.nb_channel);
            }
            for (i = 0; i < (int)lbtconf.nb_channel; i++) {
                /* Sanity check */
                if (i >= LBT_CHANNEL_FREQ_NB)
                {
                    MSG_DEBUG(DEBUG_ERROR, "ERROR~ LBT channel %d not supported, skip it\n", i );
                    break;
                }
                /* Get LBT channel configuration object from array */
                conf_lbtchan_obj = json_array_get_object(conf_array, i);

                /* Channel frequency */
                val = json_object_dotget_value(conf_lbtchan_obj, "freq_hz"); /* fetch value (if possible) */
                if (json_value_get_type(val) == JSONNumber) {
                    lbtconf.channels[i].freq_hz = (uint32_t)json_value_get_number(val);
                } else {
                    MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for lbt_cfg.channels[%d].freq_hz seems wrong, please check\n", i);
                    lbtconf.channels[i].freq_hz = 0;
                }

                /* Channel scan time */
                val = json_object_dotget_value(conf_lbtchan_obj, "scan_time_us"); /* fetch value (if possible) */
                if (json_value_get_type(val) == JSONNumber) {
                    lbtconf.channels[i].scan_time_us = (uint16_t)json_value_get_number(val);
                } else {
                    MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for lbt_cfg.channels[%d].scan_time_us seems wrong, please check\n", i);
                    lbtconf.channels[i].scan_time_us = 0;
                }
            }

            /* all parameters parsed, submitting configuration to the HAL */
            if (lgw_lbt_setconf(lbtconf) != LGW_HAL_SUCCESS) {
                MSG_DEBUG(DEBUG_ERROR, "ERROR~ Failed to configure LBT\n");
                return -1;
            }
        } else {
            MSG_DEBUG(DEBUG_INFO, "INFO~ LBT is disabled\n");
        }
    }

    /* set antenna gain configuration */
    val = json_object_get_value(conf_obj, "antenna_gain"); /* fetch value (if possible) */
    if (val != NULL) {
        if (json_value_get_type(val) == JSONNumber) {
            antenna_gain = (int8_t)json_value_get_number(val);
        } else {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for antenna_gain seems wrong, please check\n");
            antenna_gain = 0;
        }
    }
    MSG_DEBUG(DEBUG_INFO, "INFO~ antenna_gain %d dBi\n", antenna_gain);

    /* set configuration for tx gains */
    memset(&txlut, 0, sizeof txlut); /* initialize configuration structure */
    for (i = 0; i < TX_GAIN_LUT_SIZE_MAX; i++) {
        snprintf(param_name, sizeof param_name, "tx_lut_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ no configuration for tx gain lut %i\n", i);
            continue;
        }
        txlut.size++; /* update TX LUT size based on JSON object found in configuration file */
        /* there is an object to configure that TX gain index, let's parse it */
        snprintf(param_name, sizeof param_name, "tx_lut_%i.pa_gain", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].pa_gain = (uint8_t)json_value_get_number(val);
        } else {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for %s[%d] seems wrong, please check\n", param_name, i);
            txlut.lut[i].pa_gain = 0;
        }
        snprintf(param_name, sizeof param_name, "tx_lut_%i.dac_gain", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].dac_gain = (uint8_t)json_value_get_number(val);
        } else {
            txlut.lut[i].dac_gain = 3; /* This is the only dac_gain supported for now */
        }
        snprintf(param_name, sizeof param_name, "tx_lut_%i.dig_gain", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].dig_gain = (uint8_t)json_value_get_number(val);
        } else {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for %s[%d] seems wrong, please check\n", param_name, i);
            txlut.lut[i].dig_gain = 0;
        }
        snprintf(param_name, sizeof param_name, "tx_lut_%i.mix_gain", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].mix_gain = (uint8_t)json_value_get_number(val);
        } else {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for %s[%d] seems wrong, please check\n", param_name, i);
            txlut.lut[i].mix_gain = 0;
        }
        snprintf(param_name, sizeof param_name, "tx_lut_%i.rf_power", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONNumber) {
            txlut.lut[i].rf_power = (int8_t)json_value_get_number(val);
        } else {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ Data type for %s[%d] seems wrong, please check\n", param_name, i);
            txlut.lut[i].rf_power = 0;
        }
    }
    /* all parameters parsed, submitting configuration to the HAL */
    if (txlut.size > 0) {
        MSG_DEBUG(DEBUG_INFO, "INFO~ Configuring TX LUT with %u indexes\n", txlut.size);
        if (lgw_txgain_setconf(&txlut) != LGW_HAL_SUCCESS) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ Failed to configure concentrator TX Gain LUT\n");
            return -1;
        }
    } else {
        MSG_DEBUG(DEBUG_WARNING, "WARNING~ No TX gain LUT defined\n");
    }

    /* set configuration for RF chains */
    for (i = 0; i < LGW_RF_CHAIN_NB; ++i) {
        memset(&rfconf, 0, sizeof rfconf); /* initialize configuration structure */
        snprintf(param_name, sizeof param_name, "radio_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ no configuration for radio %i\n", i);
            continue;
        }
        /* there is an object to configure that radio, let's parse it */
        snprintf(param_name, sizeof param_name, "radio_%i.enable", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONBoolean) {
            rfconf.enable = (bool)json_value_get_boolean(val);
        } else {
            rfconf.enable = false;
        }
        if (rfconf.enable == false) { /* radio disabled, nothing else to parse */
            MSG_DEBUG(DEBUG_INFO, "INFO~ radio %i disabled\n", i);
        } else  { /* radio enabled, will parse the other parameters */
            snprintf(param_name, sizeof param_name, "radio_%i.freq", i);
            rfconf.freq_hz = (uint32_t)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.rssi_offset", i);
            rfconf.rssi_offset = (float)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.type", i);
            str = json_object_dotget_string(conf_obj, param_name);
            if (!strncmp(str, "SX1255", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1255;
            } else if (!strncmp(str, "SX1257", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1257;
            } else {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ invalid radio type: %s (should be SX1255 or SX1257)\n", str);
            }
            snprintf(param_name, sizeof param_name, "radio_%i.tx_enable", i);
            val = json_object_dotget_value(conf_obj, param_name);
            if (json_value_get_type(val) == JSONBoolean) {
                rfconf.tx_enable = (bool)json_value_get_boolean(val);
                if (rfconf.tx_enable == true) {
                    /* tx is enabled on this rf chain, we need its frequency range */
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_freq_min", i);
                    tx_freq_min[i] = (uint32_t)json_object_dotget_number(conf_obj, param_name);
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_freq_max", i);
                    tx_freq_max[i] = (uint32_t)json_object_dotget_number(conf_obj, param_name);
                    if ((tx_freq_min[i] == 0) || (tx_freq_max[i] == 0)) {
                        MSG_DEBUG(DEBUG_WARNING, "WARNING~ no frequency range specified for TX rf chain %d\n", i);
                    }
                    /* ... and the notch filter frequency to be set */
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_notch_freq", i);
                    rfconf.tx_notch_freq = (uint32_t)json_object_dotget_number(conf_obj, param_name);
                }
            } else {
                rfconf.tx_enable = false;
            }
            MSG_DEBUG(DEBUG_INFO, "INFO~ radio %i enabled (type %s), center frequency %u, RSSI offset %f, tx enabled %d, tx_notch_freq %u\n", i, str, rfconf.freq_hz, rfconf.rssi_offset, rfconf.tx_enable, rfconf.tx_notch_freq);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxrf_setconf(i, rfconf) != LGW_HAL_SUCCESS) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ invalid configuration for radio %i\n", i);
            return -1;
        }
    }

    /* set configuration for Lora multi-SF channels (bandwidth cannot be set) */
    for (i = 0; i < LGW_MULTI_NB; ++i) {
        memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
        snprintf(param_name, sizeof param_name, "chan_multiSF_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ no configuration for Lora multi-SF channel %i\n", i);
            continue;
        }
        /* there is an object to configure that Lora multi-SF channel, let's parse it */
        snprintf(param_name, sizeof param_name, "chan_multiSF_%i.enable", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) { /* Lora multi-SF channel disabled, nothing else to parse */
            MSG_DEBUG(DEBUG_INFO, "INFO~ Lora multi-SF channel %i disabled\n", i);
        } else  { /* Lora multi-SF channel enabled, will parse the other parameters */
            snprintf(param_name, sizeof param_name, "chan_multiSF_%i.radio", i);
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "chan_multiSF_%i.if", i);
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, param_name);
            // TODO: handle individual SF enabling and disabling (spread_factor)
            MSG_DEBUG(DEBUG_INFO, "INFO~ Lora multi-SF channel %i>  radio %i, IF %i Hz, 125 kHz bw, SF 7 to 12\n", i, ifconf.rf_chain, ifconf.freq_hz);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxif_setconf(i, ifconf) != LGW_HAL_SUCCESS) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ invalid configuration for Lora multi-SF channel %i\n", i);
            return -1;
        }
    }

    /* set configuration for Lora standard channel */
    memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
    val = json_object_get_value(conf_obj, "chan_Lora_std"); /* fetch value (if possible) */
    if (json_value_get_type(val) != JSONObject) {
        MSG_DEBUG(DEBUG_INFO, "INFO~ no configuration for Lora standard channel\n");
    } else {
        val = json_object_dotget_value(conf_obj, "chan_Lora_std.enable");
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ Lora standard channel %i disabled\n", i);
        } else  {
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.radio");
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.if");
            bw = (uint32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.bandwidth");
            switch(bw) {
                case 500000: ifconf.bandwidth = BW_500KHZ; break;
                case 250000: ifconf.bandwidth = BW_250KHZ; break;
                case 125000: ifconf.bandwidth = BW_125KHZ; break;
                default: ifconf.bandwidth = BW_UNDEFINED;
            }
            sf = (uint32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.spread_factor");
            switch(sf) {
                case  7: ifconf.datarate = DR_LORA_SF7;  break;
                case  8: ifconf.datarate = DR_LORA_SF8;  break;
                case  9: ifconf.datarate = DR_LORA_SF9;  break;
                case 10: ifconf.datarate = DR_LORA_SF10; break;
                case 11: ifconf.datarate = DR_LORA_SF11; break;
                case 12: ifconf.datarate = DR_LORA_SF12; break;
                default: ifconf.datarate = DR_UNDEFINED;
            }
            MSG_DEBUG(DEBUG_INFO, "INFO~ Lora std channel> radio %i, IF %i Hz, %u Hz bw, SF %u\n", ifconf.rf_chain, ifconf.freq_hz, bw, sf);
        }
        if (lgw_rxif_setconf(8, ifconf) != LGW_HAL_SUCCESS) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ invalid configuration for Lora standard channel\n");
            return -1;
        }
    }

    /* set configuration for FSK channel */
    memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
    val = json_object_get_value(conf_obj, "chan_FSK"); /* fetch value (if possible) */
    if (json_value_get_type(val) != JSONObject) {
        MSG_DEBUG(DEBUG_INFO, "INFO~ no configuration for FSK channel\n");
    } else {
        val = json_object_dotget_value(conf_obj, "chan_FSK.enable");
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ FSK channel %i disabled\n", i);
        } else  {
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.radio");
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, "chan_FSK.if");
            bw = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.bandwidth");
            fdev = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.freq_deviation");
            ifconf.datarate = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.datarate");

            /* if chan_FSK.bandwidth is set, it has priority over chan_FSK.freq_deviation */
            if ((bw == 0) && (fdev != 0)) {
                bw = 2 * fdev + ifconf.datarate;
            }
            if      (bw == 0)      ifconf.bandwidth = BW_UNDEFINED;
            else if (bw <= 7800)   ifconf.bandwidth = BW_7K8HZ;
            else if (bw <= 15600)  ifconf.bandwidth = BW_15K6HZ;
            else if (bw <= 31200)  ifconf.bandwidth = BW_31K2HZ;
            else if (bw <= 62500)  ifconf.bandwidth = BW_62K5HZ;
            else if (bw <= 125000) ifconf.bandwidth = BW_125KHZ;
            else if (bw <= 250000) ifconf.bandwidth = BW_250KHZ;
            else if (bw <= 500000) ifconf.bandwidth = BW_500KHZ;
            else ifconf.bandwidth = BW_UNDEFINED;

            MSG_DEBUG(DEBUG_INFO, "INFO~ FSK channel> radio %i, IF %i Hz, %u Hz bw, %u bps datarate\n", ifconf.rf_chain, ifconf.freq_hz, bw, ifconf.datarate);
        }
        if (lgw_rxif_setconf(9, ifconf) != LGW_HAL_SUCCESS) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ invalid configuration for FSK channel\n");
            return -1;
        }
    }
    json_value_free(root_val);

    return 0;
}

static int parse_gateway_configuration(const char * conf_file) {
    const char conf_obj_name[] = "gateway_conf";
    JSON_Value *root_val;
    JSON_Object *conf_obj = NULL;
    JSON_Object *serv_obj = NULL;
    JSON_Array *serv_arry = NULL;
    JSON_Value *val = NULL; /* needed to detect the absence of some fields */
    const char *str; /* pointer to sub-strings in the JSON data */
    unsigned long long ull = 0;
	
    /* try to parse JSON */
    root_val = json_parse_file_with_comments(conf_file);
    if (root_val == NULL) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ %s is not a valid JSON file\n", conf_file);
        exit(EXIT_FAILURE);
    }

    /* point to the gateway configuration object */
    conf_obj = json_object_get_object(json_value_get_object(root_val), conf_obj_name);
    if (conf_obj == NULL) {
        MSG_DEBUG(DEBUG_INFO, "INFO~ %s does not contain a JSON object named %s\n", conf_file, conf_obj_name);
        return -1;
    } else {
        MSG_DEBUG(DEBUG_INFO, "INFO~ %s does contain a JSON object named %s, parsing gateway parameters\n", conf_file, conf_obj_name);
    }

    /* gateway unique identifier (aka MAC address) (optional) */
    str = json_object_get_string(conf_obj, "gateway_ID");
    if (str != NULL) {
        sscanf(str, "%llx", &ull);
        lgwm = ull;
        MSG_DEBUG(DEBUG_INFO, "INFO~ gateway MAC address is configured to %016llX\n", ull);
    }

	
// 开始配置Server info	
	serv_arry = json_object_get_array(conf_obj, "servers");
	if (serv_arry != NULL) {
		serv_obj = json_array_get_object(serv_arry, 0);
				
		/* server hostname or IP address (optional) */
		str = json_object_get_string(serv_obj, "server_address");
		if (str != NULL) {
			strncpy(serv_addr, str, sizeof serv_addr);
			MSG_DEBUG(DEBUG_INFO, "INFO~ server hostname or IP address is configured to \"%s\"\n", serv_addr);
		}

		/* get up and down ports (optional) */
		val = json_object_get_value(serv_obj, "serv_port_up");
		if (val != NULL) {
			snprintf(serv_port_up, sizeof serv_port_up, "%u", (uint16_t)json_value_get_number(val));
			MSG_DEBUG(DEBUG_INFO, "INFO~ upstream port is configured to \"%s\"\n", serv_port_up);
		}
		val = json_object_get_value(serv_obj, "serv_port_down");
		if (val != NULL) {
			snprintf(serv_port_down, sizeof serv_port_down, "%u", (uint16_t)json_value_get_number(val));
			MSG_DEBUG(DEBUG_INFO, "INFO~ downstream port is configured to \"%s\"\n", serv_port_down);
		}

		/* get keep-alive interval (in seconds) for downstream (optional) */
		val = json_object_get_value(serv_obj, "keepalive_interval");
		if (val != NULL) {
			keepalive_time = (int)json_value_get_number(val);
			MSG_DEBUG(DEBUG_INFO, "INFO~ downstream keep-alive interval is configured to %u seconds\n", keepalive_time);
		}

		/* get interval (in seconds) for statistics display (optional) */
		val = json_object_get_value(conf_obj, "stat_interval");
		if (val != NULL) {
			stat_interval = (unsigned)json_value_get_number(val);
			MSG_DEBUG(DEBUG_INFO, "INFO~ statistics display interval is configured to %u seconds\n", stat_interval);
		}

		/* get time-out value (in ms) for upstream datagrams (optional) */
		val = json_object_get_value(serv_obj, "push_timeout_ms");
		if (val != NULL) {
			push_timeout_half.tv_usec = 500 * (long int)json_value_get_number(val);
			MSG_DEBUG(DEBUG_INFO, "INFO~ upstream PUSH_DATA time-out is configured to %u ms\n", (unsigned)(push_timeout_half.tv_usec / 500));
		}

		/* packet filtering parameters */
		val = json_object_get_value(serv_obj, "forward_crc_valid");
		if (json_value_get_type(val) == JSONBoolean) {
			fwd_valid_pkt = (bool)json_value_get_boolean(val);
		}
		MSG_DEBUG(DEBUG_INFO, "INFO~ packets received with a valid CRC will%s be forwarded\n", (fwd_valid_pkt ? "" : " NOT"));
		val = json_object_get_value(serv_obj, "forward_crc_error");
		if (json_value_get_type(val) == JSONBoolean) {
			fwd_error_pkt = (bool)json_value_get_boolean(val);
		}
		MSG_DEBUG(DEBUG_INFO, "INFO~ packets received with a CRC error will%s be forwarded\n", (fwd_error_pkt ? "" : " NOT"));
		val = json_object_get_value(serv_obj, "forward_crc_disabled");
		if (json_value_get_type(val) == JSONBoolean) {
			fwd_nocrc_pkt = (bool)json_value_get_boolean(val);
		}
		MSG_DEBUG(DEBUG_INFO, "INFO~ packets received with no CRC will%s be forwarded\n", (fwd_nocrc_pkt ? "" : " NOT"));
		
		
	
    } else 
        printf("WARNING~ No service offer.\n");
	
/////以上为服务器配置	
	
	
	
    /* GPS module TTY path (optional) */
    str = json_object_get_string(conf_obj, "gps_tty_path");
    if (str != NULL) {
        strncpy(gps_tty_path, str, sizeof gps_tty_path);
        MSG_DEBUG(DEBUG_INFO, "INFO~ GPS serial port path is configured to \"%s\"\n", gps_tty_path);
    }

    /* get reference coordinates */
    val = json_object_get_value(conf_obj, "ref_latitude");
    if (val != NULL) {
        reference_coord.lat = (double)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Reference latitude is configured to %f deg\n", reference_coord.lat);
    }
    val = json_object_get_value(conf_obj, "ref_longitude");
    if (val != NULL) {
        reference_coord.lon = (double)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Reference longitude is configured to %f deg\n", reference_coord.lon);
    }
    val = json_object_get_value(conf_obj, "ref_altitude");
    if (val != NULL) {
        reference_coord.alt = (short)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Reference altitude is configured to %i meters\n", reference_coord.alt);
    }

    /* Gateway GPS coordinates hardcoding (aka. faking) option */
    val = json_object_get_value(conf_obj, "fake_gps");
    if (json_value_get_type(val) == JSONBoolean) {
        gps_fake_enable = (bool)json_value_get_boolean(val);
        if (gps_fake_enable == true) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ fake GPS is enabled\n");
        } else {
            MSG_DEBUG(DEBUG_INFO, "INFO~ fake GPS is disabled\n");
        }
    }

    /* Beacon signal period (optional) */
    val = json_object_get_value(conf_obj, "beacon_period");
    if (val != NULL) {
        beacon_period = (uint32_t)json_value_get_number(val);
        if ((beacon_period > 0) && (beacon_period < 6)) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ invalid configuration for Beacon period, must be >= 6s\n");
            return -1;
        } else {
            MSG_DEBUG(DEBUG_INFO, "INFO~ Beaconing period is configured to %u seconds\n", beacon_period);
        }
    }

    /* Beacon TX frequency (optional) */
    val = json_object_get_value(conf_obj, "beacon_freq_hz");
    if (val != NULL) {
        beacon_freq_hz = (uint32_t)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Beaconing signal will be emitted at %u Hz\n", beacon_freq_hz);
    }

    /* Number of beacon channels (optional) */
    val = json_object_get_value(conf_obj, "beacon_freq_nb");
    if (val != NULL) {
        beacon_freq_nb = (uint8_t)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Beaconing channel number is set to %u\n", beacon_freq_nb);
    }

    /* Frequency step between beacon channels (optional) */
    val = json_object_get_value(conf_obj, "beacon_freq_step");
    if (val != NULL) {
        beacon_freq_step = (uint32_t)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Beaconing channel frequency step is set to %uHz\n", beacon_freq_step);
    }

    /* Beacon datarate (optional) */
    val = json_object_get_value(conf_obj, "beacon_datarate");
    if (val != NULL) {
        beacon_datarate = (uint8_t)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Beaconing datarate is set to SF%d\n", beacon_datarate);
    }

    /* Beacon modulation bandwidth (optional) */
    val = json_object_get_value(conf_obj, "beacon_bw_hz");
    if (val != NULL) {
        beacon_bw_hz = (uint32_t)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Beaconing modulation bandwidth is set to %dHz\n", beacon_bw_hz);
    }

    /* Beacon TX power (optional) */
    val = json_object_get_value(conf_obj, "beacon_power");
    if (val != NULL) {
        beacon_power = (int8_t)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Beaconing TX power is set to %ddBm\n", beacon_power);
    }

    /* Beacon information descriptor (optional) */
    val = json_object_get_value(conf_obj, "beacon_infodesc");
    if (val != NULL) {
        beacon_infodesc = (uint8_t)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Beaconing information descriptor is set to %u\n", beacon_infodesc);
    }

    /* Auto-quit threshold (optional) */
    val = json_object_get_value(conf_obj, "autoquit_threshold");
    if (val != NULL) {
        autoquit_threshold = (uint32_t)json_value_get_number(val);
        MSG_DEBUG(DEBUG_INFO, "INFO~ Auto-quit after %u non-acknowledged PULL_DATA\n", autoquit_threshold);
    }

    /* free JSON parsing data structure */
    json_value_free(root_val);
    return 0;
}

static uint16_t crc16(const uint8_t * data, unsigned size) {
    const uint16_t crc_poly = 0x1021;
    const uint16_t init_val = 0x0000;
    uint16_t x = init_val;
    unsigned i, j;

    if (data == NULL)  {
        return 0;
    }

    for (i=0; i<size; ++i) {
        x ^= (uint16_t)data[i] << 8;
        for (j=0; j<8; ++j) {
            x = (x & 0x8000) ? (x<<1) ^ crc_poly : (x<<1);
        }
    }

    return x;
}

static double difftimespec(struct timespec end, struct timespec beginning) {
    double x;

    x = 1E-9 * (double)(end.tv_nsec - beginning.tv_nsec);
    x += (double)(end.tv_sec - beginning.tv_sec);

    return x;
}

static int send_tx_ack(uint8_t token_h, uint8_t token_l, enum jit_error_e error) {
    uint8_t buff_ack[64]; /* buffer to give feedback to server */
    int buff_index;

    /* reset buffer */
    memset(&buff_ack, 0, sizeof buff_ack);

    /* Prepare downlink feedback to be sent to server */
    buff_ack[0] = PROTOCOL_VERSION;
    buff_ack[1] = token_h;
    buff_ack[2] = token_l;
    buff_ack[3] = PKT_TX_ACK;
    *(uint32_t *)(buff_ack + 4) = net_mac_h;
    *(uint32_t *)(buff_ack + 8) = net_mac_l;
    buff_index = 12; /* 12-byte header */

    /* Put no JSON string if there is nothing to report */
    if (error != JIT_ERROR_OK) {
        /* start of JSON structure */
        memcpy((void *)(buff_ack + buff_index), (void *)"{\"txpk_ack\":{", 13);
        buff_index += 13;
        /* set downlink error status in JSON structure */
        memcpy((void *)(buff_ack + buff_index), (void *)"\"error\":", 8);
        buff_index += 8;
        switch (error) {
            case JIT_ERROR_FULL:
            case JIT_ERROR_COLLISION_PACKET:
                memcpy((void *)(buff_ack + buff_index), (void *)"\"COLLISION_PACKET\"", 18);
                buff_index += 18;
                /* update stats */
                pthread_mutex_lock(&mx_meas_dw);
                meas_nb_tx_rejected_collision_packet += 1;
                pthread_mutex_unlock(&mx_meas_dw);
                break;
            case JIT_ERROR_TOO_LATE:
                memcpy((void *)(buff_ack + buff_index), (void *)"\"TOO_LATE\"", 10);
                buff_index += 10;
                /* update stats */
                pthread_mutex_lock(&mx_meas_dw);
                meas_nb_tx_rejected_too_late += 1;
                pthread_mutex_unlock(&mx_meas_dw);
                break;
            case JIT_ERROR_TOO_EARLY:
                memcpy((void *)(buff_ack + buff_index), (void *)"\"TOO_EARLY\"", 11);
                buff_index += 11;
                /* update stats */
                pthread_mutex_lock(&mx_meas_dw);
                meas_nb_tx_rejected_too_early += 1;
                pthread_mutex_unlock(&mx_meas_dw);
                break;
            case JIT_ERROR_COLLISION_BEACON:
                memcpy((void *)(buff_ack + buff_index), (void *)"\"COLLISION_BEACON\"", 18);
                buff_index += 18;
                /* update stats */
                pthread_mutex_lock(&mx_meas_dw);
                meas_nb_tx_rejected_collision_beacon += 1;
                pthread_mutex_unlock(&mx_meas_dw);
                break;
            case JIT_ERROR_TX_FREQ:
                memcpy((void *)(buff_ack + buff_index), (void *)"\"TX_FREQ\"", 9);
                buff_index += 9;
                break;
            case JIT_ERROR_TX_POWER:
                memcpy((void *)(buff_ack + buff_index), (void *)"\"TX_POWER\"", 10);
                buff_index += 10;
                break;
            case JIT_ERROR_GPS_UNLOCKED:
                memcpy((void *)(buff_ack + buff_index), (void *)"\"GPS_UNLOCKED\"", 14);
                buff_index += 14;
                break;
            default:
                memcpy((void *)(buff_ack + buff_index), (void *)"\"UNKNOWN\"", 9);
                buff_index += 9;
                break;
        }
        /* end of JSON structure */
        memcpy((void *)(buff_ack + buff_index), (void *)"}}", 2);
        buff_index += 2;
    }

    buff_ack[buff_index] = 0; /* add string terminator, for safety */

    /* send datagram to server */
    return send(sock_down, (void *)buff_ack, buff_index, 0);
}

static int init_socket(const char *servaddr, const char *servport, const char *rectimeout, int len) {
    int i, sockfd;
    /* network socket creation */
    struct addrinfo hints;
    struct addrinfo *result; /* store result of getaddrinfo */
    struct addrinfo *q; /* pointer to move into *result data */

    char host_name[64];
    char port_name[64];

    /* prepare hints to open network sockets */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; /* WA: Forcing IPv4 as AF_UNSPEC makes connection on localhost to fail */
    hints.ai_socktype = SOCK_DGRAM;

    /* look for server address w/ upstream port */
    i = getaddrinfo(servaddr, servport, &hints, &result);
    if (i != 0) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr, servport, gai_strerror(i));
        return -1;
    }

    /* try to open socket for upstream traffic */
    for (q=result; q!=NULL; q=q->ai_next) {
        sockfd = socket(q->ai_family, q->ai_socktype, q->ai_protocol);
        if (sockfd == -1) continue; /* try next field */
        else break; /* success, get out of loop */
    }
    if (q == NULL) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] failed to open socket to any of server %s addresses (port %s)\n", servaddr, servport);
        i = 1;
        for (q=result; q!=NULL; q=q->ai_next) {
            getnameinfo(q->ai_addr, q->ai_addrlen, host_name, sizeof host_name, port_name, sizeof port_name, NI_NUMERICHOST);
            MSG_DEBUG(DEBUG_INFO, "INFO~ [up] result %i host:%s service:%s\n", i, host_name, port_name);
            ++i;
        }

        return -1;
    }

    /* connect so we can send/receive packet with the server only */
    i = connect(sockfd, q->ai_addr, q->ai_addrlen);
    if (i != 0) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] connect returned %s\n", strerror(errno));
        return -1;
    }

    freeaddrinfo(result);

    if ((setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, rectimeout, len)) != 0) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] setsockopt returned %s\n", strerror(errno));
        return -1;
    }

    MSG_DEBUG(DEBUG_INFO, "INFO~ sockfd=%d\n", sockfd);

    return sockfd;
}

/* output the connections status to a file (/var/iot/status) */
static void output_status(int conn) {
    FILE *fp;
    fp = fopen("/var/iot/status", "w+");
    if (NULL != fp) {
        if (conn == 1) 
            fprintf(fp, "online\n"); 
        else 
            fprintf(fp, "offline\n"); 
        fflush(fp);
        fclose(fp);
    } else
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ connot open status file: /var/iot/status \n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(void)
{
    struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
    struct sigaction sigusr; /* SIGUSR1 signal handling */
    int i, j; /* loop variable and temporary variable for return value */
    int x;

    /* configuration file related */
    char *global_cfg_path= "/etc/lora/global_conf.json"; /* contain global (typ. network-wide) configuration */
    char *local_cfg_path = "/etc/lora/local_conf.json"; /* contain node specific configuration, overwrite global parameters for parameters that are defined in both */
    char *debug_cfg_path = "/etc/lora/debug_conf.json"; /* if present, all other configuration files are ignored */

    /* threads */
    pthread_t thrid_up;
    pthread_t thrid_down;
    pthread_t thrid_gps;
    pthread_t thrid_valid;
    pthread_t thrid_jit;
    pthread_t thrid_lbt;
    pthread_t thrid_timersync;

    pthread_t thrid_proc_rxpkt;
    pthread_t thrid_ent_dnlink;

    /* variables to get local copies of measurements */
    uint32_t cp_nb_rx_rcv;
    uint32_t cp_nb_rx_ok;
    uint32_t cp_nb_rx_bad;
    uint32_t cp_nb_rx_nocrc;
    uint32_t cp_up_pkt_fwd;
    uint32_t cp_up_network_byte;
    uint32_t cp_up_payload_byte;
    uint32_t cp_up_dgram_sent;
    uint32_t cp_up_ack_rcv;
    uint32_t cp_dw_pull_sent;
    uint32_t cp_dw_ack_rcv;
    uint32_t cp_dw_dgram_rcv;
    uint32_t cp_dw_network_byte;
    uint32_t cp_dw_payload_byte;
    uint32_t cp_nb_tx_ok;
    uint32_t cp_nb_tx_fail;
    uint32_t cp_nb_tx_requested = 0;
    uint32_t cp_nb_tx_rejected_collision_packet = 0;
    uint32_t cp_nb_tx_rejected_collision_beacon = 0;
    uint32_t cp_nb_tx_rejected_too_late = 0;
    uint32_t cp_nb_tx_rejected_too_early = 0;
    uint32_t cp_nb_beacon_queued = 0;
    uint32_t cp_nb_beacon_sent = 0;
    uint32_t cp_nb_beacon_rejected = 0;

    /* GPS coordinates variables */
    bool coord_ok = false;
    struct coord_s cp_gps_coord = {0.0, 0.0, 0};

    /* SX1301 data variables */
    uint32_t trig_tstamp;

    /* statistics variable */
    time_t t, now_time;
    char stat_timestamp[24];
    float rx_ok_ratio;
    float rx_bad_ratio;
    float rx_nocrc_ratio;
    float up_ack_ratio;
    float dw_ack_ratio;

    output_status(0);  /* init the status of connection */

    /* display version informations */
    time(&now_time);
    strftime(stat_timestamp, sizeof(stat_timestamp), "%Y%m%d%H%M%S", gmtime(&now_time)); /* format yyyymmddThhmmssZ */
    MSG("Starting Packet Forwarder at %s\n", stat_timestamp);
    //MSG("*** Lora concentrator HAL library version info ***\n%s\n***\n", lgw_version_info());

    /* LOG or debug message configure */

    if (!get_config("general", debug_level_char, sizeof(debug_level_char)))
        debug_level_uint = 2;
    else 
        debug_level_uint = atoi(debug_level_char);

    switch (debug_level_uint) {
        case 0: /*only ERROR debug info */
            DEBUG_PKT_FWD    = 0;  
            DEBUG_REPORT     = 0;
            DEBUG_JIT        = 0;
            DEBUG_JIT_ERROR  = 0;
            DEBUG_TIMERSYNC  = 0;
            DEBUG_BEACON     = 0;
            DEBUG_INFO       = 0;
            DEBUG_WARNING    = 0;
            DEBUG_ERROR      = 1;
            break;
        case 1:  /* PKT_FWD MSG output */
            DEBUG_PKT_FWD    = 1;  
            DEBUG_REPORT     = 0;
            DEBUG_JIT        = 0;
            DEBUG_JIT_ERROR  = 0;
            DEBUG_TIMERSYNC  = 0;
            DEBUG_BEACON     = 0;
            DEBUG_INFO       = 0;
            DEBUG_WARNING    = 1;
            DEBUG_ERROR      = 1;
            break;
        case 2:  /* PKT_FWD MSG and MAC_HEAD output */
            DEBUG_PKT_FWD    = 1;  
            DEBUG_REPORT     = 1;
            DEBUG_JIT        = 1;
            DEBUG_JIT_ERROR  = 1;
            DEBUG_TIMERSYNC  = 0;
            DEBUG_BEACON     = 0;
            DEBUG_INFO       = 1;
            DEBUG_WARNING    = 1;
            DEBUG_ERROR      = 1;
            break;
        case 3:  /* PKT_FWD MSG and MAC_HEAD and JIT output */
            DEBUG_PKT_FWD    = 1;  
            DEBUG_REPORT     = 1;
            DEBUG_JIT        = 1;
            DEBUG_JIT_ERROR  = 1;
            DEBUG_TIMERSYNC  = 0;
            DEBUG_BEACON     = 0;
            DEBUG_INFO       = 0;
            DEBUG_WARNING    = 1;
            DEBUG_ERROR      = 1;
            break;
        case 4:  /* more verbose */
            DEBUG_PKT_FWD    = 1;  
            DEBUG_REPORT     = 1;
            DEBUG_JIT        = 1;
            DEBUG_JIT_ERROR  = 1;
            DEBUG_TIMERSYNC  = 0;
            DEBUG_BEACON     = 1;
            DEBUG_INFO       = 1;
            DEBUG_WARNING    = 1;
            DEBUG_ERROR      = 1;
            break;
        default: /* default is 2 level */
            break;
    }

    /* load configuration files */
    if (access(debug_cfg_path, R_OK) == 0) { /* if there is a debug conf, parse only the debug conf */
        MSG_DEBUG(DEBUG_INFO, "INFO~ found debug configuration file %s, parsing it\n", debug_cfg_path);
        MSG_DEBUG(DEBUG_INFO, "INFO~ other configuration files will be ignored\n");
        x = parse_SX1301_configuration(debug_cfg_path);
        if (x != 0) {
            exit(EXIT_FAILURE);
        }
        x = parse_gateway_configuration(debug_cfg_path);
        if (x != 0) {
            exit(EXIT_FAILURE);
        }
    } else if (access(global_cfg_path, R_OK) == 0) { /* if there is a global conf, parse it and then try to parse local conf  */
        MSG_DEBUG(DEBUG_INFO, "INFO~ found global configuration file %s, parsing it\n", global_cfg_path);
        x = parse_SX1301_configuration(global_cfg_path);
        if (x != 0) {
            exit(EXIT_FAILURE);
        }
        x = parse_gateway_configuration(global_cfg_path);
        if (x != 0) {
            exit(EXIT_FAILURE);
        }
        if (access(local_cfg_path, R_OK) == 0) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ found local configuration file %s, parsing it\n", local_cfg_path);
            MSG_DEBUG(DEBUG_INFO, "INFO~ redefined parameters will overwrite global parameters\n");
            parse_SX1301_configuration(local_cfg_path);
            parse_gateway_configuration(local_cfg_path);
        }
    } else if (access(local_cfg_path, R_OK) == 0) { /* if there is only a local conf, parse it and that's all */
        MSG_DEBUG(DEBUG_INFO, "INFO~ found local configuration file %s, parsing it\n", local_cfg_path);
        x = parse_SX1301_configuration(local_cfg_path);
        if (x != 0) {
            exit(EXIT_FAILURE);
        }
        x = parse_gateway_configuration(local_cfg_path);
        if (x != 0) {
            exit(EXIT_FAILURE);
        }
    } else {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] failed to find any configuration file named %s, %s OR %s\n", global_cfg_path, local_cfg_path, debug_cfg_path);
        exit(EXIT_FAILURE);
    }

    /* Start GPS a.s.a.p., to allow it to lock */
    if (gps_tty_path[0] != '\0') { /* do not try to open GPS device if no path set */
        i = lgw_gps_enable(gps_tty_path, "ubx7", 0, &gps_tty_fd); /* HAL only supports u-blox 7 for now */
        if (i != LGW_GPS_SUCCESS) {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ [main] impossible to open %s for GPS sync (check permissions)\n", gps_tty_path);
            gps_enabled = false;
            gps_ref_valid = false;
        } else {
            MSG_DEBUG(DEBUG_INFO, "INFO~ [main] TTY port %s open for GPS synchronization\n", gps_tty_path);
            gps_enabled = true;
            gps_ref_valid = false;
        }
    }

    /* get timezone info */
    tzset();

    /* process some of the configuration variables */
    net_mac_h = htonl((uint32_t)(0xFFFFFFFF & (lgwm>>32)));
    net_mac_l = htonl((uint32_t)(0xFFFFFFFF &  lgwm  ));

    /* init socket for communicate */
    if ((sock_up = init_socket(serv_addr, serv_port_up,
                    (void *)&push_timeout_half, sizeof(push_timeout_half))) == -1)
        exit(EXIT_FAILURE);

    if ((sock_down = init_socket(serv_addr, serv_port_down,
                    (void *)&pull_timeout, sizeof(pull_timeout))) == -1)
        exit(EXIT_FAILURE);
    
    /* Fport filter configure */

    if (!get_config("server1", fportnum, sizeof(fportnum)))
        fport_num = 0;
    else
        fport_num = atoi(fportnum);

    if (fport_num > 244 || fport_num < 0)  /* 0 - 244 */
        fport_num = 0;
	MSG_DEBUG(DEBUG_INFO, "INFO~ FPort Filter: %u\n", fport_num);
		
    /* DevAddr filter configure */
	//int tmp=0;
    if (!get_config("server1", devaddr_mask, sizeof(devaddr_mask)))
        dev_addr_mask = 0;
    else 
		dev_addr_mask = 1;
	MSG_DEBUG(DEBUG_INFO, "INFO~ DevAddrMask: 0x%s\n", devaddr_mask);

    /* sqlitedb, mac decrypto */
	if(!get_config("general", maccrypto, sizeof(maccrypto)))
		maccrypto_num = 0;
    else
        maccrypto_num = atoi(maccrypto);

    // set the dbpath for hardcode
    //if(!get_config("general", dbpath, sizeof(dbpath)))
    //  strcpy(dbpath, "/etc/lora/devskey");

	
	MSG_DEBUG(DEBUG_INFO, "INFO~ ABP Decryption: %s\n", maccrypto_num? "yes" : "no");

    // open sqlite3 context for something, such as pakeages report
    db_init(dbpath, &cntx);

	if (!get_config("general", gwcfg, sizeof(gwcfg)))
        strcpy(gwcfg, "EU");  /* default regional config */

    if (strstr(gwcfg, "EU") != NULL) {
        rx2dr = DR_LORA_SF12;
        rx2bw = BW_125KHZ;
        rx2freq = 869525000UL;
    } else if (strstr(gwcfg, "US") != NULL) {
        rx2dr = DR_LORA_SF12;
        rx2bw = BW_500KHZ;
        rx2freq = 923300000UL;
    } else if (strstr(gwcfg, "CN") != NULL) {
        rx2dr = DR_LORA_SF12;
        rx2bw = BW_125KHZ;
        rx2freq = 505300000UL;
    } else if (strstr(gwcfg, "CN780") != NULL) {
        rx2dr = DR_LORA_SF12;
        rx2bw = BW_125KHZ;
        rx2freq = 786000000UL;
    } else if (strstr(gwcfg, "AU") != NULL) {
        rx2dr = DR_LORA_SF12;
        rx2bw = BW_500KHZ;
        rx2freq = 923300000UL;
    } else if ((strstr(gwcfg, "AS1") != NULL) || (strstr(gwcfg, "AS2") != NULL)) {
        rx2dr = DR_LORA_SF10;
        rx2bw = BW_125KHZ;
        rx2freq = 923200000UL;
    } else if (strstr(gwcfg, "KR")) { 
        rx2dr = DR_LORA_SF12;
        rx2bw = BW_125KHZ;
        rx2freq = 921900000UL;
    } else if (strstr(gwcfg, "IN")) { 
        rx2dr = DR_LORA_SF10;
        rx2bw = BW_125KHZ;
        rx2freq = 866550000UL;
    } else if (strstr(gwcfg, "RU")) { 
        rx2dr = DR_LORA_SF12;
        rx2bw = BW_125KHZ;
        rx2freq = 869100000UL;
    } else { 
        rx2dr = DR_LORA_SF12;
        rx2bw = BW_125KHZ;
        rx2freq = 869525000UL;
    }

    /* init transifer radio device */
    /* spi-gpio-custom bus0=1,24,18,20,0,8000000,19 bus1=2,22,14,26,0,8000000,21 */
    char sx1276_tx[8] = "sx1276";
    char sx1276_txpw[8] = "sxtxpw";
    char model[8] = "model";

    get_config("general", sx1276_tx, sizeof(sx1276_tx));
    get_config("general", sx1276_txpw, sizeof(sx1276_txpw));
    get_config("general", model, sizeof(model));

    /* mqtt or lorawan */
    get_config("general", server_type, sizeof(server_type));

    /* sqlitedb, mac decrypto */


    MSG_DEBUG(DEBUG_INFO, "INFO~ sx1276:%d, sxtxpw:%d, model:%s, server_type:%s\n", atoi(sx1276_tx), atoi(sx1276_txpw), model, server_type);

    /* only LG08P with sx1276 */

    if (!strcmp(model, "LG08P") && atoi(sx1276_tx) > 0) {

        sxradio = (radiodev *) malloc(sizeof(radiodev));
        sxradio->nss = 21;
        sxradio->rst = 12;
        sxradio->dio[0] = 7;
        sxradio->dio[1] = 6;
        sxradio->dio[2] = 8;
        sxradio->rf_power = (atoi(sx1276_txpw) > 0 && atoi(sx1276_txpw) <= 20) ? atoi(sx1276_txpw) : 0; /* sx1276 power < 20 */
        strcpy(sxradio->desc, "SPI_DEV_RADIO");
        sxradio->spiport = spi_open(SPI_DEV_RADIO);

        if(get_radio_version(sxradio))
            sx1276 = true;
        else
            free(sxradio);
    }

    /* init semaphore */
    i = sem_init(&rxpkt_rec_sem, 0, 0);
    if (i != 0)
        MSG_DEBUG(DEBUG_WARNING, "[sem]Semaphore initialization failed!\n");

    /* Board reset */          
    if (system("/usr/bin/reset_lgw.sh start") != 0) {
        printf("ERROR~ failed to reset SX1301, check your reset_lgw.sh script\n");
        exit(EXIT_FAILURE);
    }
        
    /* starting the concentrator */
    i = lgw_start();
    if (i == LGW_HAL_SUCCESS) {
        MSG_DEBUG(DEBUG_INFO, "INFO~ [main] concentrator started, packet can now be received\n");
    } else {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] failed to start the concentrator\n");
        lgw_exit_fail();
    }

    /* spawn threads to manage upstream and downstream */
    i = pthread_create( &thrid_up, NULL, (void * (*)(void *))thread_up, NULL);
    if (i != 0) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] impossible to create upstream thread\n");
        lgw_exit_fail();
    }
    i = pthread_create( &thrid_down, NULL, (void * (*)(void *))thread_down, NULL);
    if (i != 0) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] impossible to create downstream thread\n");
        lgw_exit_fail();
    }
    i = pthread_create( &thrid_jit, NULL, (void * (*)(void *))thread_jit, NULL);
    if (i != 0) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] impossible to create JIT thread\n");
        lgw_exit_fail();
    }
    i = pthread_create( &thrid_timersync, NULL, (void * (*)(void *))thread_timersync, NULL);
    if (i != 0) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] impossible to create Timer Sync thread\n");
        lgw_exit_fail();
    }
    i = pthread_create( &thrid_proc_rxpkt, NULL, (void * (*)(void *))thread_proc_rxpkt, NULL);
    if (i != 0) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] impossible to create proc_rxpkt thread\n");
        lgw_exit_fail();
    }
    i = pthread_create( &thrid_ent_dnlink, NULL, (void * (*)(void *))thread_ent_dnlink, NULL);
    if (i != 0) {
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] impossible to create ent_dnlink thread\n");
        lgw_exit_fail();
    }

    /* spawn thread to manage GPS */
    if (gps_enabled == true) {
        i = pthread_create( &thrid_gps, NULL, (void * (*)(void *))thread_gps, NULL);
        if (i != 0) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] impossible to create GPS thread\n");
            lgw_exit_fail();
        }
        i = pthread_create( &thrid_valid, NULL, (void * (*)(void *))thread_valid, NULL);
        if (i != 0) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] impossible to create validation thread\n");
            lgw_exit_fail();
        }
    }

    if (lbt_is_enabled() == true) {
        i = pthread_create( &thrid_lbt, NULL, (void * (*)(void *))lbt_run_rssi_scan, (void*)&exit_sig);
        if (i != 0) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ [main] impossible to create LBT thread\n");
            lgw_exit_fail();
        }
    }

    /* configure signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL); /* Ctrl-\ */
    sigaction(SIGINT, &sigact, NULL); /* Ctrl-C */
    sigaction(SIGTERM, &sigact, NULL); /* default "kill" command */

    /* signal for reconnect */
    sigemptyset(&sigusr.sa_mask);
    sigusr.sa_flags = 0;
    sigusr.sa_handler = sigusr_handler;
    sigaction(SIGUSR1, &sigusr, NULL); /* custom signal */

    /* main loop task : statistics collection */
    
    uint32_t stat_send = 0, push_ack = 0;

    /* ping measurement variables */
    struct timespec send_time;
    struct timespec recv_time;

    /* stat buffers */
    uint8_t buff_stat[STAT_BUFF_SIZE]; /* buffer to compose the upstream packet */
    int buff_index;
    uint8_t stat_ack[32]; /* buffer to receive acknowledges */

    /* protocol variables */
    uint8_t token_h; /* random token for acknowledgement matching */
    uint8_t token_l; /* random token for acknowledgement matching */

    buff_stat[0] = PROTOCOL_VERSION;
    buff_stat[3] = PKT_PUSH_DATA;
    *(uint32_t *)(buff_stat + 4) = net_mac_h;
    *(uint32_t *)(buff_stat + 8) = net_mac_l;

    while (!exit_sig && !quit_sig) {


        /* access upstream statistics, copy and reset them */
        pthread_mutex_lock(&mx_meas_up);
        cp_nb_rx_rcv       = meas_nb_rx_rcv;
        cp_nb_rx_ok        = meas_nb_rx_ok;
        cp_nb_rx_bad       = meas_nb_rx_bad;
        cp_nb_rx_nocrc     = meas_nb_rx_nocrc;
        cp_up_pkt_fwd      = meas_up_pkt_fwd;
        cp_up_network_byte = meas_up_network_byte;
        cp_up_payload_byte = meas_up_payload_byte;
        cp_up_dgram_sent   = meas_up_dgram_sent;
        cp_up_ack_rcv      = meas_up_ack_rcv;
        meas_nb_rx_rcv = 0;
        meas_nb_rx_ok = 0;
        meas_nb_rx_bad = 0;
        meas_nb_rx_nocrc = 0;
        meas_up_pkt_fwd = 0;
        meas_up_network_byte = 0;
        meas_up_payload_byte = 0;
        meas_up_dgram_sent = 0;
        meas_up_ack_rcv = 0;
        pthread_mutex_unlock(&mx_meas_up);
        if (cp_nb_rx_rcv > 0) {
            rx_ok_ratio = (float)cp_nb_rx_ok / (float)cp_nb_rx_rcv;
            rx_bad_ratio = (float)cp_nb_rx_bad / (float)cp_nb_rx_rcv;
            rx_nocrc_ratio = (float)cp_nb_rx_nocrc / (float)cp_nb_rx_rcv;
        } else {
            rx_ok_ratio = 0.0;
            rx_bad_ratio = 0.0;
            rx_nocrc_ratio = 0.0;
        }
        if (cp_up_dgram_sent > 0) {
            up_ack_ratio = (float)cp_up_ack_rcv / (float)cp_up_dgram_sent;
        } else {
            up_ack_ratio = 0.0;
        }

        /* access downstream statistics, copy and reset them */
        pthread_mutex_lock(&mx_meas_dw);
        cp_dw_pull_sent    =  meas_dw_pull_sent;
        cp_dw_ack_rcv      =  meas_dw_ack_rcv;
        cp_dw_dgram_rcv    =  meas_dw_dgram_rcv;
        cp_dw_network_byte =  meas_dw_network_byte;
        cp_dw_payload_byte =  meas_dw_payload_byte;
        cp_nb_tx_ok        =  meas_nb_tx_ok;
        cp_nb_tx_fail      =  meas_nb_tx_fail;
        cp_nb_tx_requested                 +=  meas_nb_tx_requested;
        cp_nb_tx_rejected_collision_packet +=  meas_nb_tx_rejected_collision_packet;
        cp_nb_tx_rejected_collision_beacon +=  meas_nb_tx_rejected_collision_beacon;
        cp_nb_tx_rejected_too_late         +=  meas_nb_tx_rejected_too_late;
        cp_nb_tx_rejected_too_early        +=  meas_nb_tx_rejected_too_early;
        cp_nb_beacon_queued   +=  meas_nb_beacon_queued;
        cp_nb_beacon_sent     +=  meas_nb_beacon_sent;
        cp_nb_beacon_rejected +=  meas_nb_beacon_rejected;
        meas_dw_pull_sent = 0;
        meas_dw_ack_rcv = 0;
        meas_dw_dgram_rcv = 0;
        meas_dw_network_byte = 0;
        meas_dw_payload_byte = 0;
        meas_nb_tx_ok = 0;
        meas_nb_tx_fail = 0;
        meas_nb_tx_requested = 0;
        meas_nb_tx_rejected_collision_packet = 0;
        meas_nb_tx_rejected_collision_beacon = 0;
        meas_nb_tx_rejected_too_late = 0;
        meas_nb_tx_rejected_too_early = 0;
        meas_nb_beacon_queued = 0;
        meas_nb_beacon_sent = 0;
        meas_nb_beacon_rejected = 0;
        pthread_mutex_unlock(&mx_meas_dw);
        if (cp_dw_pull_sent > 0) {
            dw_ack_ratio = (float)cp_dw_ack_rcv / (float)cp_dw_pull_sent;
        } else {
            dw_ack_ratio = 0.0;
        }

        /* access GPS statistics, copy them */
        if (gps_enabled == true) {
            pthread_mutex_lock(&mx_meas_gps);
            coord_ok = gps_coord_valid;
            cp_gps_coord = meas_gps_coord;
            pthread_mutex_unlock(&mx_meas_gps);
        }

        /* overwrite with reference coordinates if function is enabled */
        if (gps_fake_enable == true) {
            cp_gps_coord = reference_coord;
        }

        /* get timestamp for statistics */
        t = time(NULL);
        strftime(stat_timestamp, sizeof stat_timestamp, "%F %T %Z", gmtime(&t));

        /* display a report */
        MSG_DEBUG(DEBUG_REPORT, "\nREPORT~ ################## Report at: %s ##################\n", stat_timestamp);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ ### [UPSTREAM] ###\n");
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # RF packets received by concentrator: %u\n", cp_nb_rx_rcv);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # CRC_OK: %.2f%%, CRC_FAIL: %.2f%%, NO_CRC: %.2f%%\n", 100.0 * rx_ok_ratio, 100.0 * rx_bad_ratio, 100.0 * rx_nocrc_ratio);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # RF packets forwarded: %u (%u bytes)\n", cp_up_pkt_fwd, cp_up_payload_byte);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # PUSH_DATA datagrams sent: %u (%u bytes)\n", cp_up_dgram_sent, cp_up_network_byte);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # PUSH_DATA acknowledged: %.2f%%\n", 100.0 * up_ack_ratio);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ ### [DOWNSTREAM] ###\n");
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # PULL_DATA sent: %u (%.2f%% acknowledged)\n", cp_dw_pull_sent, 100.0 * dw_ack_ratio);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # PULL_RESP(onse) datagrams received: %u (%u bytes)\n", cp_dw_dgram_rcv, cp_dw_network_byte);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # RF packets sent to concentrator: %u (%u bytes)\n", (cp_nb_tx_ok+cp_nb_tx_fail), cp_dw_payload_byte);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # TX errors: %u\n", cp_nb_tx_fail);
        if (cp_nb_tx_requested != 0 ) {
            MSG_DEBUG(DEBUG_REPORT, "REPORT~ # TX rejected (collision packet): %.2f%% (req:%u, rej:%u)\n", 100.0 * cp_nb_tx_rejected_collision_packet / cp_nb_tx_requested, cp_nb_tx_requested, cp_nb_tx_rejected_collision_packet);
            MSG_DEBUG(DEBUG_REPORT, "REPORT~ # TX rejected (collision beacon): %.2f%% (req:%u, rej:%u)\n", 100.0 * cp_nb_tx_rejected_collision_beacon / cp_nb_tx_requested, cp_nb_tx_requested, cp_nb_tx_rejected_collision_beacon);
            MSG_DEBUG(DEBUG_REPORT, "REPORT~ # TX rejected (too late): %.2f%% (req:%u, rej:%u)\n", 100.0 * cp_nb_tx_rejected_too_late / cp_nb_tx_requested, cp_nb_tx_requested, cp_nb_tx_rejected_too_late);
            MSG_DEBUG(DEBUG_REPORT, "REPORT~ # TX rejected (too early): %.2f%% (req:%u, rej:%u)\n", 100.0 * cp_nb_tx_rejected_too_early / cp_nb_tx_requested, cp_nb_tx_requested, cp_nb_tx_rejected_too_early);
        }
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # BEACON queued: %u\n", cp_nb_beacon_queued);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # BEACON sent so far: %u\n", cp_nb_beacon_sent);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ # BEACON rejected: %u\n", cp_nb_beacon_rejected);
        MSG_DEBUG(DEBUG_REPORT, "REPORT~ ### [PPS] ###\n");
        
        /* get timestamp captured on PPM pulse  */
        pthread_mutex_lock(&mx_concent);
        i = lgw_get_trigcnt(&trig_tstamp);
        pthread_mutex_unlock(&mx_concent);
        if (i != LGW_HAL_SUCCESS) {
            MSG_DEBUG(DEBUG_REPORT, "REPORT~ # SX1301 time (PPS): unknown\n");
        } else {
            MSG_DEBUG(DEBUG_REPORT, "REPORT~ # SX1301 time (PPS): %u\n", trig_tstamp);
        }
        jit_print_queue(&jit_queue, false, DEBUG_INFO);
        MSG_DEBUG(DEBUG_INFO, "INFO~ ### [GPS] ###\n");
        if (gps_enabled == true) {
            /* no need for mutex, display is not critical */
            if (gps_ref_valid == true) {
                MSG_DEBUG(DEBUG_INFO, "INFO~ # Valid time reference (age: %li sec)\n", (long)difftime(time(NULL), time_reference_gps.systime));
            } else {
                MSG_DEBUG(DEBUG_INFO, "INFO~ # Invalid time reference (age: %li sec)\n", (long)difftime(time(NULL), time_reference_gps.systime));
            }
            if (coord_ok == true) {
                MSG_DEBUG(DEBUG_INFO, "INFO~ # GPS coordinates: latitude %.6f, longitude %.6f, altitude %i m\n", cp_gps_coord.lat, cp_gps_coord.lon, cp_gps_coord.alt);
            } else {
                MSG_DEBUG(DEBUG_INFO, "INFO~ # no valid GPS coordinates available yet\n");
            }
        } else if (gps_fake_enable == true) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ # GPS *FAKE* coordinates: latitude %.6f, longitude %.6f, altitude %i m\n", cp_gps_coord.lat, cp_gps_coord.lon, cp_gps_coord.alt);
        } else {
            MSG_DEBUG(DEBUG_INFO, "INFO~ # GPS sync is disabled\n");
        }
        MSG_DEBUG(DEBUG_INFO, "INFO~ ##### END #####\n");

        /* start composing datagram with the header */
        token_h = (uint8_t)rand(); /* random token */
        token_l = (uint8_t)rand(); /* random token */
        buff_stat[1] = token_h;
        buff_stat[2] = token_l;
        buff_index = 12; /* 12-byte header */

        /* generate a JSON report (will be sent to server by upstream thread) */

        if (((gps_enabled == true) && (coord_ok == true)) || (gps_fake_enable == true)) {
            j = snprintf((char *)(buff_stat + buff_index), STAT_BUFF_SIZE - buff_index, "{\"stat\":{\"time\":\"%s\",\"lati\":%.6f,\"long\":%.6f,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u}", stat_timestamp, cp_gps_coord.lat, cp_gps_coord.lon, cp_gps_coord.alt, cp_nb_rx_rcv, cp_nb_rx_ok, cp_up_pkt_fwd, 100.0 * up_ack_ratio, cp_dw_dgram_rcv, cp_nb_tx_ok);
        } else {
            j = snprintf((char *)(buff_stat + buff_index), STAT_BUFF_SIZE - buff_index,  "{\"stat\":{\"time\":\"%s\",\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u}", stat_timestamp, cp_nb_rx_rcv, cp_nb_rx_ok, cp_up_pkt_fwd, 100.0 * up_ack_ratio, cp_dw_dgram_rcv, cp_nb_tx_ok);
        }

        buff_index += j;

        buff_stat[buff_index] = '}';
        ++buff_index;
        buff_stat[buff_index] = 0; /* add string terminator, for safety */
        MSG_DEBUG(DEBUG_PKT_FWD, "RXTX~ %s\n", (char *)(buff_stat + 12)); /* DEBUG: display JSON payload */

        /* send datagram to server */
        send(sock_up, (void *)buff_stat, buff_index, 0);

        stat_send++;

        clock_gettime(CLOCK_MONOTONIC, &send_time);

        recv_time = send_time;

        for (i=0; i<2; ++i) {
            j = recv(sock_up, (void *)stat_ack, sizeof stat_ack, 0);
            clock_gettime(CLOCK_MONOTONIC, &recv_time);
            if (j == -1) {
                /* server connection error */
                continue;

            } else if ((j < 4) || (stat_ack[0] != PROTOCOL_VERSION) || (stat_ack[3] != PKT_PUSH_ACK)) {
                MSG_DEBUG(DEBUG_INFO, "INFO~ [up] ignored invalid non-ACL packet\n");
                continue;
            /*
            } else if ((stat_ack[1] != token_h) || (stat_ack[2] != token_l)) {
                //MSG_DEBUG(DEBUG_INFO, "INFO~ [up] ignored out-of sync ACK packet\n");
                continue;
            */
            } else {
                MSG_DEBUG(DEBUG_INFO, "INFO~ [up] PUSH_ACK received in %i ms\n", (int)(1000 * difftimespec(recv_time, send_time)));
                push_ack++;
                output_status(1);
                break;
            }
        }

        if (push_ack < labs(stat_send * 0.3)) {  /*maybe not recv push_ack, 90% */
            push_ack = 0;
            stat_send = 0;
            MSG_DEBUG(DEBUG_INFO, "INFO~ [up] PUSH_ACK mismatch, reconnect server \n");
            output_status(0);
            /* maybe theadup is sending message */
            pthread_mutex_lock(&mx_sockup); /* if a lock ? */
            if (sock_up) close(sock_up);
            if ((sock_up = init_socket(serv_addr, serv_port_up,
                                      (void *)&push_timeout_half, sizeof(push_timeout_half))) == -1)
                exit(EXIT_FAILURE);
            pthread_mutex_unlock(&mx_sockup);
        }

        /* wait for next reporting interval */
        //wait_ms(1000 * (stat_interval - keepalive_time));
        wait_ms(1000 * stat_interval); /*may be keepalive_time big than stat_interval*/
    }

    output_status(0);  /* exist, reset the status */

    /* wait for upstream thread to finish (1 fetch cycle max) */
    pthread_join(thrid_up, NULL);
    pthread_cancel(thrid_down); /* don't wait for downstream thread */
    pthread_cancel(thrid_jit); /* don't wait for jit thread */
    pthread_cancel(thrid_timersync); /* don't wait for timer sync thread */
    if (gps_enabled == true) {
        pthread_cancel(thrid_gps); /* don't wait for GPS thread */
        pthread_cancel(thrid_valid); /* don't wait for validation thread */

        i = lgw_gps_disable(gps_tty_fd);
        if (i == LGW_HAL_SUCCESS) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ GPS closed successfully\n");
        } else {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ failed to close GPS successfully\n");
        }
    }
    if (lbt_is_enabled() == true) 
        pthread_join(thrid_lbt, NULL); /* wait for lbt thread */

    /* if an exit signal was received, try to quit properly */
    if (exit_sig) {
        /* shut down network sockets */
        shutdown(sock_up, SHUT_RDWR);
        shutdown(sock_down, SHUT_RDWR);
        /* stop the hardware */
        i = lgw_stop();
        if (i == LGW_HAL_SUCCESS) {
            MSG_DEBUG(DEBUG_INFO, "INFO~ concentrator stopped successfully\n");
        } else {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ failed to stop concentrator successfully\n");
        }
    }
	
	 /* Board reset */          
    lgw_exit_fail();

    db_destroy(&cntx);

    MSG_DEBUG(DEBUG_INFO, "INFO~ Exiting packet forwarder program\n");
    if (sxradio)
        free(sxradio);
    exit(EXIT_SUCCESS);
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 1: RECEIVING PACKETS AND FORWARDING THEM ---------------------- */

void thread_up(void) {
    int i, j; /* loop variables */
    unsigned pkt_in_dgram; /* nb on Lora packet in the current datagram */

    /* allocate memory for packet fetching and processing */
    struct lgw_pkt_rx_s rxpkt[NB_PKT_MAX]; /* array containing inbound packets + metadata */
    struct lgw_pkt_rx_s *p; /* pointer on a RX packet */

    LoRaMacMessageData_t macmsg;

    int nb_pkt;

    /* local copy of GPS time reference */
    bool ref_ok = false; /* determine if GPS time reference must be used or not */
    struct tref local_ref; /* time reference used for UTC <-> timestamp conversion */

    /* data buffers */
    uint8_t buff_up[TX_BUFF_SIZE]; /* buffer to compose the upstream packet */
    int buff_index;
    uint8_t buff_ack[32]; /* buffer to receive acknowledges */

    /* local timekeeping variables */
    struct timespec send_time; /* time of the pull request */
    struct timespec recv_time; /* time of return from recv socket call */

    /* protocol variables */
    uint8_t token_h; /* random token for acknowledgement matching */
    uint8_t token_l; /* random token for acknowledgement matching */

    /* GPS synchronization variables */
    struct timespec pkt_utc_time;
    struct tm * x; /* broken-up UTC time */
    struct timespec pkt_gps_time;
    uint64_t pkt_gps_time_ms;

    /* pre-fill the data buffer with fixed fields */
    buff_up[0] = PROTOCOL_VERSION;
    buff_up[3] = PKT_PUSH_DATA;
    *(uint32_t *)(buff_up + 4) = net_mac_h;
    *(uint32_t *)(buff_up + 8) = net_mac_l;

    char devchar[16] = {'\0'};

    while (!exit_sig && !quit_sig) {

        /* fetch packets */
        pthread_mutex_lock(&mx_concent);
        nb_pkt = lgw_receive(NB_PKT_MAX, rxpkt);
        pthread_mutex_unlock(&mx_concent);
        if (nb_pkt == LGW_HAL_ERROR) {
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] failed packet fetch, exiting\n");
            exit(EXIT_FAILURE);
        }

        /* wait a short time if no packets, nor status report */
        if (nb_pkt == 0) {
            wait_ms(FETCH_SLEEP_MS);
            continue;
        }

        /* get a copy of GPS time reference (avoid 1 mutex per packet) */
        if ((nb_pkt > 0) && (gps_enabled == true)) {
            pthread_mutex_lock(&mx_timeref);
            ref_ok = gps_ref_valid;
            local_ref = time_reference_gps;
            pthread_mutex_unlock(&mx_timeref);
        } else {
            ref_ok = false;
        }

        /* start a thread to process rxpkt, such as decode */

        PKTS *tmp, *last;
        tmp = (PKTS *) malloc(sizeof(PKTS));
        tmp->nb_pkt = nb_pkt;
        tmp->next = NULL;
        memcpy(tmp->rxpkt, rxpkt, sizeof(struct lgw_pkt_rx_s) * nb_pkt); /* How many pkts ?*/
        pthread_mutex_lock(&mx_rxpkts_link);
        last = rxpkts_link;      /* insert rxpkt to queue, QUEUE 1: rxpkt */
        if (last == NULL)
            rxpkts_link = tmp;
        else {
            while(last->next != NULL)
                last = last->next;
            last->next = tmp;
        }
        pthread_mutex_unlock(&mx_rxpkts_link);

        sem_post(&rxpkt_rec_sem);

        /* start composing datagram with the header */
        token_h = (uint8_t)rand(); /* random token */
        token_l = (uint8_t)rand(); /* random token */
        buff_up[1] = token_h;
        buff_up[2] = token_l;
        buff_index = 12; /* 12-byte header */

        /* start of JSON structure */
        memcpy((void *)(buff_up + buff_index), (void *)"{\"rxpk\":[", 9);
        buff_index += 9;

        /* serialize Lora packets metadata and payload */
        pkt_in_dgram = 0;
        for (i=0; i < nb_pkt; ++i) {
            p = &rxpkt[i];

            macmsg.Buffer = p->payload;
            macmsg.BufSize = p->size;
            if (LORAMAC_PARSER_SUCCESS != LoRaMacParserData(&macmsg))  
                continue;

            if ((macmsg.MHDR.Bits.MType == FRAME_TYPE_DATA_UNCONFIRMED_UP) || (macmsg.MHDR.Bits.MType == FRAME_TYPE_DATA_CONFIRMED_UP)) {
                sprintf(devchar, "%08X", macmsg.FHDR.DevAddr);

                /* basic packet filtering */
                if (fport_num != 0 && !(macmsg.FPort == fport_num)){
                    MSG_DEBUG(DEBUG_PKT_FWD, "RXTX~ Drop due to Fport doesn't match fport filter:%u, message Fport: %u\n", fport_num,macmsg.FPort); /* DEBUG: display JSON payload */
                    continue;
                } /* filter */
                 
                if (dev_addr_mask != 0 && strncmp(devaddr_mask, "0", 1) && (strncmp(devchar, devaddr_mask, strlen(devaddr_mask)) != 0 )){
                    MSG_DEBUG(DEBUG_PKT_FWD, "RXTX~ Drop due to DevAddr(0x%s) doesn't match mask (0x%s)\n", 
                           devchar,devaddr_mask); /* DEBUG: display JSON payload */
                    continue;
                }
            }

            pthread_mutex_lock(&mx_meas_up);
            meas_nb_rx_rcv += 1;
            total_pkt_up++;
            switch(p->status) {
                case STAT_CRC_OK:
                    meas_nb_rx_ok += 1;
                    if (!fwd_valid_pkt) {
                        pthread_mutex_unlock(&mx_meas_up);
                        continue; /* skip that packet */
                    }
                    break;
                case STAT_CRC_BAD:
                    meas_nb_rx_bad += 1;
                    if (!fwd_error_pkt) {
                        pthread_mutex_unlock(&mx_meas_up);
                        continue; /* skip that packet */
                    }
                    break;
                case STAT_NO_CRC:
                    meas_nb_rx_nocrc += 1;
                    if (!fwd_nocrc_pkt) {
                        pthread_mutex_unlock(&mx_meas_up);
                        continue; /* skip that packet */
                    }
                    break;
                default:
                    MSG_DEBUG(DEBUG_WARNING, "WARNING~ [up] received packet with unknown status %u (size %u, modulation %u, BW %u, DR %u, RSSI %.1f)\n", p->status, p->size, p->modulation, p->bandwidth, p->datarate, p->rssi);
                    pthread_mutex_unlock(&mx_meas_up);
                    continue; /* skip that packet */
                    // exit(EXIT_FAILURE);
            }
            meas_up_pkt_fwd++;
            meas_up_payload_byte += p->size;
            pthread_mutex_unlock(&mx_meas_up);

            db_incpkt(cntx.totalup_stmt, total_pkt_up);

            /* Start of packet, add inter-packet separator if necessary */
            if (pkt_in_dgram == 0) {
                buff_up[buff_index] = '{';
                ++buff_index;
            } else {
                buff_up[buff_index] = ',';
                buff_up[buff_index+1] = '{';
                buff_index += 2;
            }

            /* RAW timestamp, 8-17 useful chars */
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, "\"tmst\":%u", p->count_us);
            if (j > 0) {
                buff_index += j;
            } else {
                MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] snprintf failed line %u\n", (__LINE__ - 4));
                exit(EXIT_FAILURE);
            }

            /* Packet RX time (GPS based), 37 useful chars */
            if (ref_ok == true) {
                /* convert packet timestamp to UTC absolute time */
                j = lgw_cnt2utc(local_ref, p->count_us, &pkt_utc_time);
                if (j == LGW_GPS_SUCCESS) {
                    /* split the UNIX timestamp to its calendar components */
                    x = gmtime(&(pkt_utc_time.tv_sec));
                    j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"time\":\"%04i-%02i-%02iT%02i:%02i:%02i.%06liZ\"", (x->tm_year)+1900, (x->tm_mon)+1, x->tm_mday, x->tm_hour, x->tm_min, x->tm_sec, (pkt_utc_time.tv_nsec)/1000); /* ISO 8601 format */
                    if (j > 0) {
                        buff_index += j;
                    } else {
                        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] snprintf failed line %u\n", (__LINE__ - 4));
                        exit(EXIT_FAILURE);
                    }
                }
                /* convert packet timestamp to GPS absolute time */
                j = lgw_cnt2gps(local_ref, p->count_us, &pkt_gps_time);
                if (j == LGW_GPS_SUCCESS) {
                    pkt_gps_time_ms = pkt_gps_time.tv_sec * 1E3 + pkt_gps_time.tv_nsec / 1E6;
                    j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"tmms\":%llu",
                                    pkt_gps_time_ms); /* GPS time in milliseconds since 06.Jan.1980 */
                    if (j > 0) {
                        buff_index += j;
                    } else {
                        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] snprintf failed line %u\n", (__LINE__ - 4));
                        exit(EXIT_FAILURE);
                    }
                }
            } else {
                clock_gettime(CLOCK_REALTIME, &pkt_utc_time);
                x = gmtime(&(pkt_utc_time.tv_sec)); /* split the UNIX timestamp to its calendar components */
                j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"time\":\"%04i-%02i-%02iT%02i:%02i:%02i.%06liZ\"", (x->tm_year)+1900, (x->tm_mon)+1, x->tm_mday, x->tm_hour, x->tm_min, x->tm_sec, (pkt_utc_time.tv_nsec)/1000); /* ISO 8601 format */
                if (j > 0) {
                    buff_index += j;
                } else {
                    MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] snprintf failed line %u\n", (__LINE__ - 4));
                    exit(EXIT_FAILURE);
                }
            }

            /* Packet concentrator channel, RF chain & RX frequency, 34-36 useful chars */
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"chan\":%1u,\"rfch\":%1u,\"freq\":%.6lf", p->if_chain, p->rf_chain, ((double)p->freq_hz / 1e6));
            if (j > 0) {
                buff_index += j;
            } else {
                MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] snprintf failed line %u\n", (__LINE__ - 4));
                exit(EXIT_FAILURE);
            }

            /* Packet status, 9-10 useful chars */
            switch (p->status) {
                case STAT_CRC_OK:
                    memcpy((void *)(buff_up + buff_index), (void *)",\"stat\":1", 9);
                    buff_index += 9;
                    break;
                case STAT_CRC_BAD:
                    memcpy((void *)(buff_up + buff_index), (void *)",\"stat\":-1", 10);
                    buff_index += 10;
                    break;
                case STAT_NO_CRC:
                    memcpy((void *)(buff_up + buff_index), (void *)",\"stat\":0", 9);
                    buff_index += 9;
                    break;
                default:
                    MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] received packet with unknown status\n");
                    memcpy((void *)(buff_up + buff_index), (void *)",\"stat\":?", 9);
                    buff_index += 9;
                    exit(EXIT_FAILURE);
            }

            /* Packet modulation, 13-14 useful chars */
            if (p->modulation == MOD_LORA) {
                memcpy((void *)(buff_up + buff_index), (void *)",\"modu\":\"LORA\"", 14);
                buff_index += 14;

                /* Lora datarate & bandwidth, 16-19 useful chars */
                switch (p->datarate) {
                    case DR_LORA_SF7:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF7", 12);
                        buff_index += 12;
                        break;
                    case DR_LORA_SF8:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF8", 12);
                        buff_index += 12;
                        break;
                    case DR_LORA_SF9:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF9", 12);
                        buff_index += 12;
                        break;
                    case DR_LORA_SF10:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF10", 13);
                        buff_index += 13;
                        break;
                    case DR_LORA_SF11:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF11", 13);
                        buff_index += 13;
                        break;
                    case DR_LORA_SF12:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF12", 13);
                        buff_index += 13;
                        break;
                    default:
                        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] lora packet with unknown datarate\n");
                        memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF?", 12);
                        buff_index += 12;
                        exit(EXIT_FAILURE);
                }
                switch (p->bandwidth) {
                    case BW_125KHZ:
                        memcpy((void *)(buff_up + buff_index), (void *)"BW125\"", 6);
                        buff_index += 6;
                        break;
                    case BW_250KHZ:
                        memcpy((void *)(buff_up + buff_index), (void *)"BW250\"", 6);
                        buff_index += 6;
                        break;
                    case BW_500KHZ:
                        memcpy((void *)(buff_up + buff_index), (void *)"BW500\"", 6);
                        buff_index += 6;
                        break;
                    default:
                        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] lora packet with unknown bandwidth\n");
                        memcpy((void *)(buff_up + buff_index), (void *)"BW?\"", 4);
                        buff_index += 4;
                        exit(EXIT_FAILURE);
                }

                /* Packet ECC coding rate, 11-13 useful chars */
                switch (p->coderate) {
                    case CR_LORA_4_5:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"4/5\"", 13);
                        buff_index += 13;
                        break;
                    case CR_LORA_4_6:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"4/6\"", 13);
                        buff_index += 13;
                        break;
                    case CR_LORA_4_7:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"4/7\"", 13);
                        buff_index += 13;
                        break;
                    case CR_LORA_4_8:
                        memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"4/8\"", 13);
                        buff_index += 13;
                        break;
                    case 0: /* treat the CR0 case (mostly false sync) */
                        memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"OFF\"", 13);
                        buff_index += 13;
                        break;
                    default:
                        MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] lora packet with unknown coderate\n");
                        memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"?\"", 11);
                        buff_index += 11;
                        exit(EXIT_FAILURE);
                }

                /* Lora SNR, 11-13 useful chars */
                j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"lsnr\":%.1f", p->snr);
                if (j > 0) {
                    buff_index += j;
                } else {
                    MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] snprintf failed line %u\n", (__LINE__ - 4));
                    exit(EXIT_FAILURE);
                }
            } else if (p->modulation == MOD_FSK) {
                memcpy((void *)(buff_up + buff_index), (void *)",\"modu\":\"FSK\"", 13);
                buff_index += 13;

                /* FSK datarate, 11-14 useful chars */
                j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"datr\":%u", p->datarate);
                if (j > 0) {
                    buff_index += j;
                } else {
                    MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] snprintf failed line %u\n", (__LINE__ - 4));
                    exit(EXIT_FAILURE);
                }
            } else {
                MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] received packet with unknown modulation\n");
                exit(EXIT_FAILURE);
            }

            /* Packet RSSI, payload size, 18-23 useful chars */
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"rssi\":%.0f,\"size\":%u", p->rssi, p->size);
            if (j > 0) {
                buff_index += j;
            } else {
                MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] snprintf failed line %u\n", (__LINE__ - 4));
                exit(EXIT_FAILURE);
            }

            /* Packet base64-encoded payload, 14-350 useful chars */
            memcpy((void *)(buff_up + buff_index), (void *)",\"data\":\"", 9);
            buff_index += 9;
            j = bin_to_b64(p->payload, p->size, (char *)(buff_up + buff_index), 341); /* 255 bytes = 340 chars in b64 + null char */
            if (j>=0) {
                buff_index += j;
            } else {
                MSG_DEBUG(DEBUG_ERROR, "ERROR~ [up] bin_to_b64 failed line %u\n", (__LINE__ - 5));
                exit(EXIT_FAILURE);
            }
            buff_up[buff_index] = '"';
            ++buff_index;

            /* End of packet serialization */
            buff_up[buff_index] = '}';
            ++buff_index;
            ++pkt_in_dgram;
        }

        /* restart fetch sequence without sending empty JSON if all packets have been filtered out */
        if (pkt_in_dgram == 0) {
            /* all packet have been filtered out and no report, restart loop */
            continue;
        } else {
            /* end of packet array */
            buff_up[buff_index] = ']';
            ++buff_index;
        }
        /* end of JSON datagram payload */
        buff_up[buff_index] = '}';
        ++buff_index;
        buff_up[buff_index] = 0; /* add string terminator, for safety */
        MSG_DEBUG(DEBUG_PKT_FWD, "RXTX~ %s\n", (char *)(buff_up + 12)); /* DEBUG: display JSON payload */

        pthread_mutex_lock(&mx_sockup); /*maybe reconnect, so lock */ 
        send(sock_up, (void *)buff_up, buff_index, 0);
        pthread_mutex_unlock(&mx_sockup);

        pthread_mutex_lock(&mx_meas_up);
        meas_up_dgram_sent += 1;
        meas_up_network_byte += buff_index;
        pthread_mutex_unlock(&mx_meas_up);

        /* wait for acknowledge (in 2 times, to catch extra packets) */
        /* no need acknowledge in data_up, because can't receive an ack in a little time*/
        clock_gettime(CLOCK_MONOTONIC, &send_time);
        for (i=0; i<2; ++i) {
            j = recv(sock_up, (void *)buff_ack, sizeof buff_ack, 0);
            clock_gettime(CLOCK_MONOTONIC, &recv_time);
            if (j == -1) {
                continue;

            } else if ((j < 4) || (buff_ack[0] != PROTOCOL_VERSION) || (buff_ack[3] != PKT_PUSH_ACK)) {
                MSG_DEBUG(DEBUG_INFO, "INFO~ [up] ignored invalid non-ACL packet\n");
                continue;
            } else if ((buff_ack[1] != token_h) || (buff_ack[2] != token_l)) {
                //MSG_DEBUG(DEBUG_INFO, "INFO~ [up] ignored out-of sync ACK packet\n");
                continue;
            } else {
                MSG_DEBUG(DEBUG_INFO, "INFO~ [up] PUSH_ACK received in %i ms\n", (int)(1000 * difftimespec(recv_time, send_time)));
                meas_up_ack_rcv += 1;
                break;
            }
        }
    }  
    MSG_DEBUG(DEBUG_INFO, "INFO~ End of upstream thread\n");
}

void thread_proc_rxpkt() {
    int i, idx; /* loop variables */
    int fsize = 0;
    struct lgw_pkt_rx_s *p; /* pointer on a RX packet */

    uint32_t mic;
    uint32_t fcnt;
    bool fcnt_valid;

    enum jit_error_e jit_result = JIT_ERROR_OK;

    PKTS *entry;
    DNLINK *dnelem;

    LoRaMacMessageData_t macmsg;

    uint8_t payloaden[MAXPAYLOAD] = {'\0'};  /* data which have decrypted */
    uint8_t payloadtxt[MAXPAYLOAD] = {'\0'};  /* data which have decrypted */
    char addr[16] = {'\0'};

    while(!exit_sig && !quit_sig) {
        sem_wait(&rxpkt_rec_sem);
        entry = rxpkts_link;
        if (entry == NULL)
            continue;
        do {
            for (idx = 0; idx < entry->nb_pkt; ++idx) {
                p = &entry->rxpkt[idx];
                macmsg.Buffer = p->payload;
                macmsg.BufSize = p->size;
                if ( LORAMAC_PARSER_SUCCESS == LoRaMacParserData(&macmsg)) { 
                    printf_mac_header(&macmsg);
					
					if (maccrypto_num) {
                        struct devinfo devinfo = { .devaddr = macmsg.FHDR.DevAddr };
                        if (db_lookup_skey(cntx.lookupskey, (void *) &devinfo)) {

                            /* Debug message of appskey
                            printf("INFO~ [Decode]appskey:");
                            for (i = 0; i < sizeof(devinfo.appskey); ++i) {
                                printf("%02X", devinfo.appskey[i]);
                            }
                            printf("\n");
                            */

                            if (p->size > 13) { /* offset of frmpayload */
                                fsize = p->size - 13 - macmsg.FHDR.FCtrl.Bits.FOptsLen; 
                                memcpy(payloaden, p->payload + 9 + macmsg.FHDR.FCtrl.Bits.FOptsLen, fsize);
                                fcnt_valid = false;
                                for (i = 0; i < FCNT_GAP; i++) {   // loop 8 times
                                    fcnt = macmsg.FHDR.FCnt | (i * 0x10000);
                                    LoRaMacComputeMic(p->payload, p->size - 4, devinfo.nwkskey, devinfo.devaddr, UP, fcnt, &mic);
                                    MSG_DEBUG(DEBUG_INFO, "INFO~ [MIC] mic=%08X, MIC=%08X, fcnt=%u, FCNT=%u\n", mic, macmsg.MIC, fcnt, macmsg.FHDR.FCnt);
                                    if (mic == macmsg.MIC) {
                                        fcnt_valid = true;
                                        MSG_DEBUG(DEBUG_INFO, "INFO~ [MIC] Found a match fcnt(=%u)\n", fcnt);
                                        break;
                                    }
                                }

                                if (fcnt_valid) {

                                    if (macmsg.FPort == 0)
                                        LoRaMacPayloadDecrypt(payloaden, fsize, devinfo.nwkskey, devinfo.devaddr, UP, fcnt, payloadtxt);
                                    else
                                        LoRaMacPayloadDecrypt(payloaden, fsize, devinfo.appskey, devinfo.devaddr, UP, fcnt, payloadtxt);

                                    /* Debug message of decoded payload
                                    printf("INFO~ [Decode]RX(%d):", fsize);
                                    for (i = 0; i < fsize; ++i) {
                                        printf("%02X", payloadtxt[i]);
                                    }
                                    printf("\n");
                                    */

                                    FILE *fp;
                                    char pushpath[128];
                                    char rssi_snr[32] = {'\0'};
                                    sprintf(rssi_snr, "%08X%08X", (short)p->rssi, (short)(p->snr*10));
                                    snprintf(pushpath, sizeof(pushpath), "/var/iot/channels/%08X", devinfo.devaddr);
                                    fp = fopen(pushpath, "w+");
                                    if (NULL == fp)
                                        MSG_DEBUG(DEBUG_INFO, "INFO~ [Decrypto] Fail to open path: %s\n", pushpath);
                                    else { 
                                        fwrite(rssi_snr,sizeof(uint8_t), 16, fp);
                                        fwrite(payloadtxt, sizeof(uint8_t), fsize + 1, fp);
                                        fflush(fp); 
                                        fclose(fp);
                                    }
                                } else 
                                    MSG_DEBUG(DEBUG_INFO, "INFO~ [MIC] Invalid fcnt(=%u) for devaddr:%08X \n", macmsg.FHDR.FCnt, devinfo.devaddr);

                            }

                            /* Customer downlink process */
                            sprintf(addr, "%08X", devinfo.devaddr);
                            dnelem = search_dnlink(addr);
                            if (dnelem != NULL) {
                                MSG_DEBUG(DEBUG_INFO, "INFO~ [procpkt]Found a match devaddr: %s\n", addr);
                                jit_result = custom_rx2dn(dnelem, &devinfo, p->count_us, TIMESTAMPED);
                                if (jit_result == JIT_ERROR_OK) { /* Next upmsg willbe indicate if received by note */
                                    pthread_mutex_lock(&mx_dnlink);
                                    if (dnelem == dn_link) 
                                        dn_link = dnelem->next;
                                    if (dnelem->pre != NULL)
                                        dnelem->pre->next = dnelem->next;
                                    if (dnelem->next != NULL)
                                        dnelem->next->pre = dnelem->pre;
                                    free(dnelem);
                                    pthread_mutex_unlock(&mx_dnlink);
                                } else {
                                    MSG_DEBUG(DEBUG_ERROR, "ERROR~ [procpkt]Packet REJECTED (jit error=%d)\n", jit_result);
                                }
                            }

                        } else
                            MSG_DEBUG(DEBUG_WARNING, "DECRYPT~ [Ignore] Can't find SessionKeys for Dev %08X\n", devinfo.devaddr);
                    }
                }

                if (strcmp(server_type, "lorawan")) {
                    payload_deal(p);
                }
            }

            pthread_mutex_lock(&mx_rxpkts_link);
            rxpkts_link = entry->next;
            pthread_mutex_unlock(&mx_rxpkts_link);
            free(entry);  /* clean rxpkts_link */
            entry = rxpkts_link;
        } while (entry != NULL);
    }
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 2: POLLING SERVER AND ENQUEUING PACKETS IN JIT QUEUE ---------- */

void thread_down(void) {
    int i; /* loop variables */

    uint32_t pull_send = 0, pull_ack = 0; /* count pull request for reconnect the link */

    /* configuration and metadata for an outbound packet */
    struct lgw_pkt_tx_s txpkt;
    bool sent_immediate = false; /* option to sent the packet immediately */

    /* local timekeeping variables */
    struct timespec send_time; /* time of the pull request */
    struct timespec recv_time; /* time of return from recv socket call */

    /* data buffers */
    uint8_t buff_down[1000]; /* buffer to receive downstream packets */
    uint8_t buff_req[12]; /* buffer to compose pull requests */
    int msg_len;

    /* protocol variables */
    uint8_t token_h; /* random token for acknowledgement matching */
    uint8_t token_l; /* random token for acknowledgement matching */
    bool req_ack = false; /* keep track of whether PULL_DATA was acknowledged or not */

    /* JSON parsing variables */
    JSON_Value *root_val = NULL;
    JSON_Object *txpk_obj = NULL;
    JSON_Value *val = NULL; /* needed to detect the absence of some fields */
    const char *str; /* pointer to sub-strings in the JSON data */
    short x0, x1;
    uint64_t x2;
    double x3, x4;

    /* variables to send on GPS timestamp */
    struct tref local_ref; /* time reference used for GPS <-> timestamp conversion */
    struct timespec gps_tx; /* GPS time that needs to be converted to timestamp */

    LoRaMacMessageData_t macmsg; /* LoraMacMessageData for decode mac header */

    /* beacon variables */
    struct lgw_pkt_tx_s beacon_pkt;
    uint8_t beacon_chan;
    uint8_t beacon_loop;
    size_t beacon_RFU1_size = 0;
    size_t beacon_RFU2_size = 0;
    uint8_t beacon_pyld_idx = 0;
    time_t diff_beacon_time;
    struct timespec next_beacon_gps_time; /* gps time of next beacon packet */
    struct timespec last_beacon_gps_time; /* gps time of last enqueued beacon packet */
    int retry;

    /* beacon data fields, byte 0 is Least Significant Byte */
    int32_t field_latitude; /* 3 bytes, derived from reference latitude */
    int32_t field_longitude; /* 3 bytes, derived from reference longitude */
    uint16_t field_crc1, field_crc2;

    /* auto-quit variable */
    uint32_t autoquit_cnt = 0; /* count the number of PULL_DATA sent since the latest PULL_ACK */

    /* Just In Time downlink */
    struct timeval current_unix_time;
    struct timeval current_concentrator_time;
    enum jit_error_e jit_result = JIT_ERROR_OK;
    enum jit_pkt_type_e downlink_type;

    buff_req[0] = PROTOCOL_VERSION;
    buff_req[3] = PKT_PULL_DATA;
    *(uint32_t *)(buff_req + 4) = net_mac_h;
    *(uint32_t *)(buff_req + 8) = net_mac_l;

    /* beacon variables initialization */
    last_beacon_gps_time.tv_sec = 0;
    last_beacon_gps_time.tv_nsec = 0;

    /* beacon packet parameters */
    beacon_pkt.tx_mode = ON_GPS; /* send on PPS pulse */
    beacon_pkt.rf_chain = 0; /* antenna A */
    beacon_pkt.rf_power = beacon_power;
    beacon_pkt.modulation = MOD_LORA;
    switch (beacon_bw_hz) {
        case 125000:
            beacon_pkt.bandwidth = BW_125KHZ;
            break;
        case 500000:
            beacon_pkt.bandwidth = BW_500KHZ;
            break;
        default:
            /* should not happen */
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ unsupported bandwidth for beacon\n");
            exit(EXIT_FAILURE);
    }
    switch (beacon_datarate) {
        case 8:
            beacon_pkt.datarate = DR_LORA_SF8;
            beacon_RFU1_size = 1;
            beacon_RFU2_size = 3;
            break;
        case 9:
            beacon_pkt.datarate = DR_LORA_SF9;
            beacon_RFU1_size = 2;
            beacon_RFU2_size = 0;
            break;
        case 10:
            beacon_pkt.datarate = DR_LORA_SF10;
            beacon_RFU1_size = 3;
            beacon_RFU2_size = 1;
            break;
        case 12:
            beacon_pkt.datarate = DR_LORA_SF12;
            beacon_RFU1_size = 5;
            beacon_RFU2_size = 3;
            break;
        default:
            /* should not happen */
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ unsupported datarate for beacon\n");
            exit(EXIT_FAILURE);
    }
    beacon_pkt.size = beacon_RFU1_size + 4 + 2 + 7 + beacon_RFU2_size + 2;
    beacon_pkt.coderate = CR_LORA_4_5;
    beacon_pkt.invert_pol = false;
    beacon_pkt.preamble = 10;
    beacon_pkt.no_crc = true;
    beacon_pkt.no_header = true;

    /* network common part beacon fields (little endian) */
    for (i = 0; i < (int)beacon_RFU1_size; i++) {
        beacon_pkt.payload[beacon_pyld_idx++] = 0x0;
    }

    /* network common part beacon fields (little endian) */
    beacon_pyld_idx += 4; /* time (variable), filled later */
    beacon_pyld_idx += 2; /* crc1 (variable), filled later */

    /* calculate the latitude and longitude that must be publicly reported */
    field_latitude = (int32_t)((reference_coord.lat / 90.0) * (double)(1<<23));
    if (field_latitude > (int32_t)0x007FFFFF) {
        field_latitude = (int32_t)0x007FFFFF; /* +90 N is represented as 89.99999 N */
    } else if (field_latitude < (int32_t)0xFF800000) {
        field_latitude = (int32_t)0xFF800000;
    }
    field_longitude = (int32_t)((reference_coord.lon / 180.0) * (double)(1<<23));
    if (field_longitude > (int32_t)0x007FFFFF) {
        field_longitude = (int32_t)0x007FFFFF; /* +180 E is represented as 179.99999 E */
    } else if (field_longitude < (int32_t)0xFF800000) {
        field_longitude = (int32_t)0xFF800000;
    }

    /* gateway specific beacon fields */
    beacon_pkt.payload[beacon_pyld_idx++] = beacon_infodesc;
    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF &  field_latitude;
    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & (field_latitude >>  8);
    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & (field_latitude >> 16);
    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF &  field_longitude;
    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & (field_longitude >>  8);
    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & (field_longitude >> 16);

    /* RFU */
    for (i = 0; i < (int)beacon_RFU2_size; i++) {
        beacon_pkt.payload[beacon_pyld_idx++] = 0x0;
    }

    /* CRC of the beacon gateway specific part fields */
    field_crc2 = crc16((beacon_pkt.payload + 6 + beacon_RFU1_size), 7 + beacon_RFU2_size);
    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF &  field_crc2;
    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & (field_crc2 >> 8);

    /* JIT queue initialization */
    jit_queue_init(&jit_queue);

    while (!exit_sig && !quit_sig) {

        /* auto-quit if the threshold is crossed */
        if ((autoquit_threshold > 0) && (autoquit_cnt >= autoquit_threshold)) {
            exit_sig = true;
            MSG_DEBUG(DEBUG_INFO, "INFO~ [down] the last %u PULL_DATA were not ACKed, exiting application\n", autoquit_threshold);
            break;
        }

        /* generate random token for request */
        token_h = (uint8_t)rand(); /* random token */
        token_l = (uint8_t)rand(); /* random token */
        buff_req[1] = token_h;
        buff_req[2] = token_l;

        /* send PULL request and record time */
        send(sock_down, (void *)buff_req, sizeof buff_req, 0);
        pull_send++;
        clock_gettime(CLOCK_MONOTONIC, &send_time);
        pthread_mutex_lock(&mx_meas_dw);
        meas_dw_pull_sent += 1;
        pthread_mutex_unlock(&mx_meas_dw);
        req_ack = false;
        autoquit_cnt++;

        /* listen to packets and process them until a new PULL request must be sent */
        recv_time = send_time;
        while ((int)difftimespec(recv_time, send_time) < keepalive_time) {

            /* try to receive a datagram every 0.2 second in 5 seconds*/
            pthread_mutex_lock(&mx_sockdn); /*maybe reconnect, so lock */ 
            msg_len = recv(sock_down, (void *)buff_down, (sizeof buff_down)-1, 0);
            pthread_mutex_unlock(&mx_sockdn); /*maybe reconnect, so lock */ 
            clock_gettime(CLOCK_MONOTONIC, &recv_time);

            /* Pre-allocate beacon slots in JiT queue, to check downlink collisions */
            beacon_loop = JIT_NUM_BEACON_IN_QUEUE - jit_queue.num_beacon;
            retry = 0;
            while (beacon_loop && (beacon_period != 0)) {
                pthread_mutex_lock(&mx_timeref);
                /* Wait for GPS to be ready before inserting beacons in JiT queue */
                if ((gps_ref_valid == true) && (xtal_correct_ok == true)) {

                    /* compute GPS time for next beacon to come      */
                    /*   LoRaWAN: T = k*beacon_period + TBeaconDelay */
                    /*            with TBeaconDelay = [1.5ms +/- 1µs]*/
                    if (last_beacon_gps_time.tv_sec == 0) {
                        /* if no beacon has been queued, get next slot from current GPS time */
                        diff_beacon_time = time_reference_gps.gps.tv_sec % ((time_t)beacon_period);
                        next_beacon_gps_time.tv_sec = time_reference_gps.gps.tv_sec +
                                                        ((time_t)beacon_period - diff_beacon_time);
                    } else {
                        /* if there is already a beacon, take it as reference */
                        next_beacon_gps_time.tv_sec = last_beacon_gps_time.tv_sec + beacon_period;
                    }
                    /* now we can add a beacon_period to the reference to get next beacon GPS time */
                    next_beacon_gps_time.tv_sec += (retry * beacon_period);
                    next_beacon_gps_time.tv_nsec = 0;

#if DEBUG_BEACON
                    {
                    time_t time_unix;

                    time_unix = time_reference_gps.gps.tv_sec + UNIX_GPS_EPOCH_OFFSET;
                    MSG_DEBUG(DEBUG_BEACON, "GPS-now : %s", ctime(&time_unix));
                    time_unix = last_beacon_gps_time.tv_sec + UNIX_GPS_EPOCH_OFFSET;
                    MSG_DEBUG(DEBUG_BEACON, "GPS-last: %s", ctime(&time_unix));
                    time_unix = next_beacon_gps_time.tv_sec + UNIX_GPS_EPOCH_OFFSET;
                    MSG_DEBUG(DEBUG_BEACON, "GPS-next: %s", ctime(&time_unix));
                    }
#endif

                    /* convert GPS time to concentrator time, and set packet counter for JiT trigger */
                    lgw_gps2cnt(time_reference_gps, next_beacon_gps_time, &(beacon_pkt.count_us));
                    pthread_mutex_unlock(&mx_timeref);

                    /* apply frequency correction to beacon TX frequency */
                    if (beacon_freq_nb > 1) {
                        beacon_chan = (next_beacon_gps_time.tv_sec / beacon_period) % beacon_freq_nb; /* floor rounding */
                    } else {
                        beacon_chan = 0;
                    }
                    /* Compute beacon frequency */
                    beacon_pkt.freq_hz = beacon_freq_hz + (beacon_chan * beacon_freq_step);

                    /* load time in beacon payload */
                    beacon_pyld_idx = beacon_RFU1_size;
                    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF &  next_beacon_gps_time.tv_sec;
                    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & (next_beacon_gps_time.tv_sec >>  8);
                    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & (next_beacon_gps_time.tv_sec >> 16);
                    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & (next_beacon_gps_time.tv_sec >> 24);

                    /* calculate CRC */
                    field_crc1 = crc16(beacon_pkt.payload, 4 + beacon_RFU1_size); /* CRC for the network common part */
                    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & field_crc1;
                    beacon_pkt.payload[beacon_pyld_idx++] = 0xFF & (field_crc1 >> 8);

                    /* Insert beacon packet in JiT queue */
                    gettimeofday(&current_unix_time, NULL);
                    get_concentrator_time(&current_concentrator_time, current_unix_time);
                    jit_result = jit_enqueue(&jit_queue, &current_concentrator_time, &beacon_pkt, JIT_PKT_TYPE_BEACON);
                    if (jit_result == JIT_ERROR_OK) {
                        /* update stats */
                        pthread_mutex_lock(&mx_meas_dw);
                        meas_nb_beacon_queued += 1;
                        pthread_mutex_unlock(&mx_meas_dw);

                        /* One more beacon in the queue */
                        beacon_loop--;
                        retry = 0;
                        last_beacon_gps_time.tv_sec = next_beacon_gps_time.tv_sec; /* keep this beacon time as reference for next one to be programmed */

                        /* display beacon payload */
                        MSG_DEBUG(DEBUG_BEACON, "BEACON~ Beacon queued (count_us=%u, freq_hz=%u, size=%u):\n", beacon_pkt.count_us, beacon_pkt.freq_hz, beacon_pkt.size);
                        MSG_DEBUG(DEBUG_BEACON, "   => " );
                        for (i = 0; i < beacon_pkt.size; ++i) {
                            MSG_DEBUG(DEBUG_BEACON, "%02X ", beacon_pkt.payload[i]);
                        }
                        MSG("\n");
                    } else {
                        MSG_DEBUG(DEBUG_BEACON, "--> beacon queuing failed with %d\n", jit_result);
                        /* update stats */
                        pthread_mutex_lock(&mx_meas_dw);
                        if (jit_result != JIT_ERROR_COLLISION_BEACON) {
                            meas_nb_beacon_rejected += 1;
                        }
                        pthread_mutex_unlock(&mx_meas_dw);
                        /* In case previous enqueue failed, we retry one period later until it succeeds */
                        /* Note: In case the GPS has been unlocked for a while, there can be lots of retries */
                        /*       to be done from last beacon time to a new valid one */
                        retry++;
                        MSG_DEBUG(DEBUG_BEACON, "--> beacon queuing retry=%d\n", retry);
                    }
                } else {
                    pthread_mutex_unlock(&mx_timeref);
                    break;
                }
            }

            /* if no network message was received, got back to listening sock_down socket */
            if (msg_len == -1) {
				if (!strcmp(server_type, "lorawan")){
						//MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] recv returned %s\n", strerror(errno)); /* too verbose */
					if (errno != EAGAIN) { /* ! timeout */
						MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] relink recv error sockup(%d),sockdown(%d)\n", sock_up, sock_down); 
						/* server connection error */
						if (sock_down) close(sock_down);
						if ((sock_down = init_socket(serv_addr, serv_port_down,
										(void *)&pull_timeout, sizeof(pull_timeout))) == -1)
							exit(EXIT_FAILURE);
					}			
				}
				continue;	
            }

            /* if the datagram does not respect protocol, just ignore it */
            if ((msg_len < 4) || ((buff_down[3] != PKT_PULL_RESP) && (buff_down[3] != PKT_PULL_ACK))) {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] ignoring invalid packet len=%d, protocol_version=%d, id=%d\n",
                        msg_len, buff_down[0], buff_down[3]);
                continue;
            }

            /* if the datagram is an ACK, check token */
            if (buff_down[3] == PKT_PULL_ACK) {
                pull_ack++;
                if ((buff_down[1] == token_h) && (buff_down[2] == token_l)) {
                    if (req_ack) {
                        MSG_DEBUG(DEBUG_INFO, "INFO~ [down] duplicate ACK received :)\n");
                    } else { /* if that packet was not already acknowledged */
                        req_ack = true;
                        autoquit_cnt = 0;
                        pthread_mutex_lock(&mx_meas_dw);
                        meas_dw_ack_rcv += 1;
                        pthread_mutex_unlock(&mx_meas_dw);
                        MSG_DEBUG(DEBUG_INFO, "INFO~ [down] PULL_ACK received in %i ms\n", (int)(1000 * difftimespec(recv_time, send_time)));
                    }
                } else { /* out-of-sync token */
                    MSG_DEBUG(DEBUG_INFO, "INFO~ [down] received out-of-sync ACK\n");
                }
                continue;
            }

            /* the datagram is a PULL_RESP */
            buff_down[msg_len] = 0; /* add string terminator, just to be safe */
            MSG_DEBUG(DEBUG_INFO, "INFO~ [down] PULL_RESP received  - token[%d:%d] :)\n", buff_down[1], buff_down[2]); /* very verbose */
            MSG_DEBUG(DEBUG_PKT_FWD, "RXTX~ %s\n", (char *)(buff_down + 4)); /* DEBUG: display JSON payload */


            /* initialize TX struct and try to parse JSON */
            memset(&txpkt, 0, sizeof txpkt);
            root_val = json_parse_string_with_comments((const char *)(buff_down + 4)); /* JSON offset */
            if (root_val == NULL) {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] invalid JSON, TX aborted\n");
                continue;
            }

            /* look for JSON sub-object 'txpk' */
            txpk_obj = json_object_get_object(json_value_get_object(root_val), "txpk");
            if (txpk_obj == NULL) {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no \"txpk\" object in JSON, TX aborted\n");
                json_value_free(root_val);
                continue;
            }

            /* Parse "immediate" tag, or target timestamp, or UTC time to be converted by GPS (mandatory) */
            i = json_object_get_boolean(txpk_obj,"imme"); /* can be 1 if true, 0 if false, or -1 if not a JSON boolean */
            if (i == 1) {
                /* TX procedure: send immediately */
                sent_immediate = true;
                downlink_type = JIT_PKT_TYPE_DOWNLINK_CLASS_C;
                MSG_DEBUG(DEBUG_INFO, "INFO~ [down] a packet will be sent in \"immediate\" mode\n");
            } else {
                sent_immediate = false;
                val = json_object_get_value(txpk_obj,"tmst");
                if (val != NULL) {
                    /* TX procedure: send on timestamp value */
                    txpkt.count_us = (uint32_t)json_value_get_number(val);

                    /* Concentrator timestamp is given, we consider it is a Class A downlink */
                    downlink_type = JIT_PKT_TYPE_DOWNLINK_CLASS_A;
                } else {
                    /* TX procedure: send on GPS time (converted to timestamp value) */
                    val = json_object_get_value(txpk_obj, "tmms");
                    if (val == NULL) {
                        MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.tmst\" or \"txpk.tmms\" objects in JSON, TX aborted\n");
                        json_value_free(root_val);
                        continue;
                    }
                    if (gps_enabled == true) {
                        pthread_mutex_lock(&mx_timeref);
                        if (gps_ref_valid == true) {
                            local_ref = time_reference_gps;
                            pthread_mutex_unlock(&mx_timeref);
                        } else {
                            pthread_mutex_unlock(&mx_timeref);
                            MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no valid GPS time reference yet, impossible to send packet on specific GPS time, TX aborted\n");
                            json_value_free(root_val);

                            /* send acknoledge datagram to server */
                            send_tx_ack(buff_down[1], buff_down[2], JIT_ERROR_GPS_UNLOCKED);
                            continue;
                        }
                    } else {
                        MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] GPS disabled, impossible to send packet on specific GPS time, TX aborted\n");
                        json_value_free(root_val);

                        /* send acknoledge datagram to server */
                        send_tx_ack(buff_down[1], buff_down[2], JIT_ERROR_GPS_UNLOCKED);
                        continue;
                    }

                    /* Get GPS time from JSON */
                    x2 = (uint64_t)json_value_get_number(val);

                    /* Convert GPS time from milliseconds to timespec */
                    x3 = modf((double)x2/1E3, &x4);
                    gps_tx.tv_sec = (time_t)x4; /* get seconds from integer part */
                    gps_tx.tv_nsec = (long)(x3 * 1E9); /* get nanoseconds from fractional part */

                    /* transform GPS time to timestamp */
                    i = lgw_gps2cnt(local_ref, gps_tx, &(txpkt.count_us));
                    if (i != LGW_GPS_SUCCESS) {
                        MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] could not convert GPS time to timestamp, TX aborted\n");
                        json_value_free(root_val);
                        continue;
                    } else {
                        MSG_DEBUG(DEBUG_INFO, "INFO~ [down] a packet will be sent on timestamp value %u (calculated from GPS time)\n", txpkt.count_us);
                    }

                    /* GPS timestamp is given, we consider it is a Class B downlink */
                    downlink_type = JIT_PKT_TYPE_DOWNLINK_CLASS_B;
                }
            }

            /* Parse "No CRC" flag (optional field) */
            val = json_object_get_value(txpk_obj,"ncrc");
            if (val != NULL) {
                txpkt.no_crc = (bool)json_value_get_boolean(val);
            }

            /* parse target frequency (mandatory) */
            val = json_object_get_value(txpk_obj,"freq");
            if (val == NULL) {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.freq\" object in JSON, TX aborted\n");
                json_value_free(root_val);
                continue;
            }
            txpkt.freq_hz = (uint32_t)((double)(1.0e6) * json_value_get_number(val));

            /* parse RF chain used for TX (mandatory) */
            val = json_object_get_value(txpk_obj,"rfch");
            if (val == NULL) {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.rfch\" object in JSON, TX aborted\n");
                json_value_free(root_val);
                continue;
            }
            txpkt.rf_chain = (uint8_t)json_value_get_number(val);

            /* parse TX power (optional field) */
            val = json_object_get_value(txpk_obj,"powe");
            if (val != NULL) {
                txpkt.rf_power = (int8_t)json_value_get_number(val) - antenna_gain;
            }

            /* Parse modulation (mandatory) */
            str = json_object_get_string(txpk_obj, "modu");
            if (str == NULL) {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.modu\" object in JSON, TX aborted\n");
                json_value_free(root_val);
                continue;
            }
            if (strcmp(str, "LORA") == 0) {
                /* Lora modulation */
                txpkt.modulation = MOD_LORA;

                /* Parse Lora spreading-factor and modulation bandwidth (mandatory) */
                str = json_object_get_string(txpk_obj, "datr");
                if (str == NULL) {
                    MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.datr\" object in JSON, TX aborted\n");
                    json_value_free(root_val);
                    continue;
                }
                i = sscanf(str, "SF%2hdBW%3hd", &x0, &x1);
                if (i != 2) {
                    MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] format error in \"txpk.datr\", TX aborted\n");
                    json_value_free(root_val);
                    continue;
                }
                switch (x0) {
                    case  7: txpkt.datarate = DR_LORA_SF7;  break;
                    case  8: txpkt.datarate = DR_LORA_SF8;  break;
                    case  9: txpkt.datarate = DR_LORA_SF9;  break;
                    case 10: txpkt.datarate = DR_LORA_SF10; break;
                    case 11: txpkt.datarate = DR_LORA_SF11; break;
                    case 12: txpkt.datarate = DR_LORA_SF12; break;
                    default:
                        MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] format error in \"txpk.datr\", invalid SF, TX aborted\n");
                        json_value_free(root_val);
                        continue;
                }
                switch (x1) {
                    case 125: txpkt.bandwidth = BW_125KHZ; break;
                    case 250: txpkt.bandwidth = BW_250KHZ; break;
                    case 500: txpkt.bandwidth = BW_500KHZ; break;
                    default:
                        MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] format error in \"txpk.datr\", invalid BW, TX aborted\n");
                        json_value_free(root_val);
                        continue;
                }

                /* Parse ECC coding rate (optional field) */
                str = json_object_get_string(txpk_obj, "codr");
                if (str == NULL) {
                    MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.codr\" object in json, TX aborted\n");
                    json_value_free(root_val);
                    continue;
                }
                if      (strcmp(str, "4/5") == 0) txpkt.coderate = CR_LORA_4_5;
                else if (strcmp(str, "4/6") == 0) txpkt.coderate = CR_LORA_4_6;
                else if (strcmp(str, "2/3") == 0) txpkt.coderate = CR_LORA_4_6;
                else if (strcmp(str, "4/7") == 0) txpkt.coderate = CR_LORA_4_7;
                else if (strcmp(str, "4/8") == 0) txpkt.coderate = CR_LORA_4_8;
                else if (strcmp(str, "1/2") == 0) txpkt.coderate = CR_LORA_4_8;
                else {
                    MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] format error in \"txpk.codr\", TX aborted\n");
                    json_value_free(root_val);
                    continue;
                }

                /* Parse signal polarity switch (optional field) */
                val = json_object_get_value(txpk_obj,"ipol");
                if (val != NULL) {
                    txpkt.invert_pol = (bool)json_value_get_boolean(val);
                }

                /* parse Lora preamble length (optional field, optimum min value enforced) */
                val = json_object_get_value(txpk_obj,"prea");
                if (val != NULL) {
                    i = (int)json_value_get_number(val);
                    if (i >= MIN_LORA_PREAMB) {
                        txpkt.preamble = (uint16_t)i;
                    } else {
                        txpkt.preamble = (uint16_t)MIN_LORA_PREAMB;
                    }
                } else {
                    txpkt.preamble = (uint16_t)STD_LORA_PREAMB;
                }

            } else if (strcmp(str, "FSK") == 0) {
                /* FSK modulation */
                txpkt.modulation = MOD_FSK;

                /* parse FSK bitrate (mandatory) */
                val = json_object_get_value(txpk_obj,"datr");
                if (val == NULL) {
                    MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.datr\" object in JSON, TX aborted\n");
                    json_value_free(root_val);
                    continue;
                }
                txpkt.datarate = (uint32_t)(json_value_get_number(val));

                /* parse frequency deviation (mandatory) */
                val = json_object_get_value(txpk_obj,"fdev");
                if (val == NULL) {
                    MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.fdev\" object in JSON, TX aborted\n");
                    json_value_free(root_val);
                    continue;
                }
                txpkt.f_dev = (uint8_t)(json_value_get_number(val) / 1000.0); /* JSON value in Hz, txpkt.f_dev in kHz */

                /* parse FSK preamble length (optional field, optimum min value enforced) */
                val = json_object_get_value(txpk_obj,"prea");
                if (val != NULL) {
                    i = (int)json_value_get_number(val);
                    if (i >= MIN_FSK_PREAMB) {
                        txpkt.preamble = (uint16_t)i;
                    } else {
                        txpkt.preamble = (uint16_t)MIN_FSK_PREAMB;
                    }
                } else {
                    txpkt.preamble = (uint16_t)STD_FSK_PREAMB;
                }

            } else {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] invalid modulation in \"txpk.modu\", TX aborted\n");
                json_value_free(root_val);
                continue;
            }

            /* Parse payload length (mandatory) */
            val = json_object_get_value(txpk_obj,"size");
            if (val == NULL) {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.size\" object in JSON, TX aborted\n");
                json_value_free(root_val);
                continue;
            }
            txpkt.size = (uint16_t)json_value_get_number(val);

            /* Parse payload data (mandatory) */
            str = json_object_get_string(txpk_obj, "data");
            if (str == NULL) {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] no mandatory \"txpk.data\" object in JSON, TX aborted\n");
                json_value_free(root_val);
                continue;
            }
            i = b64_to_bin(str, strlen(str), txpkt.payload, sizeof txpkt.payload);
            if (i != txpkt.size) {
                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [down] mismatch between .size and .data size once converter to binary\n");
            }

            /* free the JSON parse tree from memory */
            json_value_free(root_val);

            /* select TX mode */
            if (sent_immediate) {
                txpkt.tx_mode = IMMEDIATE;
            } else {
                txpkt.tx_mode = TIMESTAMPED;
            }

            /* record measurement data */
            pthread_mutex_lock(&mx_meas_dw);
            meas_dw_dgram_rcv += 1; /* count only datagrams with no JSON errors */
            total_pkt_dw++;
            meas_dw_network_byte += msg_len; /* meas_dw_network_byte */
            meas_dw_payload_byte += txpkt.size;
            pthread_mutex_unlock(&mx_meas_dw);

            db_incpkt(cntx.totaldw_stmt, total_pkt_dw);

            /* check TX parameter before trying to queue packet */
            jit_result = JIT_ERROR_OK;
            if ((txpkt.freq_hz < tx_freq_min[txpkt.rf_chain]) || (txpkt.freq_hz > tx_freq_max[txpkt.rf_chain])) {
                jit_result = JIT_ERROR_TX_FREQ;
                MSG_DEBUG(DEBUG_ERROR, "ERROR~ Packet REJECTED, unsupported frequency - %u (min:%u,max:%u)\n", txpkt.freq_hz, tx_freq_min[txpkt.rf_chain], tx_freq_max[txpkt.rf_chain]);
            }
			if (jit_result == JIT_ERROR_OK) {
                int pwr_level = 14;
                for (i=0; i<txlut.size; i++) {
                    if (txlut.lut[i].rf_power <= txpkt.rf_power &&
                            pwr_level < txlut.lut[i].rf_power) {
                        pwr_level = txlut.lut[i].rf_power;
                    }
                }
                if (pwr_level != txpkt.rf_power) {
					MSG_DEBUG(DEBUG_INFO, "INFO~ Can't find specify RF power %ddB, use %ddB\n", txpkt.rf_power, pwr_level);
                    txpkt.rf_power = pwr_level;
                }
            }

            /* insert packet to be sent into JIT queue */
            if (jit_result == JIT_ERROR_OK) {
                gettimeofday(&current_unix_time, NULL);
                get_concentrator_time(&current_concentrator_time, current_unix_time);
                jit_result = jit_enqueue(&jit_queue, &current_concentrator_time, &txpkt, downlink_type);
                if (jit_result != JIT_ERROR_OK) {
                    MSG_DEBUG(DEBUG_ERROR, "ERROR~ Packet REJECTED (jit error=%d)\n", jit_result);
                }
                pthread_mutex_lock(&mx_meas_dw);
                meas_nb_tx_requested += 1;
                pthread_mutex_unlock(&mx_meas_dw);
            }

            /* output the payload header struct */
            macmsg.Buffer = txpkt.payload;
            macmsg.BufSize = txpkt.size;
            if ( LORAMAC_PARSER_SUCCESS == LoRaMacParserData(&macmsg) ) 
                printf_mac_header(&macmsg);

            /* Send acknoledge datagram to server */
            send_tx_ack(buff_down[1], buff_down[2], jit_result);

        }

        if (pull_ack < labs(pull_send * 0.1)) {  /* 10% lost */
			if (!strcmp(server_type, "lorawan")) {
				pull_ack = 0;
				pull_send = 0;
				MSG_DEBUG(DEBUG_INFO, "INFO~ [down] PULL_ACK mismatch, reconnect server\n");
				pthread_mutex_lock(&mx_sockdn); /*maybe reconnect, so lock */ 
				if (sock_down) close(sock_down);
				if ((sock_down = init_socket(serv_addr, serv_port_down,
											(void *)&pull_timeout, sizeof(pull_timeout))) == -1)
					exit(EXIT_FAILURE);
				pthread_mutex_unlock(&mx_sockdn); /*maybe reconnect, so lock */ 				
			}
        }
    }
    MSG_DEBUG(DEBUG_INFO, "INFO~ End of downstream thread\n");
}

void print_tx_status(uint8_t tx_status) {
    switch (tx_status) {
        case TX_OFF:
            MSG_DEBUG(DEBUG_INFO, "INFO~ [jit] lgw_status returned TX_OFF\n");
            break;
        case TX_FREE:
            MSG_DEBUG(DEBUG_INFO, "INFO~ [jit] lgw_status returned TX_FREE\n");
            break;
        case TX_EMITTING:
            MSG_DEBUG(DEBUG_INFO, "INFO~ [jit] lgw_status returned TX_EMITTING\n");
            break;
        case TX_SCHEDULED:
            MSG_DEBUG(DEBUG_INFO, "INFO~ [jit] lgw_status returned TX_SCHEDULED\n");
            break;
        default:
            MSG_DEBUG(DEBUG_INFO, "INFO~ [jit] lgw_status returned UNKNOWN (%d)\n", tx_status);
            break;
    }
}


/* -------------------------------------------------------------------------- */
/* --- THREAD 3: CHECKING PACKETS TO BE SENT FROM JIT QUEUE AND SEND THEM --- */

void thread_jit(void) {
    int result = LGW_HAL_SUCCESS;
    struct lgw_pkt_tx_s pkt;
    int pkt_index = -1;
    struct timeval current_unix_time;
    struct timeval current_concentrator_time;
    uint32_t time_us;
    uint16_t diff_us;
    enum jit_error_e jit_result;
    enum jit_pkt_type_e pkt_type;
    uint8_t tx_status;

    while (!exit_sig && !quit_sig) {
        /* transfer data and metadata to the concentrator, and schedule TX */
        gettimeofday(&current_unix_time, NULL);
        get_concentrator_time(&current_concentrator_time, current_unix_time);
        jit_result = jit_peek(&jit_queue, &current_concentrator_time, &pkt_index);
        if (jit_result == JIT_ERROR_OK) {
            if (pkt_index > -1) {
                jit_result = jit_dequeue(&jit_queue, pkt_index, &pkt, &pkt_type);
                if (jit_result == JIT_ERROR_OK) {
                    /* update beacon stats */
                    if (pkt_type == JIT_PKT_TYPE_BEACON) {
                        /* Compensate breacon frequency with xtal error */
                        pthread_mutex_lock(&mx_xcorr);
                        pkt.freq_hz = (uint32_t)(xtal_correct * (double)pkt.freq_hz);
                        MSG_DEBUG(DEBUG_BEACON, "beacon_pkt.freq_hz=%u (xtal_correct=%.15lf)\n", pkt.freq_hz, xtal_correct);
                        pthread_mutex_unlock(&mx_xcorr);

                        /* Update statistics */
                        pthread_mutex_lock(&mx_meas_dw);
                        meas_nb_beacon_sent += 1;
                        pthread_mutex_unlock(&mx_meas_dw);
                        MSG_DEBUG(DEBUG_INFO, "INFO~ Beacon dequeued (count_us=%u)\n", pkt.count_us);
                    }

                    /* send packet to concentrator */
                    if (sx1276) {
                        //pthread_mutex_lock(&mx_sx1276);

                        gettimeofday(&current_unix_time, NULL);
                        get_concentrator_time(&current_concentrator_time, current_unix_time);
                        time_us = current_concentrator_time.tv_sec * 1000000UL + current_concentrator_time.tv_usec;

                        diff_us = pkt.count_us - time_us - 1495/*START_DELAY*/;

                        MSG_DEBUG(DEBUG_JIT, "JITINFO~ pending TX count_us=%u, now_us=%u, diff=%u, waitms=%d\n",
                                        pkt.count_us,
                                        time_us,
                                        pkt.count_us - time_us,
                                        diff_us);
                        

                        if (pkt.tx_mode != IMMEDIATE && diff_us > 0 && diff_us < 30000/*JIT_START_DELAY*/)
                            wait_us(diff_us);

                        /*
                        gettimeofday(&current_unix_time, NULL);
                        get_concentrator_time(&current_concentrator_time, current_unix_time);
                        time_us = current_concentrator_time.tv_sec * 1000000UL + current_concentrator_time.tv_usec;

                        printf("JITINFO~ start TX  now_us=%u, count_us=%u, diff_us=%u, diff=%u\n",
                                    time_us,
                                    pkt.count_us,
                                    diff_us,
                                    time_us - pkt.count_us);
                        */
                        

                        txlora(sxradio, &pkt); 

                        pthread_mutex_lock(&mx_meas_dw);
                        meas_nb_tx_ok += 1;
                        pthread_mutex_unlock(&mx_meas_dw);
                    } else { 
                        /* check if concentrator is free for sending new packet */
                        do {
                            pthread_mutex_lock(&mx_concent); /* may have to wait for a fetch to finish */
                            result = lgw_status(TX_STATUS, &tx_status);
                            pthread_mutex_unlock(&mx_concent); /* free concentrator ASAP */
                            if (result == LGW_HAL_ERROR) {
                                MSG_DEBUG(DEBUG_WARNING, "WARNING~ [jit] lgw_status failed, try again!\n");
                                wait_ms(100);
                            } else if (tx_status == TX_SCHEDULED) {
                                break;
                            } else if (tx_status != TX_FREE) {
                                wait_ms(100);
                            } 
                        } while (tx_status != TX_FREE);

                        pthread_mutex_lock(&mx_concent); /* may have to wait for a fetch to finish */
                        result = lgw_send(pkt);
                        pthread_mutex_unlock(&mx_concent); /* free concentrator ASAP */
                        if (result == LGW_HAL_ERROR) {
                            pthread_mutex_lock(&mx_meas_dw);
                            meas_nb_tx_fail += 1;
                            pthread_mutex_unlock(&mx_meas_dw);
                            MSG_DEBUG(DEBUG_WARNING, "WARNING~ [LGWSEND] lgw_send failed\n");
                            continue;
                        } else if (result == LGW_LBT_ISSUE) {
                            pthread_mutex_lock(&mx_meas_dw);
                            meas_nb_tx_fail += 1;
                            pthread_mutex_unlock(&mx_meas_dw);
                            MSG_DEBUG(DEBUG_WARNING, "WARNING~ [LGWSEND] lgw_send failed, chan is busy\n");
                        } else {
                            pthread_mutex_lock(&mx_meas_dw);
                            meas_nb_tx_ok += 1;
                            pthread_mutex_unlock(&mx_meas_dw);
                            MSG_DEBUG(DEBUG_INFO, "INFO~ [LGWSEND] lgw_send done: count_us=%u, freq=%u, size=%u\n", pkt.count_us, pkt.freq_hz, pkt.size);
                        }
                    }
                    
                } else {
                    MSG_DEBUG(DEBUG_ERROR, "ERROR~ jit_dequeue failed with %d\n", jit_result);
                }
            }
        } else if (jit_result == JIT_ERROR_EMPTY) {
            wait_ms(10);
            /* Do nothing, it can happen */
        } else {
            wait_ms(10);
            MSG_DEBUG(DEBUG_ERROR, "ERROR~ jit_peek failed with %d\n", jit_result);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 4: PARSE GPS MESSAGE AND KEEP GATEWAY IN SYNC ----------------- */

static void gps_process_sync(void) {
    struct timespec gps_time;
    struct timespec utc;
    uint32_t trig_tstamp; /* concentrator timestamp associated with PPM pulse */
    int i = lgw_gps_get(&utc, &gps_time, NULL, NULL);

    /* get GPS time for synchronization */
    if (i != LGW_GPS_SUCCESS) {
        //MSG_DEBUG(DEBUG_WARNING, "WARNING~ [gps] could not get GPS time from GPS\n");
        return;
    }

    /* get timestamp captured on PPM pulse  */
    pthread_mutex_lock(&mx_concent);
    i = lgw_get_trigcnt(&trig_tstamp);
    pthread_mutex_unlock(&mx_concent);
    if (i != LGW_HAL_SUCCESS) {
        MSG_DEBUG(DEBUG_WARNING, "WARNING~ [gps] failed to read concentrator timestamp\n");
        return;
    }

    /* try to update time reference with the new GPS time & timestamp */
    pthread_mutex_lock(&mx_timeref);
    i = lgw_gps_sync(&time_reference_gps, trig_tstamp, utc, gps_time);
    pthread_mutex_unlock(&mx_timeref);
    if (i != LGW_GPS_SUCCESS) {
        MSG_DEBUG(DEBUG_WARNING, "WARNING~ [gps] GPS out of sync, keeping previous time reference\n");
    }
}

static void gps_process_coords(void) {
    /* position variable */
    struct coord_s coord;
    struct coord_s gpserr;
    int    i = lgw_gps_get(NULL, NULL, &coord, &gpserr);

    /* update gateway coordinates */
    pthread_mutex_lock(&mx_meas_gps);
    if (i == LGW_GPS_SUCCESS) {
        gps_coord_valid = true;
        meas_gps_coord = coord;
        meas_gps_err = gpserr;
        // TODO: report other GPS statistics (typ. signal quality & integrity)
    } else {
        gps_coord_valid = false;
    }
    pthread_mutex_unlock(&mx_meas_gps);
}

void thread_gps(void) {
    /* serial variables */
    char serial_buff[128]; /* buffer to receive GPS data */
    size_t wr_idx = 0;     /* pointer to end of chars in buffer */

    /* variables for PPM pulse GPS synchronization */
    enum gps_msg latest_msg; /* keep track of latest NMEA message parsed */

    /* initialize some variables before loop */
    memset(serial_buff, 0, sizeof serial_buff);

    while (!exit_sig && !quit_sig) {
        size_t rd_idx = 0;
        size_t frame_end_idx = 0;

        /* blocking non-canonical read on serial port */
        ssize_t nb_char = read(gps_tty_fd, serial_buff + wr_idx, LGW_GPS_MIN_MSG_SIZE);
        if (nb_char <= 0) {
            MSG_DEBUG(DEBUG_WARNING, "WARNING~ [gps] read() returned value %d\n", nb_char);
            continue;
        }
        wr_idx += (size_t)nb_char;

        /*
        printf("\nGPS~ CHAR:");
        for (i = 0; i < wr_idx; i++) {
            printf("%02X", serial_buff[i]);
        }
        printf("\n");
        */

        /*******************************************
         * Scan buffer for UBX/NMEA sync chars and *
         * attempt to decode frame if one is found *
         *******************************************/
        while(rd_idx < wr_idx) {
            size_t frame_size = 0;

            /* Scan buffer for UBX sync char */
            if(serial_buff[rd_idx] == (char)LGW_GPS_UBX_SYNC_CHAR) {
                /***********************
                 * Found UBX sync char *
                 ***********************/
                latest_msg = lgw_parse_ubx(&serial_buff[rd_idx], (wr_idx - rd_idx), &frame_size);

                if (frame_size > 0) {
                    if (latest_msg == INCOMPLETE) {
                        /* UBX header found but frame appears to be missing bytes */
                        frame_size = 0;
                    } else if (latest_msg == INVALID) {
                        /* message header received but message appears to be corrupted */
                        MSG_DEBUG(DEBUG_WARNING, "WARNING~ [gps] could not get a valid message from GPS (no time)\n");
                        frame_size = 0;
                    } else if (latest_msg == UBX_NAV_TIMEGPS) {
                        gps_process_sync();
                    }
                }
            } else if(serial_buff[rd_idx] == LGW_GPS_NMEA_SYNC_CHAR) {
                /************************
                 * Found NMEA sync char *
                 ************************/
                /* scan for NMEA end marker (LF = 0x0a) */
                char* nmea_end_ptr = memchr(&serial_buff[rd_idx],(int)0x0a, (wr_idx - rd_idx));

                if(nmea_end_ptr) {
                    /* found end marker */
                    frame_size = nmea_end_ptr - &serial_buff[rd_idx] + 1;
                    latest_msg = lgw_parse_nmea(&serial_buff[rd_idx], frame_size);

                    if(latest_msg == INVALID || latest_msg == UNKNOWN) {
                        /* checksum failed */
                        frame_size = 0;
                    } else if (latest_msg == NMEA_GGA) { /* Get location from RMC frames */
                        gps_process_coords();

                    } else if (latest_msg == NMEA_RMC) { /* Get time from RMC frames */
                        gps_process_sync();
                    }
                }
            }

            if(frame_size > 0) {
                /* At this point message is a checksum verified frame
                   we're processed or ignored. Remove frame from buffer */
                rd_idx += frame_size;
                frame_end_idx = rd_idx;
            } else {
                rd_idx++;
            }
        } /* ...for(rd_idx = 0... */

        if(frame_end_idx) {
          /* Frames have been processed. Remove bytes to end of last processed frame */
          memcpy(serial_buff, &serial_buff[frame_end_idx], wr_idx - frame_end_idx);
          wr_idx -= frame_end_idx;
        } /* ...for(rd_idx = 0... */

        /* Prevent buffer overflow */
        if((sizeof(serial_buff) - wr_idx) < LGW_GPS_MIN_MSG_SIZE) {
            memcpy(serial_buff, &serial_buff[LGW_GPS_MIN_MSG_SIZE], wr_idx - LGW_GPS_MIN_MSG_SIZE);
            wr_idx -= LGW_GPS_MIN_MSG_SIZE;
        }
    }
    MSG_DEBUG(DEBUG_INFO, "INFO~ End of GPS thread\n");
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 5: CHECK TIME REFERENCE AND CALCULATE XTAL CORRECTION --------- */

void thread_valid(void) {

    /* GPS reference validation variables */
    long gps_ref_age = 0;
    bool ref_valid_local = false;
    double xtal_err_cpy;

    /* variables for XTAL correction averaging */
    unsigned init_cpt = 0;
    double init_acc = 0.0;
    double x;

    /* correction debug */
    // FILE * log_file = NULL;
    // time_t now_time;
    // char log_name[64];

    /* initialization */
    // time(&now_time);
    // strftime(log_name,sizeof log_name,"xtal_err_%Y%m%dT%H%M%SZ.csv",localtime(&now_time));
    // log_file = fopen(log_name, "w");
    // setbuf(log_file, NULL);
    // fprintf(log_file,"\"xtal_correct\",\"XERR_INIT_AVG %u XERR_FILT_COEF %u\"\n", XERR_INIT_AVG, XERR_FILT_COEF); // DEBUG

    /* main loop task */
    while (!exit_sig && !quit_sig) {
        wait_ms(1000);

        /* calculate when the time reference was last updated */
        pthread_mutex_lock(&mx_timeref);
        gps_ref_age = (long)difftime(time(NULL), time_reference_gps.systime);
        //printf("time(%u), ref(%u)\n", time(NULL), time_reference_gps.systime);
        if ((gps_ref_age >= 0) && (gps_ref_age <= GPS_REF_MAX_AGE)) {
            /* time ref is ok, validate and  */
            gps_ref_valid = true;
            ref_valid_local = true;
            xtal_err_cpy = time_reference_gps.xtal_err;
            //printf("XTAL err: %.15lf (1/XTAL_err:%.15lf)\n", xtal_err_cpy, 1/xtal_err_cpy); // DEBUG
        } else {
            /* time ref is too old, invalidate */
            gps_ref_valid = false;
            ref_valid_local = false;
        }
        pthread_mutex_unlock(&mx_timeref);

        /* manage XTAL correction */
        if (ref_valid_local == false) {
            /* couldn't sync, or sync too old -> invalidate XTAL correction */
            pthread_mutex_lock(&mx_xcorr);
            xtal_correct_ok = false;
            xtal_correct = 1.0;
            pthread_mutex_unlock(&mx_xcorr);
            init_cpt = 0;
            init_acc = 0.0;
        } else {
            if (init_cpt < XERR_INIT_AVG) {
                /* initial accumulation */
                init_acc += xtal_err_cpy;
                ++init_cpt;
            } else if (init_cpt == XERR_INIT_AVG) {
                /* initial average calculation */
                pthread_mutex_lock(&mx_xcorr);
                xtal_correct = (double)(XERR_INIT_AVG) / init_acc;
                //printf("XERR_INIT_AVG=%d, init_acc=%.15lf\n", XERR_INIT_AVG, init_acc);
                xtal_correct_ok = true;
                pthread_mutex_unlock(&mx_xcorr);
                ++init_cpt;
                // fprintf(log_file,"%.18lf,\"average\"\n", xtal_correct); // DEBUG
            } else {
                /* tracking with low-pass filter */
                x = 1 / xtal_err_cpy;
                pthread_mutex_lock(&mx_xcorr);
                xtal_correct = xtal_correct - xtal_correct/XERR_FILT_COEF + x/XERR_FILT_COEF;
                pthread_mutex_unlock(&mx_xcorr);
                // fprintf(log_file,"%.18lf,\"track\"\n", xtal_correct); // DEBUG
            }
        }
        // printf("Time ref: %s, XTAL correct: %s (%.15lf)\n", ref_valid_local?"valid":"invalid", xtal_correct_ok?"valid":"invalid", xtal_correct); // DEBUG
    }
    MSG_DEBUG(DEBUG_INFO, "INFO~ End of validation thread\n");
}

void thread_ent_dnlink(void) {
    int i, j, start; /* loop variables */
    uint8_t psize = 0, size = 0;

    DIR *dir;
    FILE *fp;
    struct dirent *ptr;
    struct stat statbuf;
    char dn_file[128]; 

    /* data buffers */
    char buff_down[512]; /* buffer to receive downstream packets */
    char dnpld[256];
    char hexpld[256];

    char txdr[5]; 
    char txpw[3]; 
    char txbw[4]; 
    char txfreq[12];
    char rxwindow[2];
    
    uint32_t uaddr;
    char addr[16];
    char txmode[8];
    char pdformat[8];

    DNLINK *entry = NULL;
    DNLINK *tmp = NULL;

    enum jit_error_e jit_result = JIT_ERROR_OK;

    while (!exit_sig && !quit_sig) {
        
        /* lookup file */
        if ((dir = opendir(DNPATH)) == NULL) {
            //MSG_DEBUG(DEBUG_ERROR, "ERROR~ [push]open sending path error\n");
            wait_ms(100); 
            continue;
        }

	    while ((ptr = readdir(dir)) != NULL) {
            if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) /* current dir OR parrent dir */
                continue;

            MSG_DEBUG(DEBUG_INFO, "INFO~ [DNLK]Looking file : %s\n", ptr->d_name);

            snprintf(dn_file, sizeof(dn_file), "%s/%s", DNPATH, ptr->d_name);

            if (stat(dn_file, &statbuf) < 0) {
                MSG_DEBUG(DEBUG_ERROR, "ERROR~ [DNLK]Canot stat %s!\n", ptr->d_name);
                continue;
            }

            if ((statbuf.st_mode & S_IFMT) == S_IFREG) {
                if ((fp = fopen(dn_file, "r")) == NULL) {
                    MSG_DEBUG(DEBUG_ERROR, "ERROR~ [DNLK]Cannot open %s\n", ptr->d_name);
                    continue;
                }

                memset(buff_down, '\0', sizeof(buff_down));

                size = fread(buff_down, sizeof(char), sizeof(buff_down), fp); /* the size less than buff_down return EOF */
                fclose(fp);

                unlink(dn_file); /* delete the file */

                memset(addr, '\0', sizeof(addr));
                memset(pdformat, '\0', sizeof(pdformat));
                memset(txmode, '\0', sizeof(txmode));
                memset(hexpld, '\0', sizeof(hexpld));
                memset(dnpld, '\0', sizeof(dnpld));
                memset(txdr, '\0', sizeof(txdr));
                memset(txpw, '\0', sizeof(txpw));
                memset(txbw, '\0', sizeof(txbw));
                memset(txfreq, '\0', sizeof(txfreq));
                memset(rxwindow, '\0', sizeof(rxwindow));

                /* format:  addr,txmode,pdformat,dnpld,txpw,txbw,txdr,txfreq 
                 * 如果 dnpld 含有','时会造成后面的txpw等值出错
                **/

                for (i = 0, j = 0; i < size; i++) {
                    if (buff_down[i] == ',')
                        j++;
                }

                if (j < 3) { /* Error Format, ',' must be greater than or equal to 3*/
                    MSG_DEBUG(DEBUG_INFO, "INFO~ [DNLK]Format error: %s\n", buff_down);
                    continue;
                }

                start = 0;

                if (strcpypt(addr, buff_down, &start, size, sizeof(addr)) < 1) 
                    continue;

                if (strcpypt(txmode, buff_down, &start, size, sizeof(txmode)) < 1)
                    strcpy(txmode, "time"); 

                if (strcpypt(pdformat, buff_down, &start, size, sizeof(pdformat)) < 1)
                    strcpy(pdformat, "txt"); 

                psize = strcpypt(dnpld, buff_down, &start, size, sizeof(dnpld)); 
                if (psize < 1) continue;

                entry = (DNLINK *) malloc(sizeof(DNLINK));

                if (strcpypt(txpw, buff_down, &start, size, sizeof(txpw)) > 0) {
                    entry->txpw = atoi(txpw);
                } else
                    entry->txpw = 0;

                if (strcpypt(txbw, buff_down, &start, size, sizeof(txbw)) > 0) {
                    entry->txbw = atoi(txbw);
                } else
                    entry->txbw = 0; 

                if (strcpypt(txdr, buff_down, &start, size, sizeof(txdr)) > 0) {
                    if (!strncmp(txdr, "SF7", 3))
                        entry->txdr = DR_LORA_SF7; 
                    else if (!strncmp(txdr, "SF8", 3))
                        entry->txdr = DR_LORA_SF8; 
                    else if (!strncmp(txdr, "SF9", 3))
                        entry->txdr = DR_LORA_SF9; 
                    else if (!strncmp(txdr, "SF10", 4))
                        entry->txdr = DR_LORA_SF10; 
                    else if (!strncmp(txdr, "SF11", 4))
                        entry->txdr = DR_LORA_SF11; 
                    else if (!strncmp(txdr, "SF12", 4))
                        entry->txdr = DR_LORA_SF12; 
                    else 
                        entry->txdr = 0; 
                } else 
                    entry->txdr = 0; 

                if (strcpypt(txfreq, buff_down, &start, size, sizeof(txfreq)) > 0) {
                    i = sscanf(txfreq, "%u", &entry->txfreq);
                    if (i != 1)
                        entry->txfreq = 0;
                } else 
                    entry->txfreq = 0; 

                if (strcpypt(rxwindow, buff_down, &start, size, sizeof(rxwindow)) > 0) {
                    entry->rxwindow = atoi(rxwindow);
                    if (entry->rxwindow > 2 || entry->rxwindow < 1)
                        entry->rxwindow = 0;
                } else 
                    entry->rxwindow = 0; 

                strcpy(entry->devaddr, addr);
                if (strstr(pdformat, "hex") != NULL) { 
                    if (psize % 2) {
                        MSG_DEBUG(DEBUG_INFO, "INFO~ [DNLK] Size of hex payload invalid.\n");
                        free(entry);
                        continue;
                    }
                    hex2str((uint8_t*)dnpld, (uint8_t*)hexpld, psize);
                    psize = psize/2;
                    memcpy1(entry->payload, (uint8_t*)hexpld, psize + 1);
                } else
                    memcpy1(entry->payload, (uint8_t*)dnpld, psize + 1);
                strcpy(entry->txmode, txmode);
                strcpy(entry->pdformat, pdformat);
                entry->psize = psize;
                entry->pre = NULL;
                entry->next = NULL;
				
                MSG_DEBUG(DEBUG_INFO, 
                        "INFO~ [DNLK]devaddr:%s, txmode:%s, pdfm:%s, size:%d\n",
                        entry->devaddr, entry->txmode, entry->pdformat, entry->psize);

                if (strstr(entry->txmode, "imme") != NULL) {
                    MSG_DEBUG(DEBUG_INFO, "INFO~ [DNLK]Pending IMMEDIATE of %s\n", addr);
                    uaddr  = strtoul(addr, NULL, 16);
                    struct devinfo devinfo = { .devaddr = uaddr };
                    if (db_lookup_skey(cntx.lookupskey, (void *) &devinfo)) {
                        jit_result = custom_rx2dn(entry, &devinfo, 0, IMMEDIATE);
                        if (jit_result != JIT_ERROR_OK)  
                            MSG_DEBUG(DEBUG_ERROR, "ERROR~ [DNLK]Packet REJECTED (jit error=%d)\n", jit_result);
                    } else
                            MSG_DEBUG(DEBUG_INFO, "INFO~ [DNLK]No devaddr match, Drop the link of %s\n", addr);
                    free(entry);
                    continue;
                }

                pthread_mutex_lock(&mx_dnlink);
                j = 1;
                tmp = dn_link;
                if (tmp == NULL) {
                    dn_link = entry;
                } else {
                    while (tmp->next != NULL) {
                        tmp = tmp->next;
                        ++j;
                    }
                    entry->pre = tmp;
                    tmp->next = entry;  
                }
                if (j > MAX_DNLINK_PKTS) { /* minus 1/2 pkts */
                    MSG_DEBUG(DEBUG_INFO, "INFO~ [DNLK] Adjust dnlink.\n");
                    for (i = 0; i < j/2; ++i) {
                        tmp = dn_link;
                        dn_link = dn_link->next;
                        if (tmp != NULL)
                            free(tmp);
                    }
                }
                pthread_mutex_unlock(&mx_dnlink);
                MSG_DEBUG(DEBUG_INFO, "INFO~ [DNLK] DNLINK PENDING!(%d elems).\n", j);
            }
            wait_ms(20); /* wait for HAT send or other process */
        }
        if (closedir(dir) < 0)
            MSG_DEBUG(DEBUG_INFO, "INFO~ [DNLK] Cannot close DIR: %s\n", DNPATH);
        wait_ms(100);
    }
}

static void prepare_frame(uint8_t type, struct devinfo *devinfo, uint32_t downcnt, const uint8_t* payload, int payload_size, uint8_t* frame, int* frame_size) {
	LoRaMacHeader_t hdr;
	LoRaMacFrameCtrl_t fctrl;
	uint8_t index = 0;
	uint8_t* encpayload;
	uint32_t mic;

	/*MHDR*/
	hdr.Value = 0;
	hdr.Bits.MType = type;
	frame[index] = hdr.Value;

	/*DevAddr*/
	frame[++index] = devinfo->devaddr&0xFF;
	frame[++index] = (devinfo->devaddr>>8)&0xFF;
	frame[++index] = (devinfo->devaddr>>16)&0xFF;
	frame[++index] = (devinfo->devaddr>>24)&0xFF;

	/*FCtrl*/
	fctrl.Value = 0;
	if(type == FRAME_TYPE_DATA_UNCONFIRMED_DOWN){
		fctrl.Bits.Ack = 1;
	}
	fctrl.Bits.Adr = 1;
	frame[++index] = fctrl.Value;

	/*FCnt*/
	frame[++index] = (downcnt)&0xFF;
	frame[++index] = (downcnt>>8)&0xFF;

	/*FOpts*/
	/*Fport*/
	frame[++index] = (DNFPORT)&0xFF;

	/*encrypt the payload*/
	encpayload = malloc(sizeof(uint8_t)*payload_size);
	LoRaMacPayloadEncrypt(payload, payload_size, (DNFPORT == 0) ? devinfo->nwkskey : devinfo->appskey, devinfo->devaddr, DOWN, downcnt, encpayload);
	++index;
	memcpy(frame+index, encpayload, payload_size);
	free(encpayload);
	index += payload_size;

	/*calculate the mic*/
	LoRaMacComputeMic(frame, index, devinfo->nwkskey, devinfo->devaddr, DOWN, downcnt, &mic);
    //printf("INFO~ [MIC] %08X\n", mic);
	frame[index] = mic&0xFF;
	frame[++index] = (mic>>8)&0xFF;
	frame[++index] = (mic>>16)&0xFF;
	frame[++index] = (mic>>24)&0xFF;
	*frame_size = index + 1;
}

static DNLINK* search_dnlink(char *addr) {
    DNLINK *entry;

    /* pthread_mutex_lock(&mx_dnlink); only read link, I think no need a lock! */ 
    entry = dn_link;
    while (entry != NULL) {
        if (!strcmp(entry->devaddr, addr))
            break;
        entry = entry->next;
    }
    return entry;
}

static enum jit_error_e custom_rx2dn(DNLINK *dnelem, struct devinfo *devinfo, uint32_t us, uint8_t txmode) {
    int i, fsize = 0;
    uint8_t payloaden[MAXPAYLOAD] = {'\0'};  /* data which have decrypted */
    struct lgw_pkt_tx_s txpkt;

    struct timeval current_unix_time;
    struct timeval current_concentrator_time;
    enum jit_error_e jit_result = JIT_ERROR_OK;
    enum jit_pkt_type_e downlink_type;

    memset(&txpkt, 0, sizeof(txpkt));
    txpkt.modulation = MOD_LORA;
    txpkt.no_crc = true;

    if (dnelem->rxwindow != 1)
        txpkt.count_us = us + 2000000UL; /* rx2 window plus 2s */
    else
        txpkt.count_us = us + 1000000UL; 

    if (dnelem->txfreq > 0)
        txpkt.freq_hz = dnelem->txfreq; 
    else
        txpkt.freq_hz = rx2freq; 

    txpkt.rf_chain = 0;

    if (dnelem->txpw > 0)
        txpkt.rf_power = dnelem->txpw;
    else
        txpkt.rf_power = 20;

    if (dnelem->txdr > 0)
        txpkt.datarate = dnelem->txdr;
    else
        txpkt.datarate = rx2dr;

    if (dnelem->txbw > 0)
        txpkt.bandwidth = dnelem->txbw;
    else
        txpkt.bandwidth = rx2bw;

    txpkt.coderate = CR_LORA_4_5;
    txpkt.invert_pol = true;
    txpkt.preamble = STD_LORA_PREAMB;
    txpkt.tx_mode = txmode;
    if (txmode)
        downlink_type = JIT_PKT_TYPE_DOWNLINK_CLASS_A;
    else
        downlink_type = JIT_PKT_TYPE_DOWNLINK_CLASS_C;

    /* prepare MAC message */
    ++dwfcnt; /* how to count this counter ? */

    memset1(payloaden, '\0', sizeof(payloaden));

    prepare_frame(FRAME_TYPE_DATA_UNCONFIRMED_DOWN, devinfo, dwfcnt, (uint8_t *)dnelem->payload, dnelem->psize, payloaden, &fsize);

    memcpy1(txpkt.payload, payloaden, fsize);

    txpkt.size = fsize;

    printf("INFO~ [CUSDN]TX(%d):", fsize);
    for (i = 0; i < fsize; ++i) {
        printf("%02X", payloaden[i]);
    }
    printf("\n");

    gettimeofday(&current_unix_time, NULL);
    get_concentrator_time(&current_concentrator_time, current_unix_time);
    jit_result = jit_enqueue(&jit_queue, &current_concentrator_time, &txpkt, downlink_type);
    MSG_DEBUG(DEBUG_INFO, "INFO~ [CUSDN]DNRX2-> %s, size:%u, tmst:%u, freq:%u, txdr:%u, txbw:%u, txpw:%u.\n",
            txmode?"TIME":"IMME", txpkt.size, txpkt.count_us, txpkt.freq_hz, txpkt.datarate, txpkt.bandwidth, txpkt.rf_power);

    return jit_result;
}


void payload_deal(struct lgw_pkt_rx_s* p) {
    int i;
    char tmp[256] = {'\0'};
    char chan_path[32] = {'\0'};
    char *chan_id = NULL;
    char *chan_data = NULL;
    int id_found = 0, data_size = p->size;

    FILE *fp;

    for (i = 0; i < p->size; i++) {
        tmp[i] = p->payload[i];
    }

    if (tmp[2] == 0x00 && tmp[3] == 0x00) /* Maybe has HEADER ffff0000 */
        chan_data = &tmp[4];
    else
        chan_data = tmp;

    for (i = 0; i < 16; i++) { /* if radiohead lib then have 4 byte of RH_RF95_HEADER_LEN */
        if (tmp[i] == '<' && id_found == 0) {  /* if id_found more than 1, '<' found  more than 1 */
            chan_id = &tmp[i + 1];
            ++id_found;
        }

        if (tmp[i] == '>') { 
            tmp[i] = '\0';
            chan_data = tmp + i + 1;
            data_size = data_size - i;
            ++id_found;
        }

        if (id_found == 2) /* found channel id */ 
            break;
    }

    if (id_found == 2) 
        sprintf(chan_path, "/var/iot/channels/%s", chan_id);
    else {
        sprintf(chan_path, "/var/iot/receive/%lu", time(NULL));
    }
    
    fp = fopen(chan_path, "w+");
    if ( NULL != fp ) {

        //fwrite(chan_data, sizeof(char), data_size, fp);  
        fprintf(fp, "%s\n", chan_data);
        fflush(fp);
        fclose(fp);
    } else 
        MSG_DEBUG(DEBUG_ERROR, "ERROR~ cannot open file path: %s\n", chan_path); 
}

static void lgw_exit_fail() { 
    if (system("/usr/bin/reset_lgw.sh stop") != 0) {
        printf("ERROR~ failed to reset SX1301, check your reset_lgw.sh script\n");
    }
    output_status(0);  /* exist, reset the status */
    exit(EXIT_FAILURE);
}

static int strcpypt(char* dest, const char* src, int* start, int size, int len)
{
    int i, j;

    i = *start;
    
    while (src[i] == ' ' && i < size) {
        i++;
    }

    if ( i >= size ) return 0;

    for (j = 0; i < size; i++) {
        if (src[i] == ',') {
            i++; // skip ','
            break;
        }

        if (j == len - 1) 
            continue;

		if(src[i] != 0 && src[i] != 10 )
            dest[j++] = src[i];
    }

    *start = i;

    return j;
}
/* --- EOF ------------------------------------------------------------------ */
