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
 * \brief lora packages dispatch 
 */

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#ifdef __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif

#include <stdint.h>				/* C99 types */
#include <stdbool.h>			/* bool type */
#include <stdio.h>				/* printf, fprintf, snprintf, fopen, fputs */

#include <string.h>				/* memset */
#include <signal.h>				/* sigaction */
#include <time.h>				/* time, clock_gettime, strftime, gmtime */
#include <sys/time.h>			/* timeval */
#include <unistd.h>				/* getopt, access */
#include <stdlib.h>				/* atoi, exit */
#include <errno.h>				/* error messages */
#include <math.h>				/* modf */
#include <assert.h>

#include <sys/socket.h>			/* socket specific definitions */
#include <netinet/in.h>			/* INET constants and stuff */
#include <arpa/inet.h>			/* IP address conversion stuff */
#include <netdb.h>				/* gai_strerror */

#include <pthread.h>
#include <getopt.h>
#include <limits.h>
#include <semaphore.h>

#include "fwd.h"
#include "jitqueue.h"
#include "timersync.h"
#include "parson.h"
#include "base64.h"
#include "loragw_hal.h"
#include "loragw_gps.h"
#include "loragw_aux.h"
#include "loragw_reg.h"
#include "loragw_debug.h"
#include "gwcfg.h"
#include "ghost.h"
#include "service.h"
#include "stats.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

/* signal handling variables */
volatile bool exit_sig = false;	/* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
volatile bool quit_sig = false;	/* 1 -> application terminates without shutting down the hardware */

//LGW_LIST_HEAD_NOLOCK_STATIC(thread_list, threads_s); 
LGW_LIST_HEAD_STATIC(rxpkts_list, rxpkts_s); 

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DECLARATION ---------------------------------------- */

INIT_GW;      // initialize GW

struct lgw_atexit {
    void (*func)(void);
    int is_cleanup;
    LGW_LIST_ENTRY(lgw_atexit) list;
};

static LGW_LIST_HEAD_STATIC(atexits, lgw_atexit);

static void lgw_run_atexits(int run_cleanups)
{
    struct lgw_atexit *ae;

    LGW_LIST_LOCK(&atexits);
    while ((ae = LGW_LIST_REMOVE_HEAD(&atexits, list))) {
        if (ae->func && (!ae->is_cleanup || run_cleanups)) {
            ae->func();
        }
        lgw_free(ae);
    }
    LGW_LIST_UNLOCK(&atexits);
}

static void __lgw_unregister_atexit(void (*func)(void))
{
    struct lgw_atexit *ae;

    LGW_LIST_TRAVERSE_SAFE_BEGIN(&atexits, ae, list) {
        if (ae->func == func) {
            LGW_LIST_REMOVE_CURRENT(list);
            lgw_free(ae);
            break;
        }
    }
    LGW_LIST_TRAVERSE_SAFE_END;
}

static int register_atexit(void (*func)(void), int is_cleanup)
{
    struct lgw_atexit *ae;

    ae = lgw_calloc(1, sizeof(*ae));
    if (!ae) {
        return -1;
    }
    ae->func = func;
    ae->is_cleanup = is_cleanup;
    lgw_LIST_LOCK(&atexits);
    __lgw_unregister_atexit(func);
    lgw_LIST_INSERT_HEAD(&atexits, ae, list);
    lgw_LIST_UNLOCK(&atexits);

    return 0;
}

int lgw_register_atexit(void (*func)(void))
{
    return register_atexit(func, 0);
}

int lgw_register_cleanup(void (*func)(void))
{
    return register_atexit(func, 1);
}

void lgw_unregister_atexit(void (*func)(void))
{
    LGW_LIST_LOCK(&atexits);
    __lgw_unregister_atexit(func);
    LGW_LIST_UNLOCK(&atexits);
}

static void sig_handler(int sigio);

