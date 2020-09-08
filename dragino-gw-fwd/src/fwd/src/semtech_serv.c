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
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>
#include <inttypes.h>  
#include <math.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "fwd.h"
#include "service.h"
#include "semtech_service.h"
#include "jitqueue.h"
#include "parson.h"
#include "base64.h"

#include "loragw_aux.h"

DECLARE_GW;
DECLARE_HAL;

static void semtech_pull_down(void* arg);
static void semtech_push_up(void* arg);

int semtech_start(serv_s* serv) {
    serv->net->sock_up = init_sock((char *)&serv->net->addr, (char *)&serv->net->port_up, (void*)&serv->net->push_timeout_half, sizeof(struct timeval));
    serv->net->sock_down = init_sock((char *)&serv->net->addr, (char *)&serv->net->port_down, (void*)&serv->net->pull_timeout, sizeof(struct timeval));

    if (lgw_pthread_create_detached(&serv->thread.t_up, NULL, (void *(*)(void *))semtech_push_up, serv)) {
        lgw_log(LOG_WARNING, "WARNING~ [%s] Can't create push up pthread.\n", serv->info.name);
        return -1;
    }
    if (lgw_pthread_create_detached(&serv->thread.t_down, NULL, (void *(*)(void *))semtech_pull_down, serv)) {
        lgw_log(LOG_WARNING, "WARNING~ [%s] Can't create pull down pthread.\n", serv->info.name);
        return -1;
    }

    serv->state.live = true;
    serv->state.stall_time = 0;

    return 0;
}

int semtech_stop(serv_s* serv) {
	sem_post(&serv->thread.sema);
    serv->thread.stop_sig = true;
    pthread_join(serv->thread.t_up, NULL);
    pthread_join(serv->thread.t_down, NULL);
    close(serv->net->sock_up);
    close(serv->net->sock_down);
    serv->state.live = false;
    serv->net->sock_up = -1;
    serv->net->sock_down = -1;
    return 0;
}

