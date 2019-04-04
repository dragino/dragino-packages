/*
  ____  ____      _    ____ ___ _   _  ___  
  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 

Description:
    Network server, receives UDP packets and dispatch

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: skerlan

*/

#ifndef HANDLE_H_
#define HANDLE_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>

#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <sqlite3.h>

#include "base64.h"
#include "parson.h"
#include "generic_list.h"

#include "LoRaMacCrypto.h"

#define FRAME_LEN               24 /* 17/3*4+1,corresponding to the ashanled.c */

#define DBPATH "/tmp/loraserv"

#define PKT_PUSH_DATA	0
#define PKT_PUSH_ACK	1
#define PKT_PULL_DATA	2
#define PKT_PULL_RESP	3
#define PKT_PULL_ACK	4

#define PUSH_TIMEOUT_MS     100
#define PULL_TIMEOUT_MS     200

#define SOCKET_INTERVAL_S   10
#define SOCKET_INTERVAL_MS  0
#define SEND_INTERVAL_S     2

/*define version of GWMP */
#define VERSION 2

#define STRINGIFY(x)	#x
#define STR(x)		STRINGIFY(x)
#define MSG(args...)    printf(args)
#define MSG_DEBUG(FLAG, fmt, ...)                                                               \
         do {                                                                                       \
                if (FLAG)                                                                                 \
                      fprintf(stdout, fmt, ##__VA_ARGS__); \
         } while (0)

#define NB_PKT_MAX      8 /*the max size of the "rxpk" array*/
#define BUFF_SIZE ((540 * NB_PKT_MAX) + 30)

#define LORAMAC_FRAME_MAXPAYLOAD   255

#define JSON_MAX  1024 /*1024 is big enough, not calculate the accurate size of json string*/

#define APPLICATION_SERVER     1
#define IGNORE                 0

#define MAX_NB_B64             341 /*255 bytes=340 char in b64*/
#define RANGE                  1000000 /*1s*/

#define NO_DELAY               0
#define JOIN_ACCEPT_DELAY      6000000 /*6s the second window of join*/  

#define RECV_DELAY             2000000 /*2s* the second window of receive */

#define CLASS_A                0
#define CLASS_B                1
#define CLASS_C                2

/* log level */
extern int DEBUG_INFO;
extern int DEBUG_DEBUG;
extern int DEBUG_WARNING;
extern int DEBUG_ERROR;
extern int DEBUG_JOIN;
extern int DEBUG_UPDW;
extern int DEBUG_SQL;

/* structure used for sending and receiving data */
struct pkt {
	char json_content[JSON_MAX];
};

/* packet payload  struct to handle*/
struct pkt_info {
	uint8_t pkt_payload[BUFF_SIZE - 12];/*packet payload*/
	int  pkt_no;/*packet number*/
	char gwaddr[16];/*gateway address*/
	char gweui_hex[17];/*gateway address*/
};

/*meta data of the packet */
struct metadata {
	char    gwaddr[16];
    char    gweui_hex[17];
	uint32_t tmst;     /*raw time stamp*/
	char     time[28];
	uint8_t  chan;     /* IF channel*/
	uint8_t  rfch;     /* RF channel*/
	double 	 freq;     /*frequency of IF channel*/
	uint8_t  stat;     /* packet status*/
	char     modu[5];  /* modulation:LORA or FSK*/
	char     datrl[10];/* data rate for LORA*/
	uint32_t datrf;    /*data rate for FSK*/
	char     codr[4];  /*ECC coding rate*/
	float    lsnr;     /*SNR in dB*/
	float    rssi;	   /*rssi in dB*/
	uint16_t size;     /*payload size in bytes*/
	uint16_t fcntup;     
	uint8_t fport;     
};

/* device info for session */
struct devinfo {
	uint32_t devaddr;     
    uint16_t devnonce;

    char devaddr_hex[9];
    char devnonce_hex[7];

    char deveui_hex[17];
    char appeui_hex[17];

    uint8_t appkey[16];
    uint8_t appskey[16];
    uint8_t nwkskey[16];

    uint8_t appkey_hex[33];
    uint8_t appskey_hex[33];
    uint8_t nwkskey_hex[33];
};

/* message for download PULL_RES or JOIN_ACCEPT */
struct msg_down {
	char*  json_string;
	char*  gwaddr;
};

/* json data return*/
struct jsondata {
	int to; /* which server to send to 1.app 2.nc 3.both 4.err 5.ignore */
	uint32_t devaddr;  /* gateway address for as downlink */
	char deveui_hex[17];
    struct msg_down* msg_down;
};

/*reverse memory copy*/
void revercpy( uint8_t *dst, const uint8_t *src, int size );

/*transform the array of uint8_t to hexadecimal string*/
void i82hexstr(uint8_t* uint, char* str, int size);

void str2hex(uint8_t* dest, char* src, int len);

void set_timer(int sec, int msec);

void tcp_bind(const char* servaddr, const char* port, int* listenfd);
void tcp_connect(const char* servaddr, const char* port, int* sockfd, bool* exit_sig);
void udp_bind(const char* servaddr, const char* port, int* sockfd, int type);

/*compare the node element*/
int compare_msg_down(const void* data, const void* key);

/*destory the linked list node
 * free the memory allocated in the heap
 */
void destroy_msg_down(void* msg);

/*shallow copy
 * for the data allocated in the heap,
 * just copy the pointer
 */
void assign_msg_down(void* data, const void* msg);

/*deep copy*/
void copy_msg_down(void* data, const void* msg);

void ns_msg_handle(struct jsondata* result, struct metadata* meta, uint8_t* payload);

/*packet the data that will be sent to the gateaway*/
void serialize_msg_to_gw(const char* data, int size, char* deveui_hex, char* json_data, uint32_t tmst, int delay);

#endif /* HANDLE_H_ */
