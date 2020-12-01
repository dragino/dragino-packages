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
 * \brief 
 *  Description:
*/

#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "fwd.h"
#include "service.h"
#include "gwtraf_service.h"

#include "loragw_hal.h"
#include "loragw_aux.h"

DECLARE_GW;

static void gwtraf_push_up(void* arg);

int gwtraf_start(serv_s* serv) {
    serv->net->sock_up = init_sock((char*)&serv->net->addr, (char*)&serv->net->port_up, (void*)&serv->net->push_timeout_half, sizeof(struct timeval));
    if (lgw_pthread_create_background(&serv->thread.t_up, NULL, (void *(*)(void *))gwtraf_push_up, serv)) {
        lgw_log(LOG_WARNING, "WARNING~ [%s] Can't create push up pthread.\n", serv->info.name);
        return -1;
    }
    serv->state.live = true;
    serv->state.stall_time = 0;
    lgw_db_put("service/traffic", serv->info.name, "running");
    lgw_db_put("thread", serv->info.name, "running");

    return 0;
}

void gwtraf_stop(serv_s* serv) {
	sem_post(&serv->thread.sema);
    serv->thread.stop_sig = true;
	pthread_join(serv->thread.t_up, NULL);
    serv->state.live = false;
    serv->net->sock_up = -1;
    serv->net->sock_down = -1;
    lgw_db_del("service/traffic", serv->info.name);
    lgw_db_del("thread", serv->info.name);
}