/* threads */
void thread_up(void);
void thread_gps(void);
void thread_valid(void);
void thread_jit(void);
void thread_timersync(void);
void thread_watchdog(void);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void usage( void )
{
    printf("~~~ Library version string~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf(" %s\n", lgw_version_info());
    printf("~~~ Available options ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf(" -h  print this help\n");
    printf(" -c <filename>  use config file other than 'global_conf.json'\n");
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}

static void sig_handler(int sigio) {
	if (sigio == SIGQUIT) {
		quit_sig = true;
	} else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
		exit_sig = true;
	}
	return;
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char *argv[]) {
	int i, ic;						/* loop variable and temporary variable for return value */
	struct sigaction sigact;	/* SIGQUIT&SIGINT&SIGTERM signal handling */

    const char* conf_fname = "/etc/lora/global_conf.json"; /* pointer to a string we won't touch */

    serv_s* serv_entry = NULL;  /* server list entry */

	/* threads */
	pthread_t thrid_up;
	pthread_t thrid_gps;
	pthread_t thrid_valid;
	pthread_t thrid_jit;
	pthread_t thrid_timersync;
	pthread_t thrid_watchdog;

    /* Parse command line options */
    while( (i = getopt( argc, argv, "hc:" )) != -1 )
    {
        switch( i )
        {
        case 'h':
            usage( );
            return EXIT_SUCCESS;
            break;

        case 'c':
            conf_fname = optarg;
            break;

        default:
            printf( "ERROR: argument parsing options, use -h option for help\n" );
            usage( );
            return EXIT_FAILURE;
        }
    }

	/* display version informations */
	MSG("*** Dragino Packet Forwarder for Lora Gateway ***\n");
	MSG("*** Lora concentrator HAL library version info ***\n%s\n***\n", lgw_version_info());

	/* display host endianness */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	MSG("INFO: Little endian host\n");
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	MSG("INFO: Big endian host\n");
#else
	MSG("INFO: Host endianness unknown\n");
#endif

	/* configure signal handling */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = sig_handler;
	sigaction(SIGQUIT, &sigact, NULL);	/* Ctrl-\ */
	sigaction(SIGINT, &sigact, NULL);	/* Ctrl-C */
	sigaction(SIGTERM, &sigact, NULL);	/* default "kill" command */
	sigaction(SIGQUIT, &sigact, NULL);	/* Ctrl-\ */

    if (access(conf_fname, R_OK) == 0) { /* if there is a global conf, parse it  */
        if (!parse_cfg(conf_fname, &GW)) {
            MSG("ERROR: [main] failed to find any configuration file named %s\n", conf_fname);
            exit(EXIT_FAILURE);
        }
    } else {
        MSG("ERROR: [main] failed to find any configuration file named %s\n", conf_fname);
        exit(EXIT_FAILURE);
    }

	/* Start GPS a.s.a.p., to allow it to lock */
    if (GW.gps.gps_tty_path[0] != '\0') { /* do not try to open GPS device if no path set */
        i = lgw_gps_enable(gps_tty_path, "ubx7", 0, &GW.gps.gps_tty_fd); /* HAL only supports u-blox 7 for now */
        if (i != LGW_GPS_SUCCESS) {
            printf("WARNING: [main] impossible to open %s for GPS sync (check permissions)\n", GW.gps.gps_tty_path);
            GW.gps.gps_enabled = false;
            GW.gps.gps_ref_valid = false;
        } else {
            printf("INFO: [main] TTY port %s open for GPS synchronization\n", GW.gps.gps_tty_path);
            GW.gps.gps_enabled = true;
            GW.gps.gps_ref_valid = false;
        }
    }

	/* get timezone info */
	tzset();

    /* starting ghost service */
	if (GW.cfg.ghoststream_enabled == true) {
        ghost_start(GW.cfg.ghost_addr, GW.cfg.ghost_port, GW.gps.reference_coord, GW.info.gateway_id);
        lgw_register_atexit(ghost_stop);
        MSG("INFO: [main] Ghost listener started, ghost packets can now be received.\n");
    }

	/* starting the concentrator */
	if (GW.cfg.radiostream_enabled == true) {
		MSG("INFO: [main] Starting the concentrator\n");
        if (system("/usr/bin/reset_lgw.sh start") != 0) {
            MSG("ERROR: [main] failed to start SX1302, Please start again!\n");
            exit(EXIT_FAILURE);
        } 
		i = lgw_start();
		if (i == LGW_HAL_SUCCESS) {
			MSG("INFO: [main] concentrator started, radio packets can now be received.\n");
		} else {
			MSG("ERROR: [main] failed to start the concentrator\n");
			exit(EXIT_FAILURE);
		}
	} else {
		MSG("WARNING: Radio is disabled, radio packets cannot be sent or received.\n");
	}

	jit_queue_init(&jit_queue);

    if (!lgw_pthread_create(&thrid_jit, NULL, (void *(*)(void *))thread_jit, NULL))
        MSG("ERROR: [main] impossible to create JIT thread\n");

	/* spawn thread to manage GPS */
	if (GW.gps.gps_enabled == true) {
	    // Timer synchronization needed for downstream ...
		if (!lgw_pthread_create(&thrid_timersync, NULL, (void *(*)(void *))thread_timersync, NULL))
			MSG("ERROR: [main] impossible to create Timer Sync thread\n");
		if (!lgw_pthread_create(&thrid_gps, NULL, (void *(*)(void *))thread_gps, NULL))
			MSG("ERROR: [main] impossible to create GPS thread\n");
		if (!pthread_create(&thrid_valid, NULL, (void *(*)(void *))thread_valid, NULL))
			MSG("ERROR: [main] impossible to create validation thread\n");
	}

	/* spawn thread for watchdog */
	if (GW.cfg.wd_enabled == true) {
        GW.cfg.last_loop = time(NULL);
		if (!lgw_pthread_create(&thrid_watchdog, NULL, (void *(*)(void *))thread_watchdog, NULL))
			MSG("ERROR: [main] impossible to create watchdog thread\n");
	}

	/* initialize protocol stacks */
    LGW_LIST_TRAVERSE(GW.serv_list, serv_entry, list) {
	    service_start(serv_entry);
    }

	/* main loop task : statistics transmission */
	while (!exit_sig && !quit_sig) {
		/* wait for next reporting interval */
		wait_ms(1000 * GW.cfg.time_interval);

		if (exit_sig || quit_sig) {
			break;
		}
		// Create statistics report
		stats_report();

		/* Exit strategies. */
		/* Server that are 'off-line may be a reason to exit */
		/* move to semtech_transport in due time */

		/* Code of gonzalocasas to catch transient hardware failures */
		uint32_t trig_cnt_us;

		pthread_mutex_lock(&GW.hal.mx_concent);
		if (lgw_get_trigcnt(&trig_cnt_us) == LGW_HAL_SUCCESS && trig_cnt_us == 0x7E000000) {
			MSG("ERROR: [main] unintended SX1301 reset detected, terminating packet forwarder.\n");
			exit(EXIT_FAILURE);
		}
		pthread_mutex_unlock(&GW.hal.mx_concent);
	}

	/* end of the loop ready to exit */

	/* disable watchdog */
	if (GW.cfg.wd_enabled == false)
		pthread_cancel(thrid_watchdog);

	//TODO: Dit heeft nawerk nodig / This needs some more work
	pthread_cancel(thrid_jit);	/* don't wait for jit thread */

	if (GW.gps.gps_enabled == true) {
		pthread_cancel(thrid_timersync);	/* don't wait for timer sync thread */
		pthread_cancel(thrid_gps);	        /* don't wait for GPS thread */
		pthread_cancel(thrid_valid);	    /* don't wait for validation thread */
	}

    lgw_run_atexits(1);

	/* if an exit signal was received, try to quit properly */
	if (exit_sig) {
		/* stop the hardware */
		if (GW.cfg.radiostream_enabled == true) {
			i = lgw_stop();
			if (i == LGW_HAL_SUCCESS) {
                if (system("/usr/bin/reset_lgw.sh stop") != 0) {
                    MSG("ERROR: failed to stop SX1302\n");
                } else 
				    MSG("INFO: concentrator stopped successfully\n");
			} else {
				MSG("WARNING: failed to stop concentrator successfully\n");
			}
		}
	}

	MSG("INFO: Exiting packet forwarder program\n");
	exit(EXIT_SUCCESS);
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 1: RECEIVING PACKETS AND FORWARDING THEM ---------------------- */

void thread_up(void) {

	/* allocate memory for packet fetching and processing */
	struct lgw_pkt_rx_s rxpkt[NB_PKT_MAX];	/* array containing inbound packets + metadata */
	int nb_pkt;

    rxpkts_s *rxpkt_entry = NULL;
    serv_rxpkts_s *serv_rxpkt_entry = NULL;

	MSG("INFO: [up] Thread activated for all servers.\n");

	while (!exit_sig && !quit_sig) {

		/* fetch packets */
		pthread_mutex_lock(&GW.hal.mx_concent);

		if (GW.cfg.radiostream_enabled == true)
			nb_pkt = lgw_receive(NB_PKT_MAX, rxpkt);
		else
			nb_pkt = 0;

		pthread_mutex_unlock(&GW.hal.mx_concent);

		if (nb_pkt == LGW_HAL_ERROR) {
			MSG("ERROR: [up] Failed packet fetch, continue\n");
            nb_pkt = 0;
			//exit(EXIT_FAILURE);
		}

		if (GW.cfg.ghoststream_enabled == true)
			nb_pkt = ghost_get(NB_PKT_MAX - nb_pkt, &rxpkt[nb_pkt]) + nb_pkt;

		/* wait a short time if no packets, nor status report */
		if (nb_pkt == 0) {
			wait_ms(DEFAULT_FETCH_SLEEP_MS);
			continue;
		}

        rxpkt_entry = lgw_malloc(sizeof(rxpkt_entry)); // rxpkts结构体包含有一个lora_pkt_rx_s结构数组

        if (NULL == rxpkt_entry) {
            continue;
        }

        rxpkt_entry->list->next = NULL;
        rxpkt_entry->nb_pkt = nb_pkt;
        rxpkt_entry->bind = GW.serv_list.size;
        memcpy(rxpkt_entry->rxpkt, rxpkt, sizeof(struct lgw_pkt_rx_s) * nb_pkt);

        LGW_LIST_LOCK(&rxpkts_list);
        LGW_LIST_INSERT_HEAD(&rxpkts_list, rxpkt_entry, list);
        LGW_LIST_UNLOCK(&rxpkts_list);

        service_handle_rxpkt(&GW, rxpkt_entry);

        if (rxpkts_list.size > DEFAULT_RXPKTS_LIST_SIZE)  
            lgw_pthread_create_background(NULL, NULL, (void * (*)(void *))thread_rxpkt_recycle, NULL); 
	}

	MSG("INFO: End of upstream thread\n");
}

/* -------------------------------------------------------------------------- */
/* --- THREAD : recycle rxpkts --------------------- */

void thread_rxpkt_recycle(void) {
    rxpkts_s* rxpkts_entry = NULL;

    LGW_LIST_LOCK(&rxpkts_list);
    LGW_LIST_TRAVERSE_SAFE_BEGIN(&rxpkts_list, rxpkts_entry, list) {
        if (rxpkts_entry->bind == 0) {
            LGW_LIST_REMOVE_CURRENT(rxpkts_entry);
            free(rxpkts_entry);
        }
    }
    LGW_LIST_UNLOCK(&rxpkts_list);
    
}

void print_tx_status(uint8_t tx_status) {
	switch (tx_status) {
	case TX_OFF:
		MSG("INFO: [jit] lgw_status returned TX_OFF\n");
		break;
	case TX_FREE:
		MSG("INFO: [jit] lgw_status returned TX_FREE\n");
		break;
	case TX_EMITTING:
		MSG("INFO: [jit] lgw_status returned TX_EMITTING\n");
		break;
	case TX_SCHEDULED:
		MSG("INFO: [jit] lgw_status returned TX_SCHEDULED\n");
		break;
	default:
		MSG("INFO: [jit] lgw_status returned UNKNOWN (%d)\n", tx_status);
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
	enum jit_error_e jit_result;
	enum jit_pkt_type_e pkt_type;
	uint8_t tx_status;

	MSG("INFO: JIT thread activated.\n");

	while (!exit_sig && !quit_sig) {
		wait_ms(10);

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
						increment_down(BEACON_SENT);
					}

					/* check if concentrator is free for sending new packet */
					pthread_mutex_lock(&GW.hal.mx_concent);
					result = lgw_status(TX_STATUS, &tx_status);
					pthread_mutex_unlock(&GW.hal.mx_concent);
					if (result == LGW_HAL_ERROR) {
						MSG("WARNING: [jit] lgw_status failed\n");
					} else {
						if (tx_status == TX_EMITTING) {
							MSG("ERROR: concentrator is currently emitting\n");
							print_tx_status(tx_status);
							continue;
						} else if (tx_status == TX_SCHEDULED) {
							MSG("WARNING: a downlink was already scheduled, overwriting it...\n");
							print_tx_status(tx_status);
						} else {
							/* Nothing to do */
						}
					}

					/* send packet to concentrator */
					pthread_mutex_lock(&GW.hal.mx_concent);	/* may have to wait for a fetch to finish */
					result = lgw_send(pkt);
					pthread_mutex_unlock(&GW.hal.mx_concent);	/* free concentrator ASAP */
					if (result == LGW_HAL_ERROR) {
						increment_down(TX_FAIL);
						MSG("WARNING: [jit] lgw_send failed %d\n", result);
						continue;
					} else {
						increment_down(TX_OK);
						MSG_DEBUG(DEBUG_PKT_FWD, "lgw_send done: count_us=%u\n", pkt.count_us);
					}
				} else {
					MSG("ERROR: jit_dequeue failed with %d\n", jit_result);
				}
			}
		} else if (jit_result == JIT_ERROR_EMPTY) {
			/* Do nothing, it can happen */
		} else {
			MSG("ERROR: jit_peek failed with %d\n", jit_result);
		}
	}

	MSG("INFO: End of JIT thread\n");
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 4: PARSE GPS MESSAGE AND KEEP GATEWAY IN SYNC ----------------- */