static void semtech_push_up(void* arg) {
    serv_s* serv = (serv_s*) arg;
    lgw_log(LOG_WARNING, "INFO~ [%s] Starting semtech_push_up thread.\n", serv->info.name);

    int i, j; /* loop variables */
    int retry;
    unsigned pkt_in_dgram; /* nb on Lora packet in the current datagram */
    //time_t t;
    /* allocate memory for packet fetching and processing */
    struct lgw_pkt_rx_s *p; /* pointer on a RX packet */

    /* local copy of GPS time reference */
    bool ref_ok = false; /* determine if GPS time reference must be used or not */
    struct tref local_ref; /* time reference used for UTC <-> timestamp conversion */

    /* data buffers */
    uint8_t buff_up[TX_BUFF_SIZE]; /* buffer to compose the upstream packet */
    int buff_index;
    uint8_t buff_ack[32]; /* buffer to receive acknowledges */

    /* protocol variables */
    uint8_t token_h; /* random token for acknowledgement matching */
    uint8_t token_l; /* random token for acknowledgement matching */

    /* ping measurement variables */
    struct timespec send_time;
    struct timespec recv_time;

    /* GPS synchronization variables */
    struct timespec pkt_utc_time;
    struct tm* x; /* broken-up UTC time */
    struct timespec pkt_gps_time;
    uint64_t pkt_gps_time_ms;

    /* mote info variables */
    uint32_t mote_addr = 0;
    uint16_t mote_fcnt = 0;
    uint8_t mote_fport = 0;

    /* pre-fill the data buffer with fixed fields */
    buff_up[0] = PROTOCOL_VERSION;
    buff_up[3] = PKT_PUSH_DATA;
    *(uint32_t *)(buff_up + 4) = GW.info.net_mac_h;
    *(uint32_t *)(buff_up + 8) = GW.info.net_mac_l;

	while (!serv->thread.stop_sig) {

        sem_wait(&serv->thread.sema);

        if (serv->rxpkt_serv == NULL)
            continue;

		if (GW.gps.gps_enabled == true) {
			pthread_mutex_lock(&GW.gps.mx_timeref);
			ref_ok = GW.gps.gps_ref_valid;
			local_ref = GW.gps.time_reference_gps;
			pthread_mutex_unlock(&GW.gps.mx_timeref);
		} else {
			ref_ok = false;
		}

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
		
		for (i = 0; i < serv->rxpkt_serv->nb_pkt; ++i) {
			p = &serv->rxpkt_serv->rxpkt[i];

			/* Get mote information from current packet (addr, fcnt) */
			/* FHDR - DevAddr */
			if (p->size >= 8) {
				mote_addr  = p->payload[1];
				mote_addr |= p->payload[2] << 8;
				mote_addr |= p->payload[3] << 16;
				mote_addr |= p->payload[4] << 24;
				/* FHDR - FCnt */
				mote_fcnt  = p->payload[6];
				mote_fcnt |= p->payload[7] << 8;

                mote_fport = p->payload[8];  /* if optslen = 0 */

			} else {
				mote_addr = 0;
				mote_fcnt = 0;
                mote_fport = 0;
			}

			/* basic packet filtering */
            pthread_mutex_lock(&serv->report->mx_report);
            serv->report->stat_up.meas_nb_rx_rcv += 1;
            switch(p->status) {
                case STAT_CRC_OK:
                    serv->report->stat_up.meas_nb_rx_ok += 1;
                    if (!serv->filter.fwd_valid_pkt) {
                        pthread_mutex_unlock(&serv->report->mx_report);
                        continue; /* skip that packet */
                    }
                    break;
                case STAT_CRC_BAD:
                    serv->report->stat_up.meas_nb_rx_bad += 1;
                    if (!serv->filter.fwd_error_pkt) {
                        pthread_mutex_unlock(&serv->report->mx_report);
                        continue; /* skip that packet */
                    }
                    break;
                case STAT_NO_CRC:
                    serv->report->stat_up.meas_nb_rx_nocrc += 1;
                    if (!serv->filter.fwd_nocrc_pkt) {
                        pthread_mutex_unlock(&serv->report->mx_report);
                        continue; /* skip that packet */
                    }
                    break;
                default:
                    lgw_log(LOG_WARNING, "WARNING~ [%s-up] received packet with unknown status %u (size %u, modulation %u, BW %u, DR %u, RSSI %.1f)\n", serv->info.name, p->status, p->size, p->modulation, p->bandwidth, p->datarate, p->rssic);
                    pthread_mutex_unlock(&serv->report->mx_report);
                    continue; /* skip that packet */
                    // exit(EXIT_FAILURE);
            }

            if (mote_addr != 0 && mote_fport != 0) {
                if (pkt_basic_filter(serv, mote_addr, mote_fport)) {
                    lgw_log(LOG_INFO, "INFO~ [%s-up] Drop a packet.\n", serv->info.name);
                    pthread_mutex_unlock(&serv->report->mx_report);
                    continue;
                }
            }

            serv->report->stat_up.meas_up_pkt_fwd += 1;
            serv->report->stat_up.meas_up_payload_byte += p->size;
            pthread_mutex_unlock(&serv->report->mx_report);
            lgw_log(LOG_INFO, "\nINFO~ [%s-up] received packages from mote: %08X (fcnt=%u)\n", serv->info.name, mote_addr, mote_fcnt);

			/* Start of packet, add inter-packet separator if necessary */
			if (pkt_in_dgram == 0) {
				buff_up[buff_index] = '{';
				++buff_index;
			} else {
				buff_up[buff_index] = ',';
				buff_up[buff_index+1] = '{';
				buff_index += 2;
			}

			/* JSON rxpk frame format version, 8 useful chars */
			j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, "\"jver\":%d", PROTOCOL_JSON_RXPK_FRAME_FORMAT);
			if (j > 0) {
				buff_index += j;
			} else {
				lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				buff_index = 21; /* skip that packet */
                continue;
			}

			/* RAW timestamp, 8-17 useful chars */
			j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"tmst\":%u", p->count_us);
			if (j > 0) {
				buff_index += j;
			} else {
				lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				buff_index = 21; /* skip that packet */
				continue;
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
						lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				        buff_index = 21; /* skip that packet */
						continue;
					}
				}
				/* convert packet timestamp to GPS absolute time */
				j = lgw_cnt2gps(local_ref, p->count_us, &pkt_gps_time);
				if (j == LGW_GPS_SUCCESS) {
					pkt_gps_time_ms = pkt_gps_time.tv_sec * 1E3 + pkt_gps_time.tv_nsec / 1E6;
					j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"tmms\":%" PRIu64 "", pkt_gps_time_ms); /* GPS time in milliseconds since 06.Jan.1980 */
					if (j > 0) {
						buff_index += j;
					} else {
						lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				        buff_index = 21; /* skip that packet */
                        continue;
						//pthread_exit(NULL);
					}
				}
			}

			/* Packet concentrator channel, RF chain & RX frequency, 34-36 useful chars */
			j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"chan\":%1u,\"rfch\":%1u,\"freq\":%.6lf,\"mid\":%2u", p->if_chain, p->rf_chain, ((double)p->freq_hz / 1e6), p->modem_id);
			if (j > 0) {
				buff_index += j;
			} else {
				lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				buff_index = 21; /* skip that packet */
                continue;
				//exit(EXIT_FAILURE);
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
					lgw_log(LOG_ERROR, "ERROR~ [%s-up] received packet with unknown status 0x%02X\n", serv->info.name, p->status);
					memcpy((void *)(buff_up + buff_index), (void *)",\"stat\":?", 9);
					buff_index += 9;
                    continue;
					//exit(EXIT_FAILURE);
			}

			/* Packet modulation, 13-14 useful chars */
			if (p->modulation == MOD_LORA) {
				memcpy((void *)(buff_up + buff_index), (void *)",\"modu\":\"LORA\"", 14);
				buff_index += 14;

				/* Lora datarate & bandwidth, 16-19 useful chars */
				switch (p->datarate) {
					case DR_LORA_SF5:
						memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF5", 12);
						buff_index += 12;
						break;
					case DR_LORA_SF6:
						memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF6", 12);
						buff_index += 12;
						break;
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
						lgw_log(LOG_ERROR, "ERROR~ [%s-up] lora packet with unknown datarate 0x%02X\n", serv->info.name, p->datarate);
						memcpy((void *)(buff_up + buff_index), (void *)",\"datr\":\"SF?", 12);
						buff_index += 12;
                        continue;
						//exit(EXIT_FAILURE);
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
						lgw_log(LOG_WARNING, "WARNING~ [%s-up] lora packet with unknown bandwidth 0x%02X\n", serv->info.name, p->bandwidth);
						memcpy((void *)(buff_up + buff_index), (void *)"BW?\"", 4);
						buff_index += 4;
                        continue;
						//exit(EXIT_FAILURE);
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
						lgw_log(LOG_WARNING, "WARNING~ [%s-up] lora packet with unknown coderate 0x%02X\n", serv->info.name, p->coderate);
						memcpy((void *)(buff_up + buff_index), (void *)",\"codr\":\"?\"", 11);
						buff_index += 11;
                        continue;
						//exit(EXIT_FAILURE);
				}

				/* Signal RSSI, payload size */
				j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"rssis\":%.0f", roundf(p->rssis));
				if (j > 0) {
					buff_index += j;
				} else {
					lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				    buff_index = 21; /* skip that packet */
                    continue;
					//exit(EXIT_FAILURE);
				}

				/* Lora SNR */
				j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"lsnr\":%.1f", p->snr);
				if (j > 0) {
					buff_index += j;
				} else {
					lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				    buff_index = 21; /* skip that packet */
                    continue;
					//exit(EXIT_FAILURE);
				}

				/* Lora frequency offset */
				j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"foff\":%d", p->freq_offset);
				if (j > 0) {
					buff_index += j;
				} else {
					lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				    buff_index = 21; /* skip that packet */
                    continue;
					//exit(EXIT_FAILURE);
				}
			} else if (p->modulation == MOD_FSK) {
				memcpy((void *)(buff_up + buff_index), (void *)",\"modu\":\"FSK\"", 13);
				buff_index += 13;

				/* FSK datarate, 11-14 useful chars */
				j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"datr\":%u", p->datarate);
				if (j > 0) {
					buff_index += j;
				} else {
					lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				    buff_index = 21; /* skip that packet */
                    continue;
					//exit(EXIT_FAILURE);
				}
			} else {
				lgw_log(LOG_ERROR, "ERROR~ [%s-up] received packet with unknown modulation 0x%02X\n", serv->info.name, p->modulation);
				buff_index = 21; /* skip that packet */
                continue;
				//exit(EXIT_FAILURE);
			}

			/* Channel RSSI, payload size, 18-23 useful chars */
			j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, ",\"rssi\":%.0f,\"size\":%u", roundf(p->rssic), p->size);
			if (j > 0) {
				buff_index += j;
			} else {
				lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 4));
				buff_index = 21; /* skip that packet */
				continue;
			}

			/* Packet base64-encoded payload, 14-350 useful chars */
			memcpy((void *)(buff_up + buff_index), (void *)",\"data\":\"", 9);
			buff_index += 9;
			j = bin_to_b64(p->payload, p->size, (char *)(buff_up + buff_index), 341); /* 255 bytes = 340 chars in b64 + null char */
			if (j>=0) {
				buff_index += j;
			} else {
				lgw_log(LOG_ERROR, "ERROR~ [%s-up] bin_to_b64 failed line %u\n", serv->info.name, (__LINE__ - 5));
				buff_index = 21; /* skip that packet */
                continue;
				//exit(EXIT_FAILURE);
			}
			buff_up[buff_index] = '"';
			++buff_index;

			/* End of packet serialization */
			buff_up[buff_index] = '}';
			++buff_index;
			++pkt_in_dgram;

		}

        /* 解绑 rxpkts */
        pthread_mutex_lock(&GW.mx_bind_lock);
        serv->rxpkt_serv->bind--;
        pthread_mutex_unlock(&GW.mx_bind_lock);

        serv->rxpkt_serv = NULL;

		/* restart fetch sequence without sending empty JSON if all packets have been filtered out */
		if (pkt_in_dgram == 0) {
            if (serv->report->report_ready == true) {
                /* need to clean up the beginning of the payload */
                buff_index -= 8; /* removes "rxpk":[ */
            } else
                /* all packet have been filtered out and no report, restart loop */
                continue;
		} else {
			/* end of packet array */
			buff_up[buff_index] = ']';
			++buff_index;
            /* add separator if needed */
            if (serv->report->report_ready == true) {
                buff_up[buff_index] = ',';
                ++buff_index;
            }
		}

        /* add status report if a new one is available */
        if (serv->report->report_ready == true) {
            pthread_mutex_lock(&serv->report->mx_report);
            serv->report->report_ready = false;
            j = snprintf((char *)(buff_up + buff_index), TX_BUFF_SIZE-buff_index, "%s", serv->report->status_report);
            pthread_mutex_unlock(&serv->report->mx_report);
            if (j > 0) {
                buff_index += j;
            } else {
                lgw_log(LOG_ERROR, "ERROR~ [%s-up] snprintf failed line %u\n", serv->info.name, (__LINE__ - 5));
                pthread_mutex_lock(&serv->report->mx_report);
                serv->report->report_ready = true;
                pthread_mutex_unlock(&serv->report->mx_report);
                buff_index -= 1;
            }
        }

		/* end of JSON datagram payload */
		buff_up[buff_index] = '}';
		++buff_index;
		buff_up[buff_index] = 0; /* add string terminator, for safety */

		lgw_log(LOG_PKT, "PKT~ \n[%s] JSON up: %s\n", serv->info.name, (char *)(buff_up + 12)); /* DEBUG: display JSON payload */

		/* send datagram to server */
        retry = 0;
        while (serv->net->sock_up == -1) {
            serv->net->sock_up = init_sock((char *)&serv->net->addr, (char *)&serv->net->port_up, (void*)&serv->net->push_timeout_half, sizeof(struct timeval));
            if (serv->net->sock_up == -1 && retry < DEFAULT_TRY_TIMES) {
                lgw_log(LOG_WARNING, "WARNING~ [%s-up] make socket init error, try again!\n", serv->info.name);
                wait_ms(DEFAULT_FETCH_SLEEP_MS);
                retry++;
                continue;
            } else
                break;
        }

        if (serv->net->sock_up == -1)  // drop 
            continue;

		send(serv->net->sock_up, (void *)buff_up, buff_index, 0);
		clock_gettime(CLOCK_MONOTONIC, &send_time);

		pthread_mutex_lock(&serv->report->mx_report);
		serv->report->stat_up.meas_up_dgram_sent += 1;
		serv->report->stat_up.meas_up_network_byte += buff_index;

		/* wait for acknowledge (in 2 times, to catch extra packets) */
		for (i=0; i<2; ++i) {
			j = recv(serv->net->sock_up, (void *)buff_ack, sizeof buff_ack, 0);
			clock_gettime(CLOCK_MONOTONIC, &recv_time);
			if (j == -1) {
				if (errno == EAGAIN) { /* timeout */
					continue;
				} else { /* server connection error */
					break;
				}
			} else if ((j < 4) || (buff_ack[0] != PROTOCOL_VERSION) || (buff_ack[3] != PKT_PUSH_ACK)) {
				//lgw_log(LOG_ERROR, "WARNING~ [up] ignored invalid non-ACL packet\n");
				continue;
			} else if ((buff_ack[1] != token_h) || (buff_ack[2] != token_l)) {
				//lgw_log(LOG_ERROR, "WARNING~ [up] ignored out-of sync ACK packet\n");
				continue;
			} else {
				lgw_log(LOG_INFO, "INFO~ [%s-up] PUSH_ACK received in %i ms\n", serv->info.name, (int)(1000 * difftimespec(recv_time, send_time)));
                time(&serv->state.contact);
				serv->report->stat_up.meas_up_ack_rcv += 1;
				break;
			}
		}
		pthread_mutex_unlock(&serv->report->mx_report);
	}
	lgw_log(LOG_INFO, "\nINFO~ [%s-up] End of semtech upstream thread\n", serv->info.name);
}

