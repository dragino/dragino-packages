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

#include <MQTTPacket.h>

#include "fwd.h"
#include "loragw_hal.h"

#include "service.h"
#include "mqtt_service.h"

DECLARE_GW;
DECLARE_HAL;

static int payload_deal(mqttsession_s* session, struct lgw_pkt_rx_s* p);
static void mqtt_push_up(void* arg);
static void mqtt_init(serv_s* serv);

static void mqtt_cleanup(mqttsession_s* session) {
    MQTTClientDestroy(&session->client);
    lgw_free(session);
}

static int mqtt_sendping(mqttsession_s *session) {
    return MQTTSendPing(&session->client);
}

static long mqtt_getrtt(mqttsession_s *session) {
    return MQTTGetPingTime(&session->client) / 1000;
}

static void mqtt_dnlink_cb(struct MessageData *data, void* s) {

    mqttsession_s* session = (mqttsession_s*)s;

    if (data->message->payloadlen < 0)
        return;

    if (session->dnlink_handler)
        session->dnlink_handler(data);
}

void dnlink_handler(MessageData* data) {
    lgw_log(LOG_INFO, "INFO~ mqtt suscribe %d bytes message: %s/%s\n", data->message->payloadlen, data->topicName, (char*)data->message->payload);
}

static int mqtt_connect(serv_s *serv) {
    int err = -1;

    mqtt_init(serv);

    mqttsession_s* session = (mqttsession_s*)serv->net->mqtt->session;

    if (NULL == session)
        return err;

    MQTTPacket_connectData connect = MQTTPacket_connectData_initializer;

    err = NetworkConnect(&session->network, (char*)&serv->net->addr, atoi((char*)&serv->net->port_up));

    if (err != SUCCESS) 
        goto exit;

    connect.clientID.cstring = session->id;
    connect.keepAliveInterval = KEEP_ALIVE_INTERVAL;

    // Only set credentials when we have a key
    if (NULL != session->key) {
        connect.username.cstring = session->id;
        connect.password.cstring = session->key;
    }

    err = MQTTConnect(&session->client, &connect);
    if (err != SUCCESS)
        goto exit;

    serv->state.live = true;
    serv->state.stall_time = 0;
    serv->state.connecting = true;
    if (session->dnlink_topic)
        err = MQTTSubscribe(&session->client, session->dnlink_topic, QOS_DOWN, &mqtt_dnlink_cb, session);

exit:
    return err;
}

static void mqtt_disconnect(mqttsession_s *session) {
    MQTTDisconnect(&session->client);
    NetworkDisconnect(&session->network);
}

static int mqtt_reconnect(serv_s* serv) {
    serv->state.live = false;
    lgw_log(LOG_INFO, "INFO: [TTN] Reconnecting %s\n",serv->info.name);
    if (serv->net->mqtt->session) {
        mqtt_disconnect((mqttsession_s*)serv->net->mqtt->session);
        mqtt_cleanup((mqttsession_s*)serv->net->mqtt->session);
    }
    serv->state.connecting = false;
    //sem_post(&servers[idx].send_sem);
    return mqtt_connect(serv);
}

static int mqtt_checkconnected(mqttsession_s *session) {
    return NetworkCheckConnected(&session->network);
}
  
static int mqtt_send_uplink(mqttsession_s *session, char *uplink, int len) {

    int rc = FAILURE;
    void *payload = NULL;
    char *topic = NULL;

    MQTTMessage message;
    message.qos = QOS_UP;
    message.retained = 0;
    message.dup = 0;
    message.payload = uplink;
    message.payloadlen = len;

    if (session->uplink_topic)
        rc = MQTTPublish(&session->client, session->uplink_topic, &message);
    
    return rc;
}

/*! 
 * \brief mqtt 会话初始化 
 * \param id 会话的ID，gateway_id
 * \param dnlink_handle 下发内容处理函数
 * \param cb_arg 回调函数的参数
 * \ret   返回session
 *
 */

static void mqtt_init(serv_s* serv){
    mqttsession_s* mqttsession = (mqttsession_s*)lgw_malloc(sizeof(mqttsession_s));
    memset(mqttsession, 0, sizeof(mqttsession_s));
    mqttsession->id = serv->info.name;
    mqttsession->key = serv->info.key;
    mqttsession->dnlink_handler = &dnlink_handler;
    mqttsession->read_buffer = lgw_malloc(READ_BUFFER_SIZE);
    mqttsession->send_buffer = lgw_malloc(SEND_BUFFER_SIZE);
    mqttsession->dnlink_topic = serv->net->mqtt->dntopic;
    mqttsession->uplink_topic = serv->net->mqtt->uptopic;

    NetworkInit(&mqttsession->network);
    MQTTClientInit(&mqttsession->client, &mqttsession->network, COMMAND_TIMEOUT,
                   mqttsession->send_buffer, SEND_BUFFER_SIZE, mqttsession->read_buffer,
                   READ_BUFFER_SIZE);
    serv->net->mqtt->session = (void*)mqttsession;
}

int mqtt_start(serv_s* serv) {
    int ret;
    ret = mqtt_connect(serv);
    if (ret != SUCCESS) {
        lgw_log(LOG_WARNING, "WARNING~ [%s] Can't connet mqtt server.\n", serv->info.name);
        return FAILURE;
    }
    if (lgw_pthread_create_detached(&serv->thread.t_up, NULL, (void *(*)(void *))mqtt_push_up, serv)) {
        lgw_log(LOG_WARNING, "WARNING~ [%s] Can't create push up pthread.\n", serv->info.name);
        return -1;
    }
    return 0;
}

void mqtt_stop(serv_s* serv) {
	sem_post(&serv->thread.sema);
    serv->thread.stop_sig = true;
	pthread_join(serv->thread.t_up, NULL);
    if (serv->state.connecting) 
        mqtt_disconnect((mqttsession_s*)serv->net->mqtt->session);
    mqtt_cleanup((mqttsession_s*)serv->net->mqtt->session);
    serv->state.connecting = false;
    serv->state.live = false;
}

static void mqtt_push_up(void* arg) {
    serv_s* serv = (serv_s*) arg;

	int i, j;					/* loop variables */
    int err;
	struct lgw_pkt_rx_s *p;	/* pointer on a RX packet */

    mqttsession_s* session = (mqttsession_s*)serv->net->mqtt->session;

    if (!session) {
        mqtt_reconnect(serv);
    }

	while (!serv->thread.stop_sig) {
		// wait for data to arrive
		sem_wait(&serv->thread.sema);
		for (i = 0; i < serv->rxpkt_serv->nb_pkt; ++i) {
			p = &serv->rxpkt_serv->rxpkt[i];
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
            err = payload_deal((mqttsession_s*)serv->net->mqtt->session, p);
            if (err) {
                lgw_log(LOG_WARNING, "WARNING~ [%s] send data to mqtt server error, try to reconnect.\n", serv->info.name);
                mqtt_reconnect(serv);
            } else {
                lgw_log(LOG_INFO, "INFO~ [%s] send data to mqtt server succeed.\n", serv->info.name);
            }
        }
        pthread_mutex_lock(&GW.mx_bind_lock);
        serv->rxpkt_serv->bind--;
        pthread_mutex_unlock(&GW.mx_bind_lock);
    }
}

static int payload_deal(mqttsession_s* session, struct lgw_pkt_rx_s* p) {
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

    return mqtt_send_uplink(session, chan_data, data_size);
}

