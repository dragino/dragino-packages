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

#endif /* HANDLE_H_ */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>

#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <sqlite3.h>

#include "base64.h"
#include "parson.h"
#include "generic_list.h"
#include "db.h"

#include "LoRaMacCrypto.h"

#define FRAME_LEN               24 /* 17/3*4+1,corresponding to the ashanled.c */

#define GW_SERV_ADDR        "localhost"
#define NET_PORT_PUSH       1700
#define NET_PORT_PULL       1700

#define PKT_PUSH_DATA	0
#define PKT_PUSH_ACK	1
#define PKT_PULL_DATA	2
#define PKT_PULL_RESP	3
#define PKT_PULL_ACK	4

#define SOCKET_INTERVAL_S   10
#define SOCKET_INTERVAL_MS  0
#define SEND_INTERVAL_S     2
#define AS_SEND_INTERVAL_S  1  
#define SEND_INTERVAL_MS    0

/*define version of GWMP */
#define VERSION 2

#define STRINGIFY(x)	#x
#define STR(x)		STRINGIFY(x)
#define MSG(args...)    printf(args)
#define NB_PKT_MAX      8 /*the max size of the "rxpk" array*/
#define BUFF_SIZE ((540 * NB_PKT_MAX) + 30)

#define LORAMAC_FRAME_MAXPAYLOAD   255

#define CMD_FRAME_DOWN_MAX 34 

#define JSON_MAX  1024 /*1024 is big enough, not calculate the accurate size of json string*/

#define APPLICATION_SERVER     1
#define IGNORE                 0

#define MAX_NB_B64             341 /*255 bytes=340 char in b64*/
#define RANGE                  1000000 /*1s*/

#define CMD_UP_MAX             15

#define NO_DELAY               0
#define JOIN_ACCEPT_DELAY      6000000 /*6s the second window of join*/  

#define RECV_DELAY             2000000 /*2s* the second window of receive */

#define CLASS_A                0
#define CLASS_B                1
#define CLASS_C                2

#define FAILED     0
#define SUCCESS    1

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
	char     gwaddr[16];
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

        char    appeui_hex[17] = {'\0'};
        char    gweui_hex[17] = {'\0'};
        char    deveui_hex[17] = {'\0'};

        uint8_t appkey[16] = {'\0'};
        uint8_t appskey[16] = {'\0'};
        uint8_t nwkskey[16] = {'\0'};
        uint8_t devnonce[2] = {'\0'};

        char    outstr[24] = {'\0'};  /*temp string for swap data*/
};

/* json data return*/
struct jsondata {
	int to; /* which server to send to 1.app 2.nc 3.both 4.err 5.ignore */
	uint32_t devaddr;  /* gateway address for as downlink */
	char deveui_hex[17];
        struct msg_down msg;
};

struct th_check_arg {
	uint32_t devaddr;
	uint32_t tmst;
};

/*data structure of linked list node*/
struct msg {
	char*  json_string;
};

struct msg_down {
	char*  json_string;
	char*  gwaddr;
};

struct msg_join {
	char deveui_hex[17];
	uint32_t tmst;
};

typedef enum eLoRaMacSrvCmd
{
    /*!
     * LinkCheckAns
     */
    SRV_MAC_LINK_CHECK_ANS           = 0x02,
    /*!
     * LinkADRReq
     */
    SRV_MAC_LINK_ADR_REQ             = 0x03,
    /*!
     * DutyCycleReq
     */
    SRV_MAC_DUTY_CYCLE_REQ           = 0x04,
    /*!
     * RXParamSetupReq
     */
    SRV_MAC_RX_PARAM_SETUP_REQ       = 0x05,
    /*!
     * DevStatusReq
     */
    SRV_MAC_DEV_STATUS_REQ           = 0x06,
    /*!
     * NewChannelReq
     */
    SRV_MAC_NEW_CHANNEL_REQ          = 0x07,
    /*!
     * RXTimingSetupReq
     */
    SRV_MAC_RX_TIMING_SETUP_REQ      = 0x08,
}LoRaMacSrvCmd_t;

/*!
 * LoRaMAC mote MAC commands
 * copy from LoRaMac.h
 * LoRaWAN Specification V1.0, chapter 5, table 4
 */
typedef enum eLoRaMacMoteCmd
{
    /*!
     * LinkCheckReq
     */
    MOTE_MAC_LINK_CHECK_REQ          = 0x02,
    /*!
     * LinkADRAns
     */
    MOTE_MAC_LINK_ADR_ANS            = 0x03,
    /*!
     * DutyCycleAns
     */
    MOTE_MAC_DUTY_CYCLE_ANS          = 0x04,
    /*!
     * RXParamSetupAns
     */
    MOTE_MAC_RX_PARAM_SETUP_ANS      = 0x05,
    /*!
     * DevStatusAns
     */
    MOTE_MAC_DEV_STATUS_ANS          = 0x06,
    /*!
     * NewChannelAns
     */
    MOTE_MAC_NEW_CHANNEL_ANS         = 0x07,
    /*!
     * RXTimingSetupAns
     */
    MOTE_MAC_RX_TIMING_SETUP_ANS     = 0x08,
}LoRaMacMoteCmd_t;

/*reverse memory copy*/
void revercpy( uint8_t *dst, const uint8_t *src, int size );
/*transform the array of uint8_t to hexadecimal string*/
void i8_to_hexstr(uint8_t* uint, char* str, int size);

void set_timer(int sec, int msec);

void tcp_bind(const char* servaddr, const char* port, int* listenfd);
void tcp_connect(const char* servaddr, const char* port, int* sockfd, bool* exit_sig);
void udp_bind(const char* servaddr, const char* port, int* sockfd);

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

/*packet the data that will be sent to the gateaway*/
bool serialize_msg_to_gw(const char* data, int size, const char* deveui_hex, char* json_data, char* gwaddr, uint32_t tmst, int delay);