static void gps_process_sync(void) {
	struct timespec gps_time;
	struct timespec utc;
	uint32_t trig_tstamp;		/* concentrator timestamp associated with PPM pulse */
	int i = lgw_gps_get(&utc, &gps_time, NULL, NULL);

	/* get GPS time for synchronization */
	if (i != LGW_GPS_SUCCESS) {
		MSG("WARNING: [gps] could not get GPS time from GPS\n");
		return;
	}

	/* get timestamp captured on PPM pulse  */
	pthread_mutex_lock(&GW.hal.mx_concent);
	i = lgw_get_trigcnt(&trig_tstamp);
	pthread_mutex_unlock(&GW.hal.mx_concent);
	if (i != LGW_HAL_SUCCESS) {
		MSG("WARNING: [gps] failed to read concentrator timestamp\n");
		return;
	}

	/* try to update time reference with the new GPS time & timestamp */
	pthread_mutex_lock(&GW.gps.mx_timeref);
	i = lgw_gps_sync(&GW.gps.time_reference_gps, trig_tstamp, utc, gps_time);
	pthread_mutex_unlock(&GW.gps.mx_timeref);
	if (i != LGW_GPS_SUCCESS) {
		MSG("WARNING: [gps] GPS out of sync, keeping previous time reference\n");
	}
}

