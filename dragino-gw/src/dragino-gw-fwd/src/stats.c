/*
 * stats.c - provide statistics
 *
 * Refactor by Jac
 */

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
#include <semaphore.h>

#include "dragino_gw_fwd.h"
#include "stats.h"
#include "trace.h"
#include "jitqueue.h"
#include "timersync.h"
#include "parson.h"
#include "base64.h"
#include "loragw_hal.h"
#include "loragw_gps.h"
#include "loragw_aux.h"
#include "loragw_reg.h"
#include "ttn_transport.h"
#include "connector.h"
#include "transport.h"

#define STATUS_SIZE             3072
#define TX_BUFF_SIZE    ((540 * NB_PKT_MAX) + 30 + STATUS_SIZE)

/* -------------------------------------------------------------------------- */
/* --- PUBLIC VARIABLES (GLOBAL) -------------------------------------------- */


extern bool fwd_valid_pkt;
extern bool fwd_error_pkt;
extern bool fwd_nocrc_pkt;

extern struct coord_s reference_coord;

extern bool beacon_enabled;
extern bool logger_enabled;
extern bool gps_enabled;
extern bool beacon_enabled;
extern char stat_file[];

extern char platform[];
extern char email[];
extern char description[];

extern bool gps_ref_valid;
extern bool gps_active;
extern bool gps_coord_valid;
extern bool gps_fake_enable;
extern struct tref time_reference_gps;

extern pthread_mutex_t mx_meas_gps;

extern struct coord_s meas_gps_coord;

extern pthread_mutex_t mx_stat_rep;

extern struct jit_queue_s jit_queue;

time_t startup_time;

void stats_init(Stat_down *stat_down, Stat_up *stat_up) {
	memset(stat_down 0, sizeof(Stat_down));
	memset(stat_up, 0, sizeof(Stat_up));
	startup_time = time(NULL);
}

void increment_down(Server *server, enum stats_down type) {
	pthread_mutex_lock(&(mserver->stat_down.mx_meas_up));
	switch (type) {
	case TX_OK:
		server->stat_down.meas_nb_tx_ok++;
		break;
	case TX_FAIL:
		server->stat_down.meas_nb_tx_fail++;
		break;
	case TX_REQUESTED:
		server->stat_down.meas_nb_tx_requested++;
		break;
	case TX_REJ_COLL_PACKET:
		server->stat_down.meas_nb_tx_rejected_collision_packet++;
		break;
	case TX_REJ_COLL_BEACON:
		server->stat_down.meas_nb_tx_rejected_collision_beacon++;
		break;
	case TX_REJ_TOO_LATE:
		server->stat_down.meas_nb_tx_rejected_too_late++;
		break;
	case TX_REJ_TOO_EARLY:
		server->stat_down.meas_nb_tx_rejected_too_early++;
		break;
	case BEACON_QUEUED:
		server->stat_down.meas_nb_beacon_queued++;
		break;
	case BEACON_SENT:
		server->stat_down.meas_nb_beacon_sent++;
		break;
	case BEACON_REJECTED:
		server->stat_down.meas_nb_beacon_rejected++;
		break;
	}
	pthread_mutex_unlock(&(server->stat_down.mx_meas_dw));
}

void increment_up(Server *server, enum stats_up type) {
	pthread_mutex_lock(&(server->stat_up.mx_meas_up));
	switch (type) {
	case RX_RCV:
		server->stat_up.meas_nb_rx_rcv++;
		break;
	case RX_OK:
		server->stat_up.meas_nb_rx_ok++;
		break;
	case RX_BAD:
		server->stat_up.meas_nb_rx_bad++;
		break;
	case RX_NOCRC:
		server->stat_up.meas_nb_rx_nocrc++;
		break;
	case PKT_FWD:
		server->stat_up.meas_up_pkt_fwd++;
	}
	pthread_mutex_unlock(&(server->stat_up.mx_meas_up));
}