/* -------------------------------------------------------------------------- */
/* --- THREAD 2: POLLING SERVER AND ENQUEUING PACKETS IN JIT QUEUE ---------- */

static void semtech_pull_down(void* arg) {
    serv_s* serv = (serv_s*) arg;
    lgw_log(LOG_WARNING, "INFO~ [%s] Starting semtech_push_down thread.\n", serv->info.name);

    int i; /* loop variables */
    int retry;

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

    /* beacon data fields, byte 0 is Least Significant Byte */
    int32_t field_latitude; /* 3 bytes, derived from reference latitude */
    int32_t field_longitude; /* 3 bytes, derived from reference longitude */
    uint16_t field_crc1, field_crc2;

    /* auto-quit variable */
    uint32_t autoquit_cnt = 0; /* count the number of PULL_DATA sent since the latest PULL_ACK */

    /* Just In Time downlink */
    uint32_t current_concentrator_time;
    enum jit_error_e jit_result = JIT_ERROR_OK;
    enum jit_pkt_type_e downlink_type;
    enum jit_error_e warning_result = JIT_ERROR_OK;
    int32_t warning_value = 0;
    uint8_t tx_lut_idx = 0;

    /* pre-fill the pull request buffer with fixed fields */
    buff_req[0] = PROTOCOL_VERSION;
    buff_req[3] = PKT_PULL_DATA;
    *(uint32_t *)(buff_req + 4) = GW.info.net_mac_h;
    *(uint32_t *)(buff_req + 8) = GW.info.net_mac_l;

    /* beacon variables initialization */
    last_beacon_gps_time.tv_sec = 0;
    last_beacon_gps_time.tv_nsec = 0;

    /* beacon packet parameters */
    beacon_pkt.tx_mode = ON_GPS; /* send on PPS pulse */
    beacon_pkt.rf_chain = 0; /* antenna A */
    beacon_pkt.rf_power = GW.beacon.beacon_power;
    beacon_pkt.modulation = MOD_LORA;
    switch (GW.beacon.beacon_bw_hz) {
        case 125000:
            beacon_pkt.bandwidth = BW_125KHZ;
            break;
        case 500000:
            beacon_pkt.bandwidth = BW_500KHZ;
            break;
        default:
            /* should not happen */
            lgw_log(LOG_ERROR, "ERROR~ [%s-down] unsupported bandwidth for beacon\n", serv->info.name);
            break;
            //exit(EXIT_FAILURE);
    }
    switch (GW.beacon.beacon_datarate) {
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
            lgw_log(LOG_ERROR, "ERROR~ %s unsupported datarate for beacon\n", serv->info.name);
            //exit(EXIT_FAILURE);
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
    field_latitude = (int32_t)((GW.gps.reference_coord.lat / 90.0) * (double)(1<<23));
    if (field_latitude > (int32_t)0x007FFFFF) {
        field_latitude = (int32_t)0x007FFFFF; /* +90 N is represented as 89.99999 N */
    } else if (field_latitude < (int32_t)0xFF800000) {
        field_latitude = (int32_t)0xFF800000;
    }
    field_longitude = (int32_t)((GW.gps.reference_coord.lon / 180.0) * (double)(1<<23));
    if (field_longitude > (int32_t)0x007FFFFF) {
        field_longitude = (int32_t)0x007FFFFF; /* +180 E is represented as 179.99999 E */
    } else if (field_longitude < (int32_t)0xFF800000) {
        field_longitude = (int32_t)0xFF800000;
    }

    /* gateway specific beacon fields */
    beacon_pkt.payload[beacon_pyld_idx++] = GW.beacon.beacon_infodesc;
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

    while (!serv->thread.stop_sig) {

        /* auto-quit if the threshold is crossed */
        if ((GW.cfg.autoquit_threshold > 0) && (autoquit_cnt >= GW.cfg.autoquit_threshold)) {
            serv->thread.stop_sig = true;
            lgw_log(LOG_ERROR, "INFO~ [%s-down] the last %u PULL_DATA were not ACKed, exiting application\n", serv->info.name, GW.cfg.autoquit_threshold);
            break;
        }

        /* generate random token for request */
        token_h = (uint8_t)rand(); /* random token */
        token_l = (uint8_t)rand(); /* random token */
        buff_req[1] = token_h;
        buff_req[2] = token_l;

        retry = 0;

        while (serv->net->sock_down == -1) {
            serv->net->sock_down = init_sock((char*)&serv->net->addr, (char*)&serv->net->port_down, (void*)&serv->net->pull_timeout, sizeof(struct timeval));
            if (serv->net->sock_down == -1 && retry < DEFAULT_TRY_TIMES) {
                lgw_log(LOG_WARNING, "%s: Can't make socke of pull message, Try again!\n", serv->info.name);
                retry++;
                continue;
            } else
                break;
        }

        if (serv->net->sock_down == -1) // 如果没有连接跳过下面步骤，运行下一次循环
            continue;

        /* send PULL request and record time */
        send(serv->net->sock_down, (void *)buff_req, sizeof buff_req, 0);
        clock_gettime(CLOCK_MONOTONIC, &send_time);
        pthread_mutex_lock(&serv->report->mx_report);
        serv->report->stat_down.meas_dw_pull_sent += 1;
        pthread_mutex_unlock(&serv->report->mx_report);
        req_ack = false;
        autoquit_cnt++;

        /* listen to packets and process them until a new PULL request must be sent */
        recv_time = send_time;
        while ((int)difftimespec(recv_time, send_time) < DEFAULT_KEEPALIVE) {

            /* try to receive a datagram */
            msg_len = recv(serv->net->sock_down, (void *)buff_down, (sizeof buff_down)-1, 0);
            clock_gettime(CLOCK_MONOTONIC, &recv_time);

            /* Pre-allocate beacon slots in JiT queue, to check downlink collisions */
            beacon_loop = JIT_NUM_BEACON_IN_QUEUE - GW.tx.jit_queue[0].num_beacon;
            retry = 0;
            while (beacon_loop && (GW.beacon.beacon_period != 0)) {
                pthread_mutex_lock(&GW.gps.mx_timeref);
                /* Wait for GPS to be ready before inserting beacons in JiT queue */
                if ((GW.gps.gps_ref_valid == true) && (GW.hal.xtal_correct_ok == true)) {

                    /* compute GPS time for next beacon to come      */
                    /*   LoRaWAN: T = k*beacon_period + TBeaconDelay */
                    /*            with TBeaconDelay = [1.5ms +/- 1µs]*/
                    if (last_beacon_gps_time.tv_sec == 0) {
                        /* if no beacon has been queued, get next slot from current GPS time */
                        diff_beacon_time = GW.gps.time_reference_gps.gps.tv_sec % ((time_t)GW.beacon.beacon_period);
                        next_beacon_gps_time.tv_sec = GW.gps.time_reference_gps.gps.tv_sec +
                                                        ((time_t)GW.beacon.beacon_period - diff_beacon_time);
                    } else {
                        /* if there is already a beacon, take it as reference */
                        next_beacon_gps_time.tv_sec = last_beacon_gps_time.tv_sec + GW.beacon.beacon_period;
                    }
                    /* now we can add a beacon_period to the reference to get next beacon GPS time */
                    next_beacon_gps_time.tv_sec += (retry * GW.beacon.beacon_period);
                    next_beacon_gps_time.tv_nsec = 0;

#if DEBUG_BEACON
                    {
                    time_t time_unix;

                    time_unix = GW.gps.time_reference_gps.gps.tv_sec + UNIX_GPS_EPOCH_OFFSET;
                    lgw_log(DEBUG_BEACON, "DEBUG~ [%s] GPS-now : %s", serv->info.name, ctime(&time_unix));
                    time_unix = last_beacon_gps_time.tv_sec + UNIX_GPS_EPOCH_OFFSET;
                    lgw_log(DEBUG_BEACON, "DEBUG~ [%s] GPS-last: %s", serv->info.name, ctime(&time_unix));
                    time_unix = next_beacon_gps_time.tv_sec + UNIX_GPS_EPOCH_OFFSET;
                    lgw_log(DEBUG_BEACON, "DEBUG~ [%s] GPS-next: %s", serv->info.name, ctime(&time_unix));
                    }
#endif

                    /* convert GPS time to concentrator time, and set packet counter for JiT trigger */
                    lgw_gps2cnt(GW.gps.time_reference_gps, next_beacon_gps_time, &(beacon_pkt.count_us));
                    pthread_mutex_unlock(&GW.gps.mx_timeref);

                    /* apply frequency correction to beacon TX frequency */
                    if (GW.beacon.beacon_freq_nb > 1) {
                        beacon_chan = (next_beacon_gps_time.tv_sec / GW.beacon.beacon_period) % GW.beacon.beacon_freq_nb; /* floor rounding */
                    } else {
                        beacon_chan = 0;
                    }
                    /* Compute beacon frequency */
                    beacon_pkt.freq_hz = GW.beacon.beacon_freq_hz + (beacon_chan * GW.beacon.beacon_freq_step);

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
                    pthread_mutex_lock(&GW.hal.mx_concent);
                    HAL.lgw_get_instcnt(&current_concentrator_time);
                    pthread_mutex_unlock(&GW.hal.mx_concent);
                    jit_result = jit_enqueue(&GW.tx.jit_queue[0], current_concentrator_time, &beacon_pkt, JIT_PKT_TYPE_BEACON);
                    if (jit_result == JIT_ERROR_OK) {
                        /* update stats */
                        pthread_mutex_lock(&serv->report->mx_report);
                        serv->report->meas_nb_beacon_queued += 1;
                        pthread_mutex_unlock(&serv->report->mx_report);

                        /* One more beacon in the queue */
                        beacon_loop--;
                        retry = 0;
                        last_beacon_gps_time.tv_sec = next_beacon_gps_time.tv_sec; /* keep this beacon time as reference for next one to be programmed */

                        /* display beacon payload */
                        lgw_log(LOG_INFO, "INFO~ [%s-beacon] Beacon queued (count_us=%u, freq_hz=%u, size=%u):\n", serv->info.name, beacon_pkt.count_us, beacon_pkt.freq_hz, beacon_pkt.size);
                        lgw_log(LOG_INFO, "   => ");
                        for (i = 0; i < beacon_pkt.size; ++i) {
                            lgw_log(LOG_INFO, "%02X ", beacon_pkt.payload[i]);
                        }
                        lgw_log(LOG_INFO, "\n");
                    } else {
                        lgw_log(LOG_BEACON, "[%s-beacon]--> beacon queuing failed with %d\n", serv->info.name, jit_result);
                        /* update stats */
                        pthread_mutex_lock(&serv->report->mx_report);
                        if (jit_result != JIT_ERROR_COLLISION_BEACON) {
                            serv->report->meas_nb_beacon_rejected += 1;
                        }
                        pthread_mutex_unlock(&serv->report->mx_report);
                        /* In case previous enqueue failed, we retry one period later until it succeeds */
                        /* Note: In case the GPS has been unlocked for a while, there can be lots of retries */
                        /*       to be done from last beacon time to a new valid one */
                        retry++;
                        lgw_log(LOG_BEACON, "DEBUG~ [%s-beacon]--> beacon queuing retry=%d\n", serv->info.name, retry);
                    }
                } else {
                    pthread_mutex_unlock(&GW.gps.mx_timeref);
                    break;
                }
            }

            /* if no network message was received, got back to listening sock_down socket */
            if (msg_len == -1) {
                //lgw_log(LOG_ERROR, "WARNING~ [%s-down] recv returned %s\n", strerror(errno)); /* too verbose */
                continue;
            }

            /* if the datagram does not respect protocol, just ignore it */
            if ((msg_len < 4) || (buff_down[0] != PROTOCOL_VERSION) || ((buff_down[3] != PKT_PULL_RESP) && (buff_down[3] != PKT_PULL_ACK))) {
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] ignoring invalid packet len=%d, protocol_version=%d, id=%d\n", serv->info.name, msg_len, buff_down[0], buff_down[3]);
                continue;
            }

            /* if the datagram is an ACK, check token */
            if (buff_down[3] == PKT_PULL_ACK) {
                if ((buff_down[1] == token_h) && (buff_down[2] == token_l)) {
                    if (req_ack) {
                        lgw_log(LOG_INFO, "INFO~ [%s-down] duplicate ACK received :)\n", serv->info.name);
                    } else { /* if that packet was not already acknowledged */
                        req_ack = true;
                        autoquit_cnt = 0;
                        pthread_mutex_lock(&serv->report->mx_report);
                        serv->report->stat_down.meas_dw_ack_rcv += 1;
                        pthread_mutex_unlock(&serv->report->mx_report);
                        lgw_log(LOG_INFO, "INFO~ [%s-down] PULL_ACK received in %i ms\n", serv->info.name, (int)(1000 * difftimespec(recv_time, send_time)));
                    }
                } else { /* out-of-sync token */
                    lgw_log(LOG_INFO, "INFO~ [%s-down] received out-of-sync ACK\n", serv->info.name);
                }
                continue;
            }

            /* the datagram is a PULL_RESP */
            buff_down[msg_len] = 0; /* add string terminator, just to be safe */
            lgw_log(LOG_INFO, "INFO~ [%s-down] PULL_RESP received  - token[%d:%d] :)\n", serv->info.name, buff_down[1], buff_down[2]); /* very verbose */
            lgw_log(LOG_PKT, "\nPKT~ %s JSON down: %s\n", serv->info.name, (char *)(buff_down + 4)); /* DEBUG: display JSON payload */

            /* initialize TX struct and try to parse JSON */
            memset(&txpkt, 0, sizeof txpkt);
            root_val = json_parse_string_with_comments((const char *)(buff_down + 4)); /* JSON offset */
            if (root_val == NULL) {
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] invalid JSON, TX aborted\n", serv->info.name);
                continue;
            }

            /* look for JSON sub-object 'txpk' */
            txpk_obj = json_object_get_object(json_value_get_object(root_val), "txpk");
            if (txpk_obj == NULL) {
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] no \"txpk\" object in JSON, TX aborted\n", serv->info.name);
                json_value_free(root_val);
                continue;
            }

            /* Parse "immediate" tag, or target timestamp, or UTC time to be converted by GPS (mandatory) */
            i = json_object_get_boolean(txpk_obj,"imme"); /* can be 1 if true, 0 if false, or -1 if not a JSON boolean */
            if (i == 1) {
                /* TX procedure: send immediately */
                sent_immediate = true;
                downlink_type = JIT_PKT_TYPE_DOWNLINK_CLASS_C;
                lgw_log(LOG_INFO, "INFO~ [%s-down] a packet will be sent in \"immediate\" mode\n", serv->info.name);
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
                        lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.tmst\" or \"txpk.tmms\" objects in JSON, TX aborted\n", serv->info.name);
                        json_value_free(root_val);
                        continue;
                    }
                    if (GW.gps.gps_enabled == true) {
                        pthread_mutex_lock(&GW.gps.mx_timeref);
                        if (GW.gps.gps_ref_valid == true) {
                            local_ref = GW.gps.time_reference_gps;
                            pthread_mutex_unlock(&GW.gps.mx_timeref);
                        } else {
                            pthread_mutex_unlock(&GW.gps.mx_timeref);
                            lgw_log(LOG_WARNING, "WARNING~ [%s-down] no valid GPS time reference yet, impossible to send packet on specific GPS time, TX aborted\n", serv->info.name);
                            json_value_free(root_val);

                            /* send acknoledge datagram to server */
                            send_tx_ack(serv, buff_down[1], buff_down[2], JIT_ERROR_GPS_UNLOCKED, 0);
                            continue;
                        }
                    } else {
                        lgw_log(LOG_WARNING, "WARNING~ [%s-down] GPS disabled, impossible to send packet on specific GPS time, TX aborted\n", serv->info.name);
                        json_value_free(root_val);

                        /* send acknoledge datagram to server */
                        send_tx_ack(serv, buff_down[1], buff_down[2], JIT_ERROR_GPS_UNLOCKED, 0);
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
                        lgw_log(LOG_WARNING, "WARNING~ [%s-down] could not convert GPS time to timestamp, TX aborted\n", serv->info.name);
                        json_value_free(root_val);
                        continue;
                    } else {
                        lgw_log(LOG_INFO, "INFO~ [%s-down] a packet will be sent on timestamp value %u (calculated from GPS time)\n", serv->info.name, txpkt.count_us);
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
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.freq\" object in JSON, TX aborted\n", serv->info.name);
                json_value_free(root_val);
                continue;
            }
            txpkt.freq_hz = (uint32_t)((double)(1.0e6) * json_value_get_number(val));

            /* parse RF chain used for TX (mandatory) */
            val = json_object_get_value(txpk_obj,"rfch");
            if (val == NULL) {
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.rfch\" object in JSON, TX aborted\n", serv->info.name);
                json_value_free(root_val);
                continue;
            }
            txpkt.rf_chain = (uint8_t)json_value_get_number(val);

            /* parse TX power (optional field) */
            val = json_object_get_value(txpk_obj,"powe");
            if (val != NULL) {
                txpkt.rf_power = (int8_t)json_value_get_number(val) - GW.hal.antenna_gain;
            }

            /* Parse modulation (mandatory) */
            str = json_object_get_string(txpk_obj, "modu");
            if (str == NULL) {
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.modu\" object in JSON, TX aborted\n", serv->info.name);
                json_value_free(root_val);
                continue;
            }
            if (strcmp(str, "LORA") == 0) {
                /* Lora modulation */
                txpkt.modulation = MOD_LORA;

                /* Parse Lora spreading-factor and modulation bandwidth (mandatory) */
                str = json_object_get_string(txpk_obj, "datr");
                if (str == NULL) {
                    lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.datr\" object in JSON, TX aborted\n", serv->info.name);
                    json_value_free(root_val);
                    continue;
                }
                i = sscanf(str, "SF%2hdBW%3hd", &x0, &x1);
                if (i != 2) {
                    lgw_log(LOG_WARNING, "WARNING~ [%s-down] format error in \"txpk.datr\", TX aborted\n", serv->info.name);
                    json_value_free(root_val);
                    continue;
                }
                switch (x0) {
                    case  5: txpkt.datarate = DR_LORA_SF5;  break;
                    case  6: txpkt.datarate = DR_LORA_SF6;  break;
                    case  7: txpkt.datarate = DR_LORA_SF7;  break;
                    case  8: txpkt.datarate = DR_LORA_SF8;  break;
                    case  9: txpkt.datarate = DR_LORA_SF9;  break;
                    case 10: txpkt.datarate = DR_LORA_SF10; break;
                    case 11: txpkt.datarate = DR_LORA_SF11; break;
                    case 12: txpkt.datarate = DR_LORA_SF12; break;
                    default:
                        lgw_log(LOG_WARNING, "WARNING~ [%s-down] format error in \"txpk.datr\", invalid SF, TX aborted\n", serv->info.name);
                        json_value_free(root_val);
                        continue;
                }
                switch (x1) {
                    case 125: txpkt.bandwidth = BW_125KHZ; break;
                    case 250: txpkt.bandwidth = BW_250KHZ; break;
                    case 500: txpkt.bandwidth = BW_500KHZ; break;
                    default:
                        lgw_log(LOG_WARNING, "WARNING~ [%s-down] format error in \"txpk.datr\", invalid BW, TX aborted\n", serv->info.name);
                        json_value_free(root_val);
                        continue;
                }

                /* Parse ECC coding rate (optional field) */
                str = json_object_get_string(txpk_obj, "codr");
                if (str == NULL) {
                    lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.codr\" object in json, TX aborted\n", serv->info.name);
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
                    lgw_log(LOG_WARNING, "WARNING~ [%s-down] format error in \"txpk.codr\", TX aborted\n", serv->info.name);
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
                    lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.datr\" object in JSON, TX aborted\n", serv->info.name);
                    json_value_free(root_val);
                    continue;
                }
                txpkt.datarate = (uint32_t)(json_value_get_number(val));

                /* parse frequency deviation (mandatory) */
                val = json_object_get_value(txpk_obj,"fdev");
                if (val == NULL) {
                    lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.fdev\" object in JSON, TX aborted\n", serv->info.name);
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
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] invalid modulation in \"txpk.modu\", TX aborted\n", serv->info.name);
                json_value_free(root_val);
                continue;
            }

            /* Parse payload length (mandatory) */
            val = json_object_get_value(txpk_obj,"size");
            if (val == NULL) {
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.size\" object in JSON, TX aborted\n", serv->info.name);
                json_value_free(root_val);
                continue;
            }
            txpkt.size = (uint16_t)json_value_get_number(val);

            /* Parse payload data (mandatory) */
            str = json_object_get_string(txpk_obj, "data");
            if (str == NULL) {
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] no mandatory \"txpk.data\" object in JSON, TX aborted\n", serv->info.name);
                json_value_free(root_val);
                continue;
            }
            i = b64_to_bin(str, strlen(str), txpkt.payload, sizeof txpkt.payload);
            if (i != txpkt.size) {
                lgw_log(LOG_WARNING, "WARNING~ [%s-down] mismatch between .size and .data size once converter to binary\n", serv->info.name);
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
            pthread_mutex_lock(&serv->report->mx_report);
            serv->report->stat_down.meas_dw_dgram_rcv += 1; /* count only datagrams with no JSON errors */
            serv->report->stat_down.meas_dw_network_byte += msg_len; /* meas_dw_network_byte */
            serv->report->stat_down.meas_dw_payload_byte += txpkt.size;
            pthread_mutex_unlock(&serv->report->mx_report);

            /* reset error/warning results */
            jit_result = warning_result = JIT_ERROR_OK;
            warning_value = 0;

            /* check TX frequency before trying to queue packet */
            if ((txpkt.freq_hz < GW.tx.tx_freq_min[txpkt.rf_chain]) || (txpkt.freq_hz > GW.tx.tx_freq_max[txpkt.rf_chain])) {
                jit_result = JIT_ERROR_TX_FREQ;
                lgw_log(LOG_ERROR, "ERROR~ [%s-down] Packet REJECTED, unsupported frequency - %u (min:%u,max:%u)\n", serv->info.name, txpkt.freq_hz, GW.tx.tx_freq_min[txpkt.rf_chain], GW.tx.tx_freq_max[txpkt.rf_chain]);
            }

            /* check TX power before trying to queue packet, send a warning if not supported */
            if (jit_result == JIT_ERROR_OK) {
                i = get_tx_gain_lut_index(txpkt.rf_chain, txpkt.rf_power, &tx_lut_idx);
                if ((i < 0) || (GW.tx.txlut[txpkt.rf_chain].lut[tx_lut_idx].rf_power != txpkt.rf_power)) {
                    /* this RF power is not supported, throw a warning, and use the closest lower power supported */
                    warning_result = JIT_ERROR_TX_POWER;
                    warning_value = (int32_t)GW.tx.txlut[txpkt.rf_chain].lut[tx_lut_idx].rf_power;
                    printf("WARNING~ Requested TX power is not supported (%ddBm), actual power used: %ddBm\n", txpkt.rf_power, warning_value);
                    txpkt.rf_power = GW.tx.txlut[txpkt.rf_chain].lut[tx_lut_idx].rf_power;
                }
            }

            /* insert packet to be sent into JIT queue */
            if (jit_result == JIT_ERROR_OK) {
                pthread_mutex_lock(&GW.hal.mx_concent);
                HAL.lgw_get_instcnt(&current_concentrator_time);
                pthread_mutex_unlock(&GW.hal.mx_concent);
                jit_result = jit_enqueue(&GW.tx.jit_queue[txpkt.rf_chain], current_concentrator_time, &txpkt, downlink_type);
                if (jit_result != JIT_ERROR_OK) {
                    printf("ERROR~ Packet REJECTED (jit error=%d)\n", jit_result);
                } else {
                    /* In case of a warning having been raised before, we notify it */
                    jit_result = warning_result;
                }
                pthread_mutex_lock(&serv->report->mx_report);
                serv->report->stat_down.meas_nb_tx_requested += 1;
                pthread_mutex_unlock(&serv->report->mx_report);
            }

            /* Send acknoledge datagram to server */
            send_tx_ack(serv, buff_down[1], buff_down[2], jit_result, warning_value);
        }
    }
    lgw_log(LOG_INFO, "\nINFO~ [%s-down] End of downstream thread\n", serv->info.name);
}