static void gps_process_coords(void) {
	/* position variable */
	struct coord_s coord;
	struct coord_s gpserr;
	int i = lgw_gps_get(NULL, NULL, &coord, &gpserr);

	/* update gateway coordinates */
	pthread_mutex_lock(&GW.gps.mx_meas_gps);
	if (i == LGW_GPS_SUCCESS) {
		GW.gps.gps_coord_valid = true;
		GW.gps.meas_gps_coord = coord;
		GW.gps.meas_gps_err = gpserr;
		// TODO: report other GPS statistics (typ. signal quality & integrity)
	} else {
		GW.gps.gps_coord_valid = false;
	}
	pthread_mutex_unlock(&GW.gps.mx_meas_gps);
}

void thread_gps(void) {
	/* serial variables */
	char serial_buff[128];		/* buffer to receive GPS data */
	size_t wr_idx = 0;			/* pointer to end of chars in buffer */

	/* variables for PPM pulse GPS synchronization */
	enum gps_msg latest_msg;	/* keep track of latest NMEA message parsed */

	/* initialize some variables before loop */
	memset(serial_buff, 0, sizeof serial_buff);

	while (!exit_sig && !quit_sig) {
		size_t rd_idx = 0;
		size_t frame_end_idx = 0;

		/* blocking non-canonical read on serial port */
		ssize_t nb_char = read(GW.gps.gps_tty_fd, serial_buff + wr_idx, LGW_GPS_MIN_MSG_SIZE);
		if (nb_char <= 0) {
			MSG("WARNING: [gps] read() returned value %ld\n", nb_char);
			continue;
		}
		wr_idx += (size_t) nb_char;

		 /*******************************************
         * Scan buffer for UBX/NMEA sync chars and *
         * attempt to decode frame if one is found *
         *******************************************/
		while (rd_idx < wr_idx) {
			size_t frame_size = 0;

			/* Scan buffer for UBX sync char */
			if (serial_buff[rd_idx] == (char)LGW_GPS_UBX_SYNC_CHAR) {

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
						MSG("WARNING: [gps] could not get a valid message from GPS (no time)\n");
						frame_size = 0;
					} else if (latest_msg == UBX_NAV_TIMEGPS) {
						gps_process_sync();
					}
				}
			} else if (serial_buff[rd_idx] == LGW_GPS_NMEA_SYNC_CHAR) {
		        /************************
                 * Found NMEA sync char *
                 ************************/
				/* scan for NMEA end marker (LF = 0x0a) */
				char *nmea_end_ptr = memchr(&serial_buff[rd_idx], (int)0x0a, (wr_idx - rd_idx));

				if (nmea_end_ptr) {
					/* found end marker */
					frame_size = nmea_end_ptr - &serial_buff[rd_idx] + 1;
					latest_msg = lgw_parse_nmea(&serial_buff[rd_idx], frame_size);

					if (latest_msg == INVALID || latest_msg == UNKNOWN) {
						/* checksum failed */
						frame_size = 0;
					} else if (latest_msg == NMEA_GGA) {	/* Get location from GGA frames */
						gps_process_coords();
					} else if (latest_msg == NMEA_RMC) {	/* Get time/date from RMC frames */
						gps_process_sync();
					}
				}
			}

			if (frame_size > 0) {
				/* At this point message is a checksum verified frame
				   we're processed or ignored. Remove frame from buffer */
				rd_idx += frame_size;
				frame_end_idx = rd_idx;
			} else {
				rd_idx++;
			}
		}/* ...for(rd_idx = 0... */

		if (frame_end_idx) {
			/* Frames have been processed. Remove bytes to end of last processed frame */
			memcpy(serial_buff, &serial_buff[frame_end_idx], wr_idx - frame_end_idx);
			wr_idx -= frame_end_idx;
		}

		/* ...for(rd_idx = 0... */
		/* Prevent buffer overflow */
		if ((sizeof(serial_buff) - wr_idx) < LGW_GPS_MIN_MSG_SIZE) {
			memcpy(serial_buff, &serial_buff[LGW_GPS_MIN_MSG_SIZE], wr_idx - LGW_GPS_MIN_MSG_SIZE);
			wr_idx -= LGW_GPS_MIN_MSG_SIZE;
		}
	}
	MSG("INFO: End of GPS thread\n");
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

	MSG("INFO: Validation thread activated.\n");

	/* correction debug */
	// FILE * log_file = NULL;
	// time_t now_time;
	// char log_name[64];

	/* initialization */
	// time(&now_time);
	// strftime(log_name,sizeof log_name,"xtal_err_%Y%m%dT%H%M%SZ.csv",localtime(&now_time));
	// log_file = fopen(log_name, "w");
	// setbuf(log_file, NULL);
	// fprintf(log_file,"\"GW.hal.xtal_correct\",\"XERR_INIT_AVG %u XERR_FILT_COEF %u\"\n", XERR_INIT_AVG, XERR_FILT_COEF); // DEBUG

	/* main loop task */
	while (!exit_sig && !quit_sig) {
		wait_ms(1000);

		/* calculate when the time reference was last updated */
		pthread_mutex_lock(&GW.gps.mx_timeref);
		gps_ref_age = (long)difftime(time(NULL), time_reference_gps.systime);
		if ((gps_ref_age >= 0) && (gps_ref_age <= GPS_REF_MAX_AGE)) {
			/* time ref is ok, validate and  */
			GW.gps.gps_ref_valid = true;
			ref_valid_local = true;
			xtal_err_cpy = time_reference_gps.xtal_err;
		} else {
			/* time ref is too old, invalidate */
			GW.gps.gps_ref_valid = false;
			ref_valid_local = false;
		}
		pthread_mutex_unlock(&GW.gps.mx_timeref);

		/* manage XTAL correction */
		if (ref_valid_local == false) {
			/* couldn't sync, or sync too old -> invalidate XTAL correction */
			pthread_mutex_lock(&GW.hal.mx_xcorr);
			GW.hal.GW.hal.xtal_correct_ok = false;
			GW.hal.xtal_correct = 1.0;
			pthread_mutex_unlock(&GW.hal.mx_xcorr);
			init_cpt = 0;
			init_acc = 0.0;
		} else {
			if (init_cpt < XERR_INIT_AVG) {
				/* initial accumulation */
				init_acc += xtal_err_cpy;
				++init_cpt;
			} else if (init_cpt == XERR_INIT_AVG) {
				/* initial average calculation */
				pthread_mutex_lock(&GW.hal.mx_xcorr);
				GW.hal.xtal_correct = (double)(XERR_INIT_AVG) / init_acc;
				GW.hal.GW.hal.xtal_correct_ok = true;
				pthread_mutex_unlock(&GW.hal.mx_xcorr);
				++init_cpt;
				// fprintf(log_file,"%.18lf,\"average\"\n", GW.hal.xtal_correct); // DEBUG
			} else {
				/* tracking with low-pass filter */
				x = 1 / xtal_err_cpy;
				pthread_mutex_lock(&GW.hal.mx_xcorr);
				GW.hal.xtal_correct = GW.hal.xtal_correct - GW.hal.xtal_correct / XERR_FILT_COEF + x / XERR_FILT_COEF;
				pthread_mutex_unlock(&GW.hal.mx_xcorr);
				// fprintf(log_file,"%.18lf,\"track\"\n", GW.hal.xtal_correct); // DEBUG
			}
		}
		MSG_DEBUG(DEBUG_LOG, "Time ref: %s, XTAL correct: %s (%.15lf)\n", 
                ref_valid_local ? "valid" : "invalid", GW.hal.xtal_correct_ok ? "valid" : "invalid", GW.hal.xtal_correct);	// DEBUG
	}
	MSG("INFO: End of validation thread\n");
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 6: WATCHDOG TO CHECK IF THE SOFTWARE ACTUALLY FORWARDS DATA --- */

void thread_watchdog(void) {
	/* main loop task */
	while (!exit_sig && !quit_sig) {
		wait_ms(30000);
		// timestamp updated within the last 3 stat intervals? If not assume something is wrong and exit
		if ((time(NULL) - GW.cfg.last_loop) > (long int)((GW.cfg.time_interval * 3) + 5)) {
			MSG("ERROR: Watchdog timer expired!\n");
			exit(254);
		}
	}
}

/* --- EOF ------------------------------------------------------------------ */