void stats_data_up(Server *server) {
	int i;
	struct lgw_pkt_rx_s *p;		/* pointer on a RX packet */
	uint32_t mote_addr = 0;
	uint16_t mote_fcnt = 0;

    if (NULL == server->rxpkts->first) {  /* rxpkts head */
        return;
    }

	for (i = 0; i < nb_pkt; ++i) {
		p = &server->rxpkts->first->rxpkt[i]; 

		/* Get mote information from current packet (addr, fcnt) */
		/* FHDR - DevAddr */
		mote_addr = p->payload[1];
		mote_addr |= p->payload[2] << 8;
		mote_addr |= p->payload[3] << 16;
		mote_addr |= p->payload[4] << 24;
		/* FHDR - FCnt */
		mote_fcnt = p->payload[6];
		mote_fcnt |= p->payload[7] << 8;
        /* Fport */

		/* Basic sanity check for USB connected interfaces */
		/*if (mote_addr == 0) continue; */

		/* Additional sanity check */
		if (p->status == STAT_UNDEFINED || p->modulation == MOD_UNDEFINED ||
			p->bandwidth == BW_UNDEFINED || p->datarate == DR_UNDEFINED) {
			/* signal invalid packet to transmission stages */
			/* removing the packet from the 'list' would be better but this works for now */
			LOGGER("WARNING: [stats] received packet with unknown status %u (size %u, modulation %u, BW %u, DR %u, RSSI %.1f)\n", p->status, p->size, p->modulation, p->bandwidth, p->datarate, p->rssi);
			p->status = STAT_UNDEFINED;
			continue;
		}

        /* filter by server */
        if (mote_filter(server->server_name, fport, mote_addr)) {
            continue;
        }

		increment_up(server, RX_RCV);

		/* basic packet filtering */
		switch (p->status) {
		case STAT_CRC_OK:
			increment_up(server, RX_OK);
			LOGGER("INFO: [stats] received packet with valid CRC from mote: %08X (fcnt=%u)\n", mote_addr, mote_fcnt);
			if (!fwd_valid_pkt) {
				continue;		/* skip that packet */
			}
			break;
		case STAT_CRC_BAD:
			increment_up(server, RX_BAD);
			LOGGER("INFO: [stats] received packet with bad CRC from mote: %08X (fcnt=%u)\n", mote_addr, mote_fcnt);
			if (!fwd_error_pkt) {
				continue;		/* skip that packet */
			}
			break;
		case STAT_NO_CRC:
			increment_up(server, RX_NOCRC);
			LOGGER("INFO: [stats] received packet without CRC from mote: %08X (fcnt=%u)\n", mote_addr, mote_fcnt);
			if (!fwd_nocrc_pkt) {
				continue;		/* skip that packet */
			}
			break;
		default:
			// do not count packets with invalid status, probably USB interface issue anyway
			LOGGER("WARNING: [stats] received packet with unknown status %u (size %u, modulation %u, BW %u, DR %u, RSSI %.1f)\n", p->status, p->size, p->modulation, p->bandwidth, p->datarate, p->rssi);
			continue;
		}

		increment_up(server, PKT_FWD);
		pthread_mutex_lock(&(server->stat_up.mx_meas_up));
		server->stat_up.meas_up_payload_byte += p->size;
		pthread_mutex_unlock(&(server->stat_up.mx_meas_up));
	}
}

/* Function to safely calculate the moving averages */
static double moveave(double old, uint32_t utel, uint32_t unoe) {
	double dtel = utel;
	double dnoe = unoe;

	if (unoe == 0) {
		return old;
	} else if (old < 1e-3) {
		return dtel / dnoe;
	} else {
		return (old * stat_damping + (dtel / dnoe) * (100 - stat_damping)) / 100;
	}
}