static void gwtraf_push_up(void* arg) {
    serv_s* serv = (serv_s*) arg;
	int i, j;					/* loop variables */
	int strt, retry;
    int nb_pkt = 0;
	/* allocate memory for packet fetching and processing */
	struct lgw_pkt_rx_s *p;	/* pointer on a RX packet */

    rxpkts_s* rxpkt_entry = NULL;

	/* local copy of GPS time reference */
	bool ref_ok = false;		/* determine if GPS time reference must be used or not */
	struct tref local_ref;		/* time reference used for UTC <-> timestamp conversion */

	/* data buffers */
	uint8_t buff_up[TX_BUFF_SIZE];	/* buffer to compose the upstream packet */
	int buff_index;

	/* GPS synchronization variables */
	struct timespec pkt_utc_time;
	struct tm *x;				/* broken-up UTC time */

	/* variables for identification */
	char iso_timestamp[24];
	time_t system_time;

	uint32_t mote_addr = 0;
	uint16_t mote_fcnt = 0;
    uint8_t mote_fport = 0;

    lgw_log(LOG_INFO, "INFO~ [%s] Starting gwtraf_push_up...\n", serv->info.name);
	while (!serv->thread.stop_sig) {
		// wait for data to arrive
		sem_wait(&serv->thread.sema);

        nb_pkt = get_rxpkt(serv);     //only get the first rxpkt of list
        if (nb_pkt == 0)
            continue;

        lgw_log(LOG_DEBUG, "DEBUG~ [%s] gwtraf_push_up fetch %d pachages.\n", serv->info.name, nb_pkt);

		//TODO: is this okay, can time be recruited from the local system if gps is not working?
		/* get a copy of GPS time reference (avoid 1 mutex per packet) */
		if (GW.gps.gps_enabled == true) {
			pthread_mutex_lock(&GW.gps.mx_timeref);
			ref_ok = GW.gps.gps_ref_valid;
			local_ref = GW.gps.time_reference_gps;
			pthread_mutex_unlock(&GW.gps.mx_timeref);
		} else {
			ref_ok = false;
		}

        /* start of JSON structure */
        buff_index = 0;			

        /* Make when we are, define the start of the packet array. */
        system_time = time(NULL);
        strftime(iso_timestamp, sizeof iso_timestamp, "%FT%TZ", gmtime(&system_time));
        j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index,
                                    "{\"type\":\"uplink\",\"gw\":\"%016llX\",\"time\":\"%s\", \"rxpk\":[",
                                    (long long unsigned int)GW.info.lgwm, iso_timestamp);
        if (j > 0) {
            buff_index += j;
        } else {
            continue;
        }

        strt = buff_index;

        /* serialize one Lora packet metadata and payload */
        for (i = 0; i < nb_pkt; i++) {
            p = &serv->rxpkt[i];

            buff_index = strt;

            /* basic packet filtering */
            switch (p->status) {
            case STAT_CRC_OK:
                if (!serv->filter.fwd_valid_pkt) {
                    continue;	/* skip that packet */
                }
                break;
            case STAT_CRC_BAD:
                if (!serv->filter.fwd_error_pkt) {
                    continue;	/* skip that packet */
                }
                break;
            case STAT_NO_CRC:
                if (!serv->filter.fwd_nocrc_pkt) {
                    continue;	/* skip that packet */
                }
                break;
            default:
                continue;		/* skip that packet */
            }

            if (p->size >= 8) {
                mote_addr = p->payload[1];
                mote_addr |= p->payload[2] << 8;
                mote_addr |= p->payload[3] << 16;
                mote_addr |= p->payload[4] << 24;
                mote_fcnt = p->payload[6];
                mote_fcnt |= p->payload[7] << 8;
                mote_fport = p->payload[8];  /* if optslen = 0 */
            } else {
                mote_addr = 0;
                mote_fcnt = 0;
                mote_fport = 0;
            }

            if (mote_addr != 0 && mote_fport != 0) {
                if (pkt_basic_filter(serv, mote_addr, mote_fport)) {
                    lgw_log(LOG_INFO, "INFO~ [%s-up] Drop a packet.\n", serv->info.name);
                    continue;
                }
            }

            /* RAW timestamp, 8-17 useful chars */
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, "\"tmst\":%u", p->count_us);
            if (j > 0) {
                buff_index += j;
            } else {
                lgw_log(LOG_ERROR, "ERROR: [%s-up] failed to add field \"tmst\" to the transmission buffer.\n", serv->info.name);
                continue;		/* skip that packet */
                //exit(EXIT_FAILURE);
            }

            /* Packet RX time (GPS based), 37 useful chars */
            if (ref_ok == true) {
                /* convert packet timestamp to UTC absolute time */
                j = lgw_cnt2utc(local_ref, p->count_us, &pkt_utc_time);
                if (j == LGW_GPS_SUCCESS) {
                    /* split the UNIX timestamp to its calendar components */
                    x = gmtime(&(pkt_utc_time.tv_sec));
                    j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, ",\"time\":\"%04i-%02i-%02iT%02i:%02i:%02i.%06liZ\"", (x->tm_year) + 1900, (x->tm_mon) + 1, x->tm_mday, x->tm_hour, x->tm_min, x->tm_sec, (pkt_utc_time.tv_nsec) / 1000);	/* ISO 8601 format */
                    if (j > 0) {
                        buff_index += j;
                    } else {
                        lgw_log(LOG_ERROR, "ERROR: [%s-up] failed to add field \"time\" to the transmission buffer.\n", serv->info.name);
                        continue;	/* skip that packet */
                        //exit(EXIT_FAILURE);
                    }
                }
            }

            /* Packet concentrator channel, RF chain & RX frequency, 34-36 useful chars */
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index,
                                    ",\"chan\":%1u,\"rfch\":%1u,\"freq\":%.6lf",
                                    p->if_chain, p->rf_chain, ((double)p->freq_hz / 1e6));
            if (j > 0) {
                buff_index += j;
            } else {
                continue;
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
                continue;		/* skip that packet */
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
                    continue;	/* skip that packet */
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
                    continue;	/* skip that packet */
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
                case 0:		/* treat the CR0 case (mostly false sync) */
                    memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"OFF\"", 13);
                    buff_index += 13;
                    break;
                default:
                    continue;	/* skip that packet */
                }

                /* Lora SNR, 11-13 useful chars */
                j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, ",\"lsnr\":%.1f", p->snr);
                if (j > 0) {
                    buff_index += j;
                } else {
                    continue;	/* skip that packet */
                }
            } else if (p->modulation == MOD_FSK) {
                memcpy((void *)(buff_up + buff_index), (void *)",\"modu\":\"FSK\"", 13);
                buff_index += 13;

                /* FSK datarate, 11-14 useful chars */
                j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, ",\"datr\":%u", p->datarate);
                if (j > 0) {
                    buff_index += j;
                } else {
                    continue;	/* skip that packet */
                }
            } else {
                continue;		/* skip that packet */
            }

            /* Packet RSSI, payload size, 18-23 useful chars */
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, ",\"rssi\":%.0f,\"size\":%u", p->rssis, p->size);
            if (j > 0) {
                buff_index += j;
            } else {
                continue;		/* skip that packet */
            }

            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE - buff_index, ",\"mote\":\"%08X\",\"fcnt\":%u", mote_addr, mote_fcnt);
            if (j > 0) {
                buff_index += j;
            } else {
                continue;		/* skip that packet */
            }

            /* End of packet serialization */
            buff_up[buff_index] = '}';
            ++buff_index;

            /* end of JSON datagram payload */
            buff_up[buff_index] = 0;	/* add string terminator, for safety */

            lgw_log(LOG_INFO, "\n[%s] gwtraf up: %s\n", serv->info.name, (char *)(buff_up + 12));	/* DEBUG: display JSON payload */
            retry = 0;
            while (serv->net->sock_up == -1) {
                serv->net->sock_up = init_sock((char*)&serv->net->addr, (char*)&serv->net->port_up, (void*)&serv->net->push_timeout_half, sizeof(struct timeval));
                if (serv->net->sock_up == -1 && retry < DEFAULT_TRY_TIMES) {
                    lgw_log(LOG_WARNING, "WARNING~ [%s-up] make socket init error, try again!\n", serv->info.name);
                    wait_ms(DEFAULT_FETCH_SLEEP_MS);
                    retry++;
                    continue;
                } else
                    break;
            }

            if (serv->net->sock_up == -1)
                continue;

            /* send datagram to servers sequentially */
            send(serv->net->sock_up, (void *)buff_up, buff_index, 0);
        }

	}
    lgw_log(LOG_INFO, "INFO~ [%s] END of gwtraf_push_up\n", serv->info.name);
}


