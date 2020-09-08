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
 */

#ifndef _MQTT_PROTO_H
#define _MQTT_PROTO_H

#include <stdint.h>
#include <MQTTClient.h>

#define KEEP_ALIVE_INTERVAL 20
#define COMMAND_TIMEOUT 2000
#define READ_BUFFER_SIZE 512
#define SEND_BUFFER_SIZE 512

#define QOS_STATUS QOS1
#define QOS_DOWN QOS1
#define QOS_UP QOS1
#define QOS_CONNECT QOS1
#define QOS_WILL QOS1

typedef void (*dnlink_headler_f)(MessageData*);
  
typedef struct _mqttsession_s {
	Network network;
	MQTTClient client;
	dnlink_headler_f dnlink_handler;
	void *cb_arg;
	unsigned char *read_buffer;
	unsigned char *send_buffer;
	char *id;      
	char *key;     
	char *dnlink_topic;
	char *uplink_topic;
} mqttsession_s;

int mqtt_start(serv_s*);

void mqtt_stop(serv_s*);

#endif						
