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
 * \brief lora packets status report
 */

#include <stdint.h>				/* C99 types */
#include <stdbool.h>			/* bool type */
#include <stdio.h>				/* printf, fprintf, snprintf, fopen, fputs */

#include <string.h>				/* memset */
#include <time.h>				/* time, clock_gettime, strftime, gmtime */
#include <sys/time.h>			/* timeval */
#include <unistd.h>				/* getopt, access */
#include <stdlib.h>				/* atoi, exit */
#include <errno.h>				/* error messages */
#include <math.h>				/* modf */
#include <assert.h>

#include <pthread.h>

#include "fwd.h"
#include "stats.h"
#include "jitqueue.h"
#include "parson.h"
#include "loragw_hal.h"
#include "loragw_gps.h"

DECLARE_GW;

static void semtech_report(serv_s *serv) {
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

	/* GPS coordinates and variables */
	bool coord_ok = false;
	struct coord_s cp_gps_coord = { 0.0, 0.0, 0 };
	char gps_state[16] = "unknown";

	//struct coord_s cp_gps_err;

	/* statistics variable */
	char stat_timestamp[24];
	char iso_timestamp[24];
	float rx_ok_ratio;
	float rx_bad_ratio;
	float rx_nocrc_ratio;
	float up_ack_ratio;
	float dw_ack_ratio;

	/* access upstream statistics, copy and reset them */
	pthread_mutex_lock(&(serv->report->mx_report));
	cp_nb_rx_rcv = serv->report->stat_up.meas_nb_rx_rcv;
	cp_nb_rx_ok = serv->report->stat_up.meas_nb_rx_ok;
	cp_nb_rx_bad = serv->report->stat_up.meas_nb_rx_bad;
	cp_nb_rx_nocrc = serv->report->stat_up.meas_nb_rx_nocrc;
	cp_up_pkt_fwd = serv->report->stat_up.meas_up_pkt_fwd;
	cp_up_network_byte = serv->report->stat_up.meas_up_network_byte;
	cp_up_payload_byte = serv->report->stat_up.meas_up_payload_byte;

	cp_nb_rx_drop = cp_nb_rx_rcv - cp_nb_rx_ok - cp_nb_rx_bad - cp_nb_rx_nocrc;

	cp_up_dgram_sent = serv->report->stat_up.meas_up_dgram_sent;
	cp_up_ack_rcv = serv->report->stat_up.meas_up_ack_rcv;

	serv->report->stat_up.meas_nb_rx_rcv = 0;
	serv->report->stat_up.meas_nb_rx_ok = 0;
	serv->report->stat_up.meas_nb_rx_bad = 0;
	serv->report->stat_up.meas_nb_rx_nocrc = 0;
	serv->report->stat_up.meas_up_pkt_fwd = 0;
	serv->report->stat_up.meas_up_network_byte = 0;
	serv->report->stat_up.meas_up_payload_byte = 0;

	/* get timestamp for statistics (must be done inside the lock) */
	current_time = time(NULL);
	serv->state.stall_time = (int)(current_time - serv->state.contact);
	pthread_mutex_unlock(&serv->report->mx_report);

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
	pthread_mutex_lock(&serv->report->mx_report);

	cp_dw_pull_sent  = serv->report->stat_down.meas_dw_pull_sent;
	cp_dw_ack_rcv = serv->report->stat_down.meas_dw_ack_rcv;
	cp_dw_dgram_rcv = serv->report->stat_down.meas_dw_dgram_rcv;
	cp_dw_dgram_acp = serv->report->stat_down.meas_dw_dgram_acp;

	cp_dw_network_byte = serv->report->stat_down.meas_dw_network_byte;
	cp_dw_payload_byte = serv->report->stat_down.meas_dw_payload_byte;
	cp_nb_tx_ok = serv->report->stat_down.meas_nb_tx_ok;
	cp_nb_tx_fail = serv->report->stat_down.meas_nb_tx_fail;

	//TODO: Why were here all '+=' instead of '='?? The summed values grow unbounded and eventually overflow!
	cp_nb_tx_requested += serv->report->stat_down.meas_nb_tx_requested;	
	cp_nb_tx_rejected_collision_packet += serv->report->stat_down.meas_nb_tx_rejected_collision_packet;	
	cp_nb_tx_rejected_collision_beacon += serv->report->stat_down.meas_nb_tx_rejected_collision_beacon;	
	cp_nb_tx_rejected_too_late += serv->report->stat_down.meas_nb_tx_rejected_too_late;	
	cp_nb_tx_rejected_too_early += serv->report->stat_down.meas_nb_tx_rejected_too_early;	
	cp_nb_beacon_queued += serv->report->meas_nb_beacon_queued;	
	cp_nb_beacon_sent += serv->report->meas_nb_beacon_sent;	
	cp_nb_beacon_rejected += serv->report->meas_nb_beacon_rejected;	

	serv->report->stat_down.meas_dw_network_byte = 0;
	serv->report->stat_down.meas_dw_payload_byte = 0;
	serv->report->stat_down.meas_nb_tx_ok = 0;
	serv->report->stat_down.meas_nb_tx_fail = 0;
	serv->report->stat_down.meas_nb_tx_requested = 0;
	serv->report->stat_down.meas_nb_tx_rejected_collision_packet = 0;
	serv->report->stat_down.meas_nb_tx_rejected_collision_beacon = 0;
	serv->report->stat_down.meas_nb_tx_rejected_too_late = 0;
	serv->report->stat_down.meas_nb_tx_rejected_too_early = 0;
	serv->report->meas_nb_beacon_queued = 0;
	serv->report->meas_nb_beacon_sent = 0;
	serv->report->meas_nb_beacon_rejected = 0;
	pthread_mutex_unlock(&serv->report->mx_report);

	if (cp_dw_pull_sent > 0) {
		dw_ack_ratio = (float)cp_dw_ack_rcv / (float)cp_dw_pull_sent;
	} else {
		dw_ack_ratio = 0.0;
	}

	/* access GPS statistics, copy them */
	if (GW.gps.gps_enabled) {
		pthread_mutex_lock(&GW.gps.mx_meas_gps);
		coord_ok = GW.gps.gps_coord_valid;
		cp_gps_coord = GW.gps.meas_gps_coord;
		pthread_mutex_unlock(&GW.gps.mx_meas_gps);
	}

	/* overwrite with reference coordinates if function is enabled */
	if (GW.gps.gps_fake_enable == true) {
		cp_gps_coord = GW.gps.reference_coord;
	}

	/* Determine the GPS state in human understandable form */
	{
		if (GW.gps.gps_enabled == false)
			snprintf(gps_state, sizeof gps_state, "disabled");
		else if (GW.gps.gps_fake_enable == true)
			snprintf(gps_state, sizeof gps_state, "fake");
		else if (GW.gps.gps_ref_valid == false)
			snprintf(gps_state, sizeof gps_state, "searching");
		else if (GW.gps.gps_enabled == true)
			snprintf(gps_state, sizeof gps_state, "enabled");
		else
			snprintf(gps_state, sizeof gps_state, "locked");
	}

	/* display a report */
	lgw_log(LOG_INFO, "\n#######################################################\n");
	lgw_log(LOG_INFO, "##### [%s] %s #####\n", stat_timestamp, serv->info.name);
	lgw_log(LOG_INFO, "### [UPSTREAM] ###\n");
	lgw_log(LOG_INFO, "# RF packets received by concentrator: %u\n", cp_nb_rx_rcv);
	lgw_log(LOG_INFO, "# CRC_OK: %.2f%%, CRC_FAIL: %.2f%%, NO_CRC: %.2f%%\n",
                    100.0 * rx_ok_ratio, 100.0 * rx_bad_ratio,
                    100.0 * rx_nocrc_ratio);
	lgw_log(LOG_INFO, "# RF packets forwarded: %u (%u bytes)\n", cp_up_pkt_fwd, cp_up_payload_byte);
	lgw_log(LOG_INFO, "# PUSH_DATA datagrams sent: %u (%u bytes)\n", cp_up_dgram_sent, cp_up_network_byte);
	lgw_log(LOG_INFO, "# PUSH_DATA acknowledged: %.2f%%\n", 100.0 * up_ack_ratio);

	lgw_log(LOG_INFO, "### [DOWNSTREAM] ###\n");
	lgw_log(LOG_INFO, "# PULL_DATA sent: %u (%.2f%% acknowledged)\n", cp_dw_pull_sent, 100.0 * dw_ack_ratio);
	lgw_log(LOG_INFO, "# PULL_RESP(onse) datagrams received: %u (%u bytes)\n", cp_dw_dgram_rcv, cp_dw_network_byte);
	lgw_log(LOG_INFO, "# RF packets sent to concentrator: %u (%u bytes)\n", (cp_nb_tx_ok + cp_nb_tx_fail), cp_dw_payload_byte);
	lgw_log(LOG_INFO, "# TX errors: %u\n", cp_nb_tx_fail);

	if (cp_nb_tx_requested != 0) {
        lgw_log(LOG_INFO, "# TX rejected (collision packet): %.2f%% (req:%u, rej:%u)\n",
                   100.0 * cp_nb_tx_rejected_collision_packet / cp_nb_tx_requested, 
                   cp_nb_tx_requested,
                   cp_nb_tx_rejected_collision_packet);
        lgw_log(LOG_INFO, "# TX rejected (collision beacon): %.2f%% (req:%u, rej:%u)\n",
                   100.0 * cp_nb_tx_rejected_collision_beacon / cp_nb_tx_requested, 
                   cp_nb_tx_requested,
                   cp_nb_tx_rejected_collision_beacon);
        lgw_log(LOG_INFO, "# TX rejected (too late): %.2f%% (req:%u, rej:%u)\n",
                   100.0 * cp_nb_tx_rejected_too_late / cp_nb_tx_requested,
                   cp_nb_tx_requested, 
                   cp_nb_tx_rejected_too_late);
        lgw_log(LOG_INFO, "# TX rejected (too early): %.2f%% (req:%u, rej:%u)\n",
                   100.0 * cp_nb_tx_rejected_too_early / cp_nb_tx_requested,
                   cp_nb_tx_requested, 
                   cp_nb_tx_rejected_too_early);
	}

	lgw_log(LOG_INFO, "### [BEACON] ###\n");
	lgw_log(LOG_INFO, "# Packets queued: %u\n", cp_nb_beacon_queued);
	lgw_log(LOG_INFO, "# Packets sent so far: %u\n", cp_nb_beacon_sent);
	lgw_log(LOG_INFO, "# Packets rejected: %u\n", cp_nb_beacon_rejected);

	lgw_log(LOG_INFO, "### [JIT] ###\n");
    jit_print_queue (&GW.tx.jit_queue[0], false, LOG_JIT);
    lgw_log(LOG_INFO, "----------------\n");
    jit_print_queue (&GW.tx.jit_queue[1], false, LOG_JIT);

    lgw_log(LOG_INFO, "### [GPS] ###\n");
	//TODO: this is not symmetrical. time can also be derived from other sources, fix
	if (GW.gps.gps_enabled == true) {
		lgw_log(LOG_INFO, "### [GPS] ###\n");
		/* no need for mutex, display is not critical */
		if (GW.gps.gps_ref_valid == true) {
			lgw_log(LOG_INFO, "# Valid gps time reference (age: %li sec)\n", (long)difftime(time(NULL), GW.gps.time_reference_gps.systime));
		} else {
			lgw_log(LOG_INFO, "# Invalid gps time reference (age: %li sec)\n", (long)difftime(time(NULL), GW.gps.time_reference_gps.systime));
		}

		if (coord_ok) {
			lgw_log(LOG_INFO, "# System GPS coordinates: latitude %.5f, longitude %.5f, altitude %i m\n",
				   cp_gps_coord.lat, cp_gps_coord.lon, cp_gps_coord.alt);
		} else {
			lgw_log(LOG_INFO, "# no valid GPS coordinates available yet\n");
		}
    } else if (GW.gps.gps_fake_enable == true) {
        lgw_log(LOG_INFO, "# GPS *FAKE* coordinates: latitude %.5f, longitude %.5f, altitude %i m\n", cp_gps_coord.lat, cp_gps_coord.lon, cp_gps_coord.alt);
	} else {
		lgw_log(LOG_INFO, "### GPS IS DISABLED! \n");
	}

	/* generate a JSON report (will be sent to serv by upstream thread) */

	/* Check which format to use */
	bool semtech_format = strcmp(serv->report->stat_format, "semtech") == 0;
	bool other_format = strcmp(serv->report->stat_format, "other") == 0;

	JSON_Value *root_value = NULL;
	JSON_Object *root_object = NULL;

	if (other_format) {
		root_value = json_value_init_object();
		root_object = json_value_get_object(root_value);
		JSON_Value *servers_array_value = json_value_init_array();
		JSON_Array *servers_array_object = json_value_get_array(servers_array_value);

		json_object_set_value(root_object, "servers", servers_array_value);
		json_object_set_string(root_object, "time", iso_timestamp);
		json_object_dotset_string(root_object, "device.id", GW.info.gateway_id);
		json_object_dotset_number(root_object, "device.latitude", cp_gps_coord.lat);
		json_object_dotset_number(root_object, "device.longitude", cp_gps_coord.lon);
		json_object_dotset_number(root_object, "device.altitude", cp_gps_coord.alt);
		json_object_dotset_number(root_object, "device.uptime", current_time - serv->state.startup_time);
		json_object_dotset_string(root_object, "device.gps", gps_state);
		json_object_dotset_string(root_object, "device.platform", GW.info.platform);
		json_object_dotset_string(root_object, "device.email", GW.info.email);
		json_object_dotset_string(root_object, "device.description", GW.info.description);
		json_object_dotset_number(root_object, "current.up_radio_packets_received", cp_nb_rx_rcv);
		json_object_dotset_number(root_object, "current.up_radio_packets_crc_good", cp_nb_rx_ok);
		json_object_dotset_number(root_object, "current.up_radio_packets_crc_bad", cp_nb_rx_bad);
		json_object_dotset_number(root_object, "current.up_radio_packets_crc_absent", cp_nb_rx_nocrc);
		json_object_dotset_number(root_object, "current.up_radio_packets_dropped", cp_nb_rx_drop);
		json_object_dotset_number(root_object, "current.up_radio_packets_forwarded", cp_up_pkt_fwd);
		json_object_dotset_number(root_object, "current.up_server_datagrams_send", cp_up_dgram_sent);
		json_object_dotset_number(root_object, "current.up_server_datagrams_acknowledged", cp_up_ack_rcv);
		json_object_dotset_number(root_object, "current.down_heartbeat_send", cp_dw_pull_sent);
		json_object_dotset_number(root_object, "current.down_heartbeat_received", cp_dw_ack_rcv);
		json_object_dotset_number(root_object, "current.down_server_datagrams_received", cp_dw_dgram_rcv);
		json_object_dotset_number(root_object, "current.down_server_datagrams_accepted", cp_dw_dgram_acp);
		json_object_dotset_number(root_object, "current.down_radio_packets_succes", cp_nb_tx_ok);
		json_object_dotset_number(root_object, "current.down_radio_packets_failure", cp_nb_tx_fail);
		json_object_dotset_number(root_object, "current.down_radio_packets_collision_packet", cp_nb_tx_rejected_collision_packet);
		json_object_dotset_number(root_object, "current.down_radio_packets_collision_beacon", cp_nb_tx_rejected_collision_beacon);
		json_object_dotset_number(root_object, "current.down_radio_packets_too_early", cp_nb_tx_rejected_too_early);
		json_object_dotset_number(root_object, "current.down_radio_packets_too_late", cp_nb_tx_rejected_too_late);
		json_object_dotset_number(root_object, "current.down_beacon_packets_queued", cp_nb_beacon_queued);
		json_object_dotset_number(root_object, "current.down_beacon_packets_send", cp_nb_beacon_sent);
		json_object_dotset_number(root_object, "current.down_beacon_packets_rejected", cp_nb_beacon_rejected);
    }

	if (semtech_format) {
		pthread_mutex_lock(&serv->report->mx_report);
        memset(serv->report->status_report, 0, sizeof(serv->report->status_report));
		if ((GW.gps.gps_enabled && coord_ok) || GW.gps.gps_fake_enable) {
			snprintf(serv->report->status_report, STATUS_SIZE,
					 "{\"stat\":{\"time\":\"%s\",\"lati\":%.5f,\"long\":%.5f,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}",
					 stat_timestamp, cp_gps_coord.lat, cp_gps_coord.lon,
					 cp_gps_coord.alt, cp_nb_rx_rcv, cp_nb_rx_ok,
					 cp_up_pkt_fwd, 100.0 * up_ack_ratio, cp_dw_dgram_rcv,
					 cp_nb_tx_ok, GW.info.platform, 
                     GW.info.email, GW.info.description);
		} else {
			snprintf(serv->report->status_report, STATUS_SIZE,
					 "{\"stat\":{\"time\":\"%s\",\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}",
					 stat_timestamp, cp_nb_rx_rcv, cp_nb_rx_ok,
					 cp_up_pkt_fwd, 100.0 * up_ack_ratio, cp_dw_dgram_rcv,
					 cp_nb_tx_ok, GW.info.platform, 
                     GW.info.email, GW.info.description);
		}
		lgw_log(LOG_INFO, "#[%s] Semtech status report ready. \n", serv->info.name);
		serv->report->report_ready = true;
		pthread_mutex_unlock(&serv->report->mx_report);
        //lgw_db_put("/fwd/pkts/report", timestr, serv->report->status_report);
        sem_post(&serv->thread.sema);
	}

	if (other_format) {
		pthread_mutex_lock(&serv->report->mx_report);
        memset(serv->report->status_report, 0, sizeof(serv->report->status_report));
        json_serialize_to_buffer(root_value, serv->report->status_report, STATUS_SIZE);
		serv->report->report_ready = true;
		pthread_mutex_unlock(&serv->report->mx_report);
		json_value_free(root_value);
    }

	lgw_log(LOG_INFO, "################### [%s] end of reporting #########################\n", serv->info.name);

}

void report_start() {
    serv_s* serv_entry;
    LGW_LIST_TRAVERSE(&GW.serv_list, serv_entry, list) { 
        switch (serv_entry->info.type) {
            case semtech:
                semtech_report(serv_entry);
                break;
            default:
	            lgw_log(LOG_INFO, "\n################[%s] no report of this service ###############\n", serv_entry->info.name);
                break;
        }
    }
}