void stats_report(Server *server) {
	int i;
	time_t current_time;

	/* variables to get local copies of measurements */
	uint32_t cp_nb_rx_rcv = 0;
	uint32_t cp_nb_rx_ok = 0;
	uint32_t cp_nb_rx_bad = 0;
	uint32_t cp_nb_rx_drop = 0;
	uint32_t cp_nb_rx_nocrc = 0;
	uint32_t cp_up_pkt_fwd = 0;
	uint32_t cp_up_network_byte = 0;
	uint32_t cp_up_payload_byte = 0;
	uint32_t cp_up_dgram_sent = 0;
	uint32_t cp_up_ack_rcv = 0;
	uint32_t cp_dw_pull_sent = 0;
	uint32_t cp_dw_ack_rcv = 0;
	uint32_t cp_dw_dgram_rcv = 0;
	uint32_t cp_dw_dgram_acp = 0;
	uint32_t cp_dw_network_byte = 0;
	uint32_t cp_dw_payload_byte = 0;
	uint32_t cp_nb_tx_ok = 0;
	uint32_t cp_nb_tx_fail = 0;
	uint32_t cp_nb_tx_requested = 0;
	uint32_t cp_nb_tx_rejected_collision_packet = 0;
	uint32_t cp_nb_tx_rejected_collision_beacon = 0;
	uint32_t cp_nb_tx_rejected_too_late = 0;
	uint32_t cp_nb_tx_rejected_too_early = 0;
	uint32_t cp_nb_beacon_queued = 0;
	uint32_t cp_nb_beacon_sent = 0;
	uint32_t cp_nb_beacon_rejected = 0;

	/* array to collect data per server */
	int ar_up_dgram_sent = 0;
	int ar_up_ack_rcv = 0;
	int ar_dw_pull_sent = 0;
	int ar_dw_ack_rcv = 0;
	int ar_dw_dgram_rcv = 0;
	int ar_dw_dgram_acp = 0;
	int stall_time = 0;

	/* moving averages for overall statistics */
	double move_up_rx_quality = 0;	        /* ratio of received crc_good packets over total received packets */
	double move_up_ack_quality = 0;	        /* ratio of datagram sent to datagram acknowledged to server */
	double move_dw_ack_quality = 0;	        /* ratio of pull request to pull response to server */
	double move_dw_datagram_quality = 0;	/* ratio of json correct datagrams to total datagrams received */
	double move_dw_receive_quality = 0;	    /* ratio of successfully aired data packets to total received data packets */
	double move_dw_beacon_quality = 0;	    /* ratio of successfully sent to queued for the beacon */

	/* GPS coordinates and variables */
	bool coord_ok = false;
	struct coord_s cp_gps_coord = { 0.0, 0.0, 0 };
	char gps_state[16] = "unknown";

	//struct coord_s cp_gps_err;

	/* statistics variable */
	char *stat_file_tmp = ".temp_statistics";
	char stat_timestamp[24];
	char iso_timestamp[24];
	float rx_ok_ratio;
	float rx_bad_ratio;
	float rx_nocrc_ratio;
	float up_ack_ratio;
	float dw_ack_ratio;

	/* access upstream statistics, copy and reset them */
	pthread_mutex_lock(&(server->stat_up.mx_meas_up));
	cp_nb_rx_rcv = server->stat_up.meas_nb_rx_rcv;
	cp_nb_rx_ok = server->stat_up.meas_nb_rx_ok;
	cp_nb_rx_bad = server->stat_up.meas_nb_rx_bad;
	cp_nb_rx_nocrc = server->stat_up.meas_nb_rx_nocrc;
	cp_up_pkt_fwd = server->stat_up.meas_up_pkt_fwd;
	cp_up_network_byte = server->stat_up.meas_up_network_byte;
	cp_up_payload_byte = server->stat_up.meas_up_payload_byte;

	cp_nb_rx_drop = cp_nb_rx_rcv - cp_nb_rx_ok - cp_nb_rx_bad - cp_nb_rx_nocrc;

	cp_up_dgram_sent =  meas_up_dgram_sent;
	cp_up_ack_rcv =  meas_up_ack_rcv;

	server->stat_up.meas_nb_rx_rcv = 0;
	server->stat_up.meas_nb_rx_ok = 0;
	server->stat_up.meas_nb_rx_bad = 0;
	server->stat_up.meas_nb_rx_nocrc = 0;
	server->stat_up.meas_up_pkt_fwd = 0;
	server->stat_up.meas_up_network_byte = 0;
	server->stat_up.meas_up_payload_byte = 0;

	/* get timestamp for statistics (must be done inside the lock) */
	current_time = time(NULL);
	server->stall_time = (int)(current_time - servers->contact);
	pthread_mutex_unlock(&mx_meas_up);

	/* Do the math */
	strftime(stat_timestamp, sizeof stat_timestamp, "%F %T %Z", gmtime(&current_time));
	strftime(iso_timestamp, sizeof stat_timestamp, "%FT%TZ", gmtime(&current_time));

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

	cp_dw_pull_sent  = server->stat_down.meas_dw_pull_sent;
	cp_dw_ack_rcv = server->stat_down.meas_dw_ack_rcv;
	cp_dw_dgram_rcv = server->stat_down.meas_dw_dgram_rcv;
	cp_dw_dgram_acp = server->stat_down.meas_dw_dgram_acp;

	cp_dw_network_byte = meas_dw_network_byte;
	cp_dw_payload_byte = meas_dw_payload_byte;
	cp_nb_tx_ok = server->stat_down.meas_nb_tx_ok;
	cp_nb_tx_fail = server->stat_down.meas_nb_tx_fail;

	//TODO: Why were here all '+=' instead of '='?? The summed values grow unbounded and eventually overflow!
	cp_nb_tx_requested = server->stat_down.meas_nb_tx_requested;	// was +=
	cp_nb_tx_rejected_collision_packet = server->stat_down.meas_nb_tx_rejected_collision_packet;	// was +=
	cp_nb_tx_rejected_collision_beacon = server->stat_down.meas_nb_tx_rejected_collision_beacon;	// was +=
	cp_nb_tx_rejected_too_late = server->stat_down.meas_nb_tx_rejected_too_late;	// was +=
	cp_nb_tx_rejected_too_early = server->stat_down.meas_nb_tx_rejected_too_early;	// was +=
	cp_nb_beacon_queued = server->stat_down.meas_nb_beacon_queued;	// was +=
	cp_nb_beacon_sent = server->stat_down.meas_nb_beacon_sent;	// was +=
	cp_nb_beacon_rejected = server->stat_down.meas_nb_beacon_rejected;	// was +=

	server->stat_down.meas_dw_network_byte = 0;
	server->stat_down.meas_dw_payload_byte = 0;
	server->stat_down.meas_nb_tx_ok = 0;
	server->stat_down.meas_nb_tx_fail = 0;
	server->stat_down.meas_nb_tx_requested = 0;
	server->stat_down.meas_nb_tx_rejected_collision_packet = 0;
	server->stat_down.meas_nb_tx_rejected_collision_beacon = 0;
	server->stat_down.meas_nb_tx_rejected_too_late = 0;
	server->stat_down.meas_nb_tx_rejected_too_early = 0;
	server->stat_down.meas_nb_beacon_queued = 0;
	server->stat_down.meas_nb_beacon_sent = 0;
	server->stat_down.meas_nb_beacon_rejected = 0;
	pthread_mutex_unlock(&mx_meas_dw);

	if (cp_dw_pull_sent > 0) {
		dw_ack_ratio = (float)cp_dw_ack_rcv / (float)cp_dw_pull_sent;
	} else {
		dw_ack_ratio = 0.0;
	}

	/* access GPS statistics, copy them */
	if (gps_active == true) {
		pthread_mutex_lock(&mx_meas_gps);
		coord_ok = gps_coord_valid;
		cp_gps_coord = meas_gps_coord;
		//cp_gps_err    =  meas_gps_err;
		pthread_mutex_unlock(&mx_meas_gps);
	}

	/* overwrite with reference coordinates if function is enabled */
	if (gps_fake_enable == true) {
		//gps_enabled = true;
		coord_ok = true;
		cp_gps_coord = reference_coord;
	}

	/* Determine the GPS state in human understandable form */
	{
		if (gps_enabled == false)
			snprintf(gps_state, sizeof gps_state, "disabled");
		else if (gps_fake_enable == true)
			snprintf(gps_state, sizeof gps_state, "fake");
		else if (gps_active == false)
			snprintf(gps_state, sizeof gps_state, "inactive");
		else if (gps_ref_valid == false)
			snprintf(gps_state, sizeof gps_state, "searching");
		else
			snprintf(gps_state, sizeof gps_state, "locked");
	}

	/* calculate the moving averages */
	move_up_rx_quality = moveave(move_up_rx_quality, cp_nb_rx_ok, cp_nb_rx_rcv);
	move_up_ack_quality = moveave(move_up_ack_quality, cp_up_ack_rcv, cp_up_dgram_sent);
	move_dw_ack_quality = moveave(move_dw_ack_quality, cp_dw_ack_rcv, cp_dw_pull_sent);
	move_dw_datagram_quality = moveave(move_dw_datagram_quality, cp_dw_dgram_acp, cp_dw_dgram_rcv);
	move_dw_receive_quality = moveave(move_dw_receive_quality, cp_dw_ack_rcv, cp_dw_pull_sent);
	move_dw_beacon_quality = moveave(move_dw_beacon_quality, cp_nb_beacon_sent, cp_nb_beacon_queued);

	/* display a report */
	printf("\n##### [%s]%s #####\n", stat_timestamp, server->server_name);
	if (server->upstream) {
		printf("### [UPSTREAM] ###\n");
		printf("# RF packets received by concentrator: %u\n", cp_nb_rx_rcv);
		printf("# CRC_OK: %.2f%%, CRC_FAIL: %.2f%%, NO_CRC: %.2f%%\n",
			   100.0 * rx_ok_ratio, 100.0 * rx_bad_ratio,
			   100.0 * rx_nocrc_ratio);
		printf("# RF packets forwarded: %u (%u bytes)\n", cp_up_pkt_fwd, cp_up_payload_byte);
		printf("# PUSH_DATA datagrams sent: %u (%u bytes)\n", cp_up_dgram_sent, cp_up_network_byte);
		printf("# PUSH_DATA acknowledged: %.2f%%\n", 100.0 * up_ack_ratio);
	} else {
		printf("### UPSTREAM IS DISABLED! \n");
	}
	if (server->downstream) {
		printf("### [DOWNSTREAM] ###\n");
		printf("# PULL_DATA sent: %u (%.2f%% acknowledged)\n", cp_dw_pull_sent, 100.0 * dw_ack_ratio);
		printf("# PULL_RESP(onse) datagrams received: %u (%u bytes)\n", cp_dw_dgram_rcv, cp_dw_network_byte);
		printf("# RF packets sent to concentrator: %u (%u bytes)\n", (cp_nb_tx_ok + cp_nb_tx_fail), cp_dw_payload_byte);
		printf("# TX errors: %u\n", cp_nb_tx_fail);
		if (cp_nb_tx_requested != 0) {
			printf("# TX rejected (collision packet): %.2f%% (req:%u, rej:%u)\n",
				 100.0 * cp_nb_tx_rejected_collision_packet / cp_nb_tx_requested, 
                 cp_nb_tx_requested,
				 cp_nb_tx_rejected_collision_packet);
			printf("# TX rejected (collision beacon): %.2f%% (req:%u, rej:%u)\n",
				 100.0 * cp_nb_tx_rejected_collision_beacon / cp_nb_tx_requested, 
                 cp_nb_tx_requested,
				 cp_nb_tx_rejected_collision_beacon);
			printf("# TX rejected (too late): %.2f%% (req:%u, rej:%u)\n",
				   100.0 * cp_nb_tx_rejected_too_late / cp_nb_tx_requested,
				   cp_nb_tx_requested, 
                   cp_nb_tx_rejected_too_late);
			printf("# TX rejected (too early): %.2f%% (req:%u, rej:%u)\n",
				   100.0 * cp_nb_tx_rejected_too_early / cp_nb_tx_requested,
				   cp_nb_tx_requested, 
                   cp_nb_tx_rejected_too_early);
		}
	} else {
		printf("### DOWNSTREAM IS DISABLED! \n");
	}
	if (beacon_enabled == true) {
		printf("### [BEACON] ###\n");
		printf("# Packets queued: %u\n", cp_nb_beacon_queued);
		printf("# Packets sent so far: %u\n", cp_nb_beacon_sent);
		printf("# Packets rejected: %u\n", cp_nb_beacon_rejected);
	} else {
		printf("### BEACON IS DISABLED! \n");
	}
	printf("### [JIT] ###\n");
	jit_report_queue(&jit_queue);
	//TODO: this is not symmetrical. time can also be derived from other sources, fix
	if (gps_enabled == true) {
		printf("### [GPS] ###\n");
		/* no need for mutex, display is not critical */
		if (gps_fake_enable == true) {
			printf("# No time keeping possible due to fake gps.\n");
		} else if (gps_ref_valid == true) {
			printf("# Valid gps time reference (age: %li sec)\n",
				   (long)difftime(time(NULL), time_reference_gps.systime));
		} else {
			printf("# Invalid gps time reference (age: %li sec)\n",
				   (long)difftime(time(NULL), time_reference_gps.systime));
		}
		if (gps_fake_enable == true) {
			printf ("# Manual GPS coordinates: latitude %.5f, longitude %.5f, altitude %i m\n",
				    cp_gps_coord.lat, cp_gps_coord.lon, cp_gps_coord.alt);
		} else if (coord_ok == true) {
			printf("# System GPS coordinates: latitude %.5f, longitude %.5f, altitude %i m\n",
				   cp_gps_coord.lat, cp_gps_coord.lon, cp_gps_coord.alt);
		} else {
			printf("# no valid GPS coordinates available yet\n");
		}
	} else {
		printf("### GPS IS DISABLED! \n");
	}

	printf("### [PERFORMANCE] ###\n");

	if (server->upstream) {
		if (server->type == semtech && server->downstream == true) {
            printf("# Upstream radio packet quality: %.2f%%.\n", 100 * move_up_rx_quality);
            printf("# Upstream datagram acknowledgment quality for server \"%s\" is %.2f%%.\n",
					server->addr, 100 * move_up_ack_quality);
        }
		
	}

	if (server->downstream) {
		if (server->type == semtech && server->downstream == true)
			printf("# Downstream heart beat acknowledgment quality for server \"%s\" is %.2f%%.\n",
				   servers[i].addr, 100 * move_dw_ack_quality[i]);
		}
		if (server->type == semtech && server->downstream == true)
			printf("# Downstream datagram content quality for server \"%s\" is %.2f%%.\n",
				   server->addr, 100 * move_dw_datagram_quality);
		}
		if (server->type == semtech && server->downstream == true)
			printf ("# Downstream radio transmission quality for server \"%s\" is %.2f%%.\n",
				    server->addr, 100 * move_dw_receive_quality);
		}
	}
	if (beacon_enabled == true) {
		printf("# Downstream beacon transmission quality: %.2f%%.\n", 100 * move_dw_beacon_quality);
	}

	printf("### [ CONNECTIONS ] ###\n");
	transport_status(server);

	/* generate a JSON report (will be sent to server by upstream thread) */

	/* Check which format to use */
	bool semtech_format = strcmp(server->stat_format, "semtech") == 0;
	bool lorank_idee_verbose = strcmp(server->stat_format, "idee_verup") == 0;
	bool lorank_idee_concise = strcmp(server->stat_format, "idee_concise") == 0;
	bool has_stat_file = stat_file[0] != 0;

	JSON_Value *root_value_verbose = NULL;
	JSON_Object *root_object_verbose = NULL;
	JSON_Value *root_value_concise = NULL;
	JSON_Object *root_object_concise = NULL;

	if (server->statusstream == true || has_stat_file == true) {
		root_value_verbose = json_value_init_object();
		root_object_verbose = json_value_get_object(root_value_verbose);
		JSON_Value *servers_array_value = json_value_init_array();
		JSON_Array *servers_array_object = json_value_get_array(servers_array_value);

		if (server->type == semtech) {
            JSON_Value *sub_value = json_value_init_object();
            JSON_Object *sub_object = json_value_get_object(sub_value);
            json_object_set_string(sub_object, "name", server->addr);
            json_object_set_boolean(sub_object, "found", server->live == true);
            if (server->live == true)
                json_object_set_number(sub_object, "last_seen", server->stall_time);
            else
                json_object_set_string(sub_object, "last_seen", "never");
            json_array_append_value(servers_array_object, sub_value);
		}
		json_object_set_value(root_object_verbose, "servers", servers_array_value);
		json_object_set_string(root_object_verbose, "time", iso_timestamp);
		json_object_dotset_string(root_object_verbose, "device.id", server->gw_id);
		json_object_dotset_boolean(root_object_verbose, "device.up_active", upstream_enabled == true);
		json_object_dotset_boolean(root_object_verbose, "device.down_active", downstream_enabled == true);
		json_object_dotset_number(root_object_verbose, "device.latitude", cp_gps_coord.lat);
		json_object_dotset_number(root_object_verbose, "device.longitude", cp_gps_coord.lon);
		json_object_dotset_number(root_object_verbose, "device.altitude", cp_gps_coord.alt);
		json_object_dotset_number(root_object_verbose, "device.uptime", current_time - startup_time);
		json_object_dotset_string(root_object_verbose, "device.gps", gps_state);
		json_object_dotset_string(root_object_verbose, "device.platform", platform);
		json_object_dotset_string(root_object_verbose, "device.email", email);
		json_object_dotset_string(root_object_verbose, "device.description", description);
		json_object_dotset_number(root_object_verbose, "current.up_radio_packets_received", cp_nb_rx_rcv);
		json_object_dotset_number(root_object_verbose, "current.up_radio_packets_crc_good", cp_nb_rx_ok);
		json_object_dotset_number(root_object_verbose, "current.up_radio_packets_crc_bad", cp_nb_rx_bad);
		json_object_dotset_number(root_object_verbose, "current.up_radio_packets_crc_absent", cp_nb_rx_nocrc);
		json_object_dotset_number(root_object_verbose, "current.up_radio_packets_dropped", cp_nb_rx_drop);
		json_object_dotset_number(root_object_verbose, "current.up_radio_packets_forwarded", cp_up_pkt_fwd);
		json_object_dotset_number(root_object_verbose, "current.up_server_datagrams_send", cp_up_dgram_sent);
		json_object_dotset_number(root_object_verbose, "current.up_server_datagrams_acknowledged", cp_up_ack_rcv);
		json_object_dotset_number(root_object_verbose, "current.down_heartbeat_send", cp_dw_pull_sent);
		json_object_dotset_number(root_object_verbose, "current.down_heartbeat_received", cp_dw_ack_rcv);
		json_object_dotset_number(root_object_verbose, "current.down_server_datagrams_received", cp_dw_dgram_rcv);
		json_object_dotset_number(root_object_verbose, "current.down_server_datagrams_accepted", cp_dw_dgram_acp);
		json_object_dotset_number(root_object_verbose, "current.down_radio_packets_succes", cp_nb_tx_ok);
		json_object_dotset_number(root_object_verbose, "current.down_radio_packets_failure", cp_nb_tx_fail);
		json_object_dotset_number(root_object_verbose, "current.down_radio_packets_collision_packet", cp_nb_tx_rejected_collision_packet);
		json_object_dotset_number(root_object_verbose, "current.down_radio_packets_collision_beacon", cp_nb_tx_rejected_collision_beacon);
		json_object_dotset_number(root_object_verbose, "current.down_radio_packets_too_early", cp_nb_tx_rejected_too_early);
		json_object_dotset_number(root_object_verbose, "current.down_radio_packets_too_late", cp_nb_tx_rejected_too_late);
		json_object_dotset_number(root_object_verbose, "current.down_beacon_packets_queued", cp_nb_beacon_queued);
		json_object_dotset_number(root_object_verbose, "current.down_beacon_packets_send", cp_nb_beacon_sent);
		json_object_dotset_number(root_object_verbose, "current.down_beacon_packets_rejected", cp_nb_beacon_rejected);
		json_object_dotset_number(root_object_verbose, "performance.up_radio_packet_quality", move_up_rx_quality);
		json_object_dotset_double(root_object_verbose, "performance.up_server_datagram_quality", move_up_ack_quality);
		json_object_dotset_double(root_object_verbose, "performance.down_server_heartbeat_quality", move_dw_ack_quality);
		json_object_dotset_double(root_object_verbose, "performance.down_server_datagram_quality", move_dw_datagram_quality);
		json_object_dotset_double(root_object_verbose, "performance.down_radio_packet_quality", move_dw_receive_quality);
		json_object_dotset_number(root_object_verbose, "performance.down_beacon_packet_quality", move_dw_beacon_quality);
	}

	if (server->statusstream == true && lorank_idee_concise) {
		root_value_concise = json_value_init_object();
		root_object_concise = json_value_get_object(root_value_concise);
		json_object_dotset_string(root_object_concise, "dev.id", server->gw_id);
		json_object_dotset_number(root_object_concise, "dev.lat", cp_gps_coord.lat);
		json_object_dotset_number(root_object_concise, "dev.lon", cp_gps_coord.lon);
		json_object_dotset_number(root_object_concise, "dev.alt", cp_gps_coord.alt);
		json_object_dotset_number(root_object_concise, "dev.up", current_time - startup_time);
		json_object_dotset_string(root_object_concise, "dev.gps", gps_state);
		json_object_dotset_string(root_object_concise, "dev.pfrm", platform);
		json_object_dotset_string(root_object_concise, "dev.email", email);
		json_object_dotset_string(root_object_concise, "dev.desc", description);
		if (server->upstream == true) {
			json_object_dotset_number(root_object_concise, "prf.up_rf", move_up_rx_quality);
			json_object_dotset_double(root_object_concise, "prf.up_srv_dg", move_up_ack_quality);
		}
		if (downstream_enabled == true) {
			json_object_dotset_double(root_object_concise, "prf.dw_srv_hb", move_dw_ack_quality);
			json_object_dotset_double(root_object_concise, "prf.dw_srv_dg", move_dw_datagram_quality);
			json_object_dotset_double(root_object_concise, "prf.dw_rf", move_dw_receive_quality);
			json_object_dotset_number(root_object_concise, "prf.dw_bcn", move_dw_beacon_quality);
		}
	}

	if (has_stat_file == true) {
		if (json_serialize_to_file_pretty(root_value_verbose, stat_file_tmp) == JSONSuccess)
			rename(stat_file_tmp, stat_file);
	}

	if (server->statusstream == true) {
		pthread_mutex_lock(&server->mx_stat_rep);
		if (semtech_format == true) {
			if ((gps_enabled == true) && (coord_ok == true)) {
				snprintf(status_report, STATUS_SIZE,
						 "{\"stat\":{\"time\":\"%s\",\"lati\":%.5f,\"long\":%.5f,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}",
						 stat_timestamp, cp_gps_coord.lat, cp_gps_coord.lon,
						 cp_gps_coord.alt, cp_nb_rx_rcv, cp_nb_rx_ok,
						 cp_up_pkt_fwd, 100.0 * up_ack_ratio, cp_dw_dgram_rcv,
						 cp_nb_tx_ok, platform, email, description);
			} else {
				snprintf(status_report, STATUS_SIZE,
						 "{\"stat\":{\"time\":\"%s\",\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}",
						 stat_timestamp, cp_nb_rx_rcv, cp_nb_rx_ok,
						 cp_up_pkt_fwd, 100.0 * up_ack_ratio, cp_dw_dgram_rcv,
						 cp_nb_tx_ok, platform, email, description);
			}
			printf("#[%s] Semtech status report sent. \n", server->server_name);
		} else if (lorank_idee_verbose == true) {
			/* The time field is already permanently included in the packet stream, note that may be a little later. */
			json_object_remove(root_object_verbose, "time");
			json_serialize_to_buffer(root_value_verbose, status_report, STATUS_SIZE);
			printf("#[%s] Ideetron verbose status report sent. \n", server->server_name);
		} else if (lorank_idee_concise == true) {
			json_serialize_to_buffer(root_value_concise, status_report, STATUS_SIZE);
			printf("#[%s] Ideetron concise status report sent. \n", server->server_name);
		} else {
			printf("#[%s] NO status report sent (format unknown!) \n", server->server_name);
		}
		server->report_ready = true;
		pthread_mutex_unlock(&server->mx_stat_rep);
	}

	if (server->statusstream == true || has_stat_file == true)
		json_value_free(root_value_verbose);

	if (server->statusstream == true && lorank_idee_concise)
		json_value_free(root_value_concise);

	printf("#####[%s] Stat END #####\n", server->server_name);

	// Send status using TTN protocol
	transport_status_up(server, cp_nb_rx_rcv, cp_nb_rx_ok, cp_nb_tx_ok + cp_nb_tx_fail, cp_nb_tx_ok);
}

/****************************************
 * END
 */

