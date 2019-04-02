/*
  ____  ____      _    ____ ___ _   _  ___  
  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 

Description:


License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: skerlan

*/

#include <unistd.h>
#include "handle.h"
#include "db.h"

/*stream direction*/
#define UP                                     0
#define DOWN                                   1

#define JOIN_ACC_SIZE                          17

extern struct context cntx;

/* ----------------------------------------------------------------- */
/* ------------------ PRIVATE function ----------------------------- */

static struct timeval push_timeout_half = {0, (PUSH_TIMEOUT_MS * 500)}; /* cut in half, critical for throughput */
static struct timeval pull_timeout = {0, (PULL_TIMEOUT_MS * 1000)}; /* non critical for throughput */

/*prepare the frame payload and compute the session keys*/
static void as_prepare_frame(uint8_t *frame_payload, uint16_t devnonce, uint8_t* appkey, uint32_t devaddr, uint8_t *nwkskey, uint8_t *appskey);

/* -------------------------------------------------------------------------------- */
/*!
 * LoRaMAC header field definition (MHDR field)
 * Copy from LoRaMac.h
 * LoRaWAN Specification V1.0, chapter 4.2
 */
typedef union uLoRaMacHeader {
    /*!
     * Byte-access to the bits
     */
    uint8_t Value;
    /*!
     * Structure containing single access to header bits
     */
    struct sHdrBits
    {
        /*!
         * Major version
         */
        uint8_t MType           : 3;
        /*!
         * RFU
         */
        uint8_t RFU             : 3;
        /*!
         * Message type
         */
        uint8_t Major           : 2;
    }Bits;
}LoRaMacHeader_t;

/*!
 * LoRaMAC frame control field definition (FCtrl)
 * Copy from LoRaMAC.h
 * LoRaWAN Specification V1.0, chapter 4.3.1
 */
typedef union uLoRaMacFrameCtrl {
    /*!
     * Byte-access to the bits
     */
    uint8_t Value;
    struct sCtrlBits
    {
        /*!
         * ADR control in frame header
         */
        uint8_t Adr             : 1;
        /*!
         * ADR acknowledgment request bit
         */
        uint8_t AdrAckReq       : 1;
        /*!
         * Message acknowledge bit
         */
        uint8_t Ack             : 1;
        /*!
         * Frame pending bit
         */
        uint8_t FPending        : 1;
        /*!
         * Frame options length
         */
        uint8_t FOptsLen        : 4;
    }Bits;
}LoRaMacFrameCtrl_t;

/*!
 * LoRaMAC DLSettings field definition
 * LoRaWAN Specification V1.0, chapter 5.4
 */
typedef union uLoRaMacDLSettings{
	uint8_t Value;
	struct sDLSBits{
		uint8_t RFU              : 1;
		uint8_t RX1DRoffset      : 3;
		uint8_t RX2DataRate      : 4;
	}Bits;
}LoRaMacDLSettings_t;

/*!
 * LoRaMAC DLSettings field definition
 * LoRaWAN Specification V1.0, chapter 6.2.5
 */
typedef union uLoRaMacRxDelay{
	uint8_t Value;
	struct sRxDBits{
		uint8_t RFU              : 4;
		uint8_t Del              : 4;
	}Bits;
}LoRaMacRxDelay_t;

/*!
 * LoRaMAC frame types
 * Copy from LoRaMAC.h
 * LoRaWAN Specification V1.0, chapter 4.2.1, table 1
 */
typedef enum eLoRaMacFrameType
{
    /*!
     * LoRaMAC join request frame
     */
    FRAME_TYPE_JOIN_REQ              = 0x00,
    /*!
     * LoRaMAC join accept frame
     */
    FRAME_TYPE_JOIN_ACCEPT           = 0x01,
    /*!
     * LoRaMAC unconfirmed up-link frame
     */
    FRAME_TYPE_DATA_UNCONFIRMED_UP   = 0x02,
    /*!
     * LoRaMAC unconfirmed down-link frame
     */
    FRAME_TYPE_DATA_UNCONFIRMED_DOWN = 0x03,
    /*!
     * LoRaMAC confirmed up-link frame
     */
    FRAME_TYPE_DATA_CONFIRMED_UP     = 0x04,
    /*!
     * LoRaMAC confirmed down-link frame
     */
    FRAME_TYPE_DATA_CONFIRMED_DOWN   = 0x05,
    /*!
     * LoRaMAC RFU frame
     */
    FRAME_TYPE_RFU                   = 0x06,
    /*!
     * LoRaMAC proprietary frame
     */
    FRAME_TYPE_PROPRIETARY           = 0x07,
}LoRaMacFrameType_t;

/* --------------------------------------------------------------------------- */
/* ------- handle message process -------------------------------------------- */

void ns_msg_handle(struct jsondata* result, struct metadata* meta, uint8_t* payload) {

	uint8_t appeui[8];
	uint8_t deveui[8];

    char tempstr[3] = {'\0'};

    struct devinfo devinfo;

    int i, delay = 0;

	int size = meta->size; 	/*the length of payload*/
	LoRaMacHeader_t macHdr; /*the MAC header*/

	/*typical fields for confirmed/unconfirmed message*/
	LoRaMacFrameCtrl_t fCtrl;

	/* json args for parsing data to json string*/
    char json_string[512];

	uint32_t fopts_len;/*the size of foptions field*/
	uint8_t adr;       /*indicate if ADR is permitted*/

    char frame_payload[FRAME_LEN];  /* prepare payload for join accept */
	uint8_t fpayload[LORAMAC_FRAME_MAXPAYLOAD]; /*the frame payload of UP_DATE*/
    uint8_t frame_payload_b64[MAX_NB_B64];

	uint32_t mic = 0;
	uint32_t cal_mic;/*the MIC value calculated by the payload*/

	/*analyse the type of the message*/
	macHdr.Value = payload[0];
	switch (macHdr.Bits.MType) {
		case FRAME_TYPE_JOIN_REQ: {     /*join request message*/
			revercpy(appeui, payload + 1, 8);
			revercpy(deveui, payload + 9, 8);
			i82hexstr(appeui, devinfo.appeui_hex, 8);
			i82hexstr(deveui, devinfo.deveui_hex, 8);
			devinfo.devnonce |= (uint16_t)payload[17];
			devinfo.devnonce |= ((uint16_t)payload[18])<<8;
			mic |= (uint32_t)payload[19];
			mic |= ((uint32_t)payload[20])<<8;
			mic |= ((uint32_t)payload[21])<<16;
			mic |= ((uint32_t)payload[22])<<24;

			MSG_DEBUG(DEBUG_INFO, "\nINFO: [up] JOIN REQUE: appeui=%s, deveui=%s, devnonce=(%4X)\n", 
                    devinfo.appeui_hex, devinfo.deveui_hex, devinfo.devnonce);
			/*judge whether it is a repeated message*/
            /* select devnonce from devs where deveui = ? and devnonce = ?*/
            /*
			if (db_judge_joinrepeat(cntx.judgejoinrepeat, &devinfo)) {
				MSG("WARNING: [up] same devnonce as last.\n");
				result->to = IGNORE;
				break;
			} 
            */

            /* select appkey from apps where appeui = ? */
			db_lookup_appkey(cntx.lookupappkey, &devinfo); 

            i82hexstr(devinfo.appkey, devinfo.appkey_hex, 16);

			LoRaMacJoinComputeMic(payload, 23 - 4, devinfo.appkey, &cal_mic);

            MSG_DEBUG(DEBUG_INFO, "\nINFO~ appkey={%s}, mic=%u, cal_mic=%u\n", devinfo.appkey_hex, mic, cal_mic);

			/*if mic is wrong,the join request will be ignored*/
			if (mic != cal_mic) {
				MSG_DEBUG(DEBUG_WARNING, "WARNING: [up] join request payload mic is wrong!\n");
				result->to = IGNORE;
			} else {
				srand(time(NULL));
				devinfo.devaddr = (uint32_t)rand();
				as_prepare_frame(frame_payload, devinfo.devnonce, devinfo.appkey, devinfo.devaddr, devinfo.nwkskey, devinfo.appskey);

                bin_to_b64(frame_payload, JOIN_ACC_SIZE, frame_payload_b64, MAX_NB_B64);

                memset(devinfo.appskey_hex, 0, sizeof(devinfo.appskey_hex));
                memset(devinfo.nwkskey_hex, 0, sizeof(devinfo.nwkskey_hex));

                for (i = 0; i < 16; i++) {
                    memset(tempstr, 0, sizeof(tempstr));
                    snprintf(tempstr, sizeof(tempstr), "%02X", devinfo.appskey[i]);
                    strcat(devinfo.appskey_hex, tempstr);
                    memset(tempstr, 0, sizeof(tempstr));
                    snprintf(tempstr, sizeof(tempstr), "%02X", devinfo.nwkskey[i]);
                    strcat(devinfo.nwkskey_hex, tempstr);
                }

                MSG_DEBUG(DEBUG_INFO, "\nINFO~ [up]nwkskey=(%s), appskey=(%s), devaddr=(%08X)\n", 
                        devinfo.nwkskey_hex, devinfo.appskey_hex, devinfo.devaddr);

                /* update or IGNORE devs set devaddr = ?, appskey = ?, nwkskey = ?, devnonce = ? wheredeveui = ? */
                db_update_devinfo(cntx.updatedevinfo, &devinfo);  /* update device info on database */

				serialize_msg_to_gw(frame_payload_b64, 17, meta->gweui_hex, json_string, meta->tmst, JOIN_ACCEPT_DELAY); 
                result->msg_down = malloc(sizeof(struct msg_down));
                result->msg_down->gwaddr = malloc(strlen(meta->gwaddr) + 1);
                result->msg_down->json_string = malloc(strlen(json_string) + 1);
				strcpy(result->msg_down->gwaddr, meta->gwaddr);
				strcpy(result->msg_down->json_string, json_string);
				result->to = APPLICATION_SERVER;
			}
			break;
		}
		/*unconfirmed message*/
		case FRAME_TYPE_DATA_UNCONFIRMED_UP:/*fall through,just handle like confirmed message*/
		/*confirmed message*/
		case FRAME_TYPE_DATA_CONFIRMED_UP: {
			devinfo.devaddr |= (uint32_t)payload[1];
			devinfo.devaddr |= ((uint32_t)payload[2])<<8;
			devinfo.devaddr |= ((uint32_t)payload[3])<<16;
			devinfo.devaddr |= ((uint32_t)payload[4])<<24;
			fCtrl.Value = payload[5];
			fopts_len = fCtrl.Bits.FOptsLen;
			adr = fCtrl.Bits.Adr;
			meta->fcntup |= (uint16_t)payload[6];
			meta->fcntup |= ((uint16_t)payload[7])<<8;
			meta->fport = payload[8 + fopts_len];
			memcpy(fpayload, payload + 9 + fopts_len, size - 13 - fopts_len);
			mic |= (uint32_t)payload[size - 4];
			mic |= ((uint32_t)payload[size - 3])<<8;
			mic |= ((uint32_t)payload[size - 2])<<16;
			mic |= ((uint32_t)payload[size - 1])<<24;

			MSG_DEBUG(DEBUG_INFO, "\nINFO~ [up] DATA Receive devaddr=(%08X)\n", devinfo.devaddr);

            /* select deveui from devs where devaddr = ? */
            memset(devinfo.deveui_hex, 0, sizeof(devinfo.deveui_hex));

			db_judge_devaddr(cntx.judgedevaddr, &devinfo);

			if (strlen(devinfo.deveui_hex) < 8) {
				MSG_DEBUG(DEBUG_WARNING, "WARNING~ [up] The device not register!\n");
				/*The device has not joined in the LoRaWAN */
				result->to = IGNORE;
				break;
			}

			/*judge whether it is a repeated message*/
            /* select id from upmsg where deveui = ? and tmst = ? */
			if (db_judge_msgrepeat(cntx.judgemsgrepeat, devinfo.deveui_hex, meta->tmst)) {
				MSG_DEBUG(DEBUG_WARNING, "WARNING: [up] repeated push_data!\n");
				result->to = IGNORE;
				break;
			} 

            /* select nwkskey from devs where deveui = ? */
			db_lookup_nwkskey(cntx.lookupnwkskey, &devinfo); 

			LoRaMacComputeMic(payload, meta->size - 4, devinfo.nwkskey, devinfo.devaddr, UP, (uint32_t)meta->fcntup, &cal_mic);
			if(cal_mic != mic){
				MSG("WARNING: [up] push data payload mic is wrong!\n");
				result->to = IGNORE;
				break;
            }

            /* udpate tables insert upmsg */
            /* insert into upmsg (tmst, datarate, freq, rssi, snr, fcntup, gweui, appeui, deveui, frmpayload) values */
                                
            db_insert_upmsg(cntx.insertupmsg, &devinfo, meta, payload);

			LoRaMacPayloadDecrypt(payload, meta->size, devinfo.appskey, devinfo.devaddr, UP, (uint32_t)meta->fcntup, fpayload);

            MSG_DEBUG(DEBUG_INFO, "INFO~ [up]Decrypted: %s\n", fpayload);

			/*when the message contains both MAC command and userdata*/
                        /* do nothing */

			result->to = IGNORE;

			break;
		}
		/*proprietary message*/
		case FRAME_TYPE_PROPRIETARY: {
			memcpy(fpayload, payload + 1, meta->size - 1);
			result->to = IGNORE;
			break;
		}
	}

}

void serialize_msg_to_gw(const char* data, int size, char* gweui_hex, char* json_data, uint32_t tmst, int delay) {
	JSON_Value  *root_val_x = NULL;
	JSON_Object *root_obj_x = NULL;
	unsigned int rx2_dr;
	float rx2_freq;
	char dr[16];
	struct timespec time;/*storing local timestamp*/
	char* json_str = NULL;

    /* select rx2datarate, rx2freq from gwprofile where id in select profileid from gws where gweui = ? */
    db_lookup_profile(cntx.lookupprofile, gweui_hex, &rx2_dr, &rx2_freq);

	switch (rx2_dr) {
		case 0:{
			strcpy(dr, "SF12BW125");
			break;
		}
		case 1:{
			strcpy(dr, "SF11BW125");
			break;
		}
		case 2:{
			strcpy(dr, "SF10BW125");
			break;
		}
		case 3:{
			strcpy(dr, "SF9BW125");
			break;
		}
		case 4:{
			strcpy(dr, "SF8BW125");
			break;
		}
		case 5:{
			strcpy(dr, "SF7BW125");
			break;
		}
		case 6:{
			strcpy(dr, "SF7BW250");
			break;
		}
		case 7:{
			strcpy(dr, "FSK");
			break;
		}
		default:{
			strcpy(dr, "SF12BW125");
		}
	}
	MSG_DEBUG(DEBUG_INFO, "INFO~ JOIN_ACCEPT(%s %.6f)\n", dr, rx2_freq);
	clock_gettime(CLOCK_REALTIME, &time);
	root_val_x = json_value_init_object();
	root_obj_x = json_value_get_object(root_val_x);
	if (delay == NO_DELAY) {
		json_object_dotset_boolean(root_obj_x, "txpk.imme", true);
	} else {
		json_object_dotset_number(root_obj_x, "txpk.tmst", tmst + delay);
	}
	json_object_dotset_number(root_obj_x, "txpk.freq", rx2_freq);
	json_object_dotset_number(root_obj_x, "txpk.rfch", 0);
	json_object_dotset_number(root_obj_x, "txpk.powe", 20);
	json_object_dotset_string(root_obj_x, "txpk.modu", "LORA");
	json_object_dotset_string(root_obj_x, "txpk.datr", dr);
	json_object_dotset_string(root_obj_x, "txpk.codr", "4/5");
	json_object_dotset_boolean(root_obj_x, "txpk.ipol", 1);
	json_object_dotset_number(root_obj_x, "txpk.size", size);
	json_object_dotset_string(root_obj_x, "txpk.data", data);
	json_str = json_serialize_to_string(root_val_x);
    strncpy(json_data, json_str, strlen(json_str) + 1);
    MSG_DEBUG(DEBUG_INFO, "INFO~ [donw] TXPK:(%s)\n", json_str);
	json_free_serialized_string(json_str);
	json_value_free(root_val_x);
}


static void as_prepare_frame(uint8_t *frame_payload, uint16_t devnonce, uint8_t* appkey, uint32_t devaddr, uint8_t *nwkskey, uint8_t *appskey) {
	uint8_t  payload[JOIN_ACC_SIZE];
	uint8_t index = 0;
	LoRaMacHeader_t hdr;
	int rand_num;
	uint8_t appNonce[3];
	uint8_t NetID[3] = {0x00, 0x00, 0x00};
	LoRaMacDLSettings_t dls;
	LoRaMacRxDelay_t rxd;
	uint32_t mic;

	/*the header of mac LoRaMac packet*/
	hdr.Value=0;
	hdr.Bits.MType = FRAME_TYPE_JOIN_ACCEPT;
	payload[index] = hdr.Value;
	/*generate the appNonce,here just for experienment using random number*/
	srand(time(NULL));
	rand_num = (uint32_t)rand();
	/*AppNonce*/
	payload[++index] = appNonce[2] = rand_num & 0xFF;
	payload[++index] = appNonce[1] = (rand_num>>8) & 0xFF;
	payload[++index] = appNonce[0] = (rand_num>>16) & 0xFF;
	/*NetID*/
	payload[++index] = NetID[2];
	payload[++index] = NetID[1];
	payload[++index] = NetID[0];
	/*devaddr*/
	payload[++index] = devaddr & 0xFF;
	payload[++index] = (devaddr>>8) & 0xFF;
	payload[++index] = (devaddr>>16) & 0xFF;
	payload[++index] = (devaddr>>24) & 0xFF;
	/*DLSettings*/
	dls.Value = 0;
	dls.Bits.RX1DRoffset = 0;
	dls.Bits.RX2DataRate = 0;
	payload[++index] = dls.Value;
	/*RxDelay*/
	rxd.Value = 0;
	rxd.Bits.Del = 1;
	payload[++index] = rxd.Value;

	LoRaMacJoinComputeMic(payload, (uint16_t)17 - 4, appkey, &mic);
	payload[++index] = mic & 0xFF;
	payload[++index] = (mic>>8) & 0xFF;
	payload[++index] = (mic>>16) & 0xFF;
	payload[++index] = (mic>>24) & 0xFF;

	/*compute the two session key
	 *the second argument is corresponding to the LoRaMac.c(v4.0.0)
	 *it seems that it makes a mistake,because the byte-order is adverse
	*/
	LoRaMacJoinComputeSKeys(appkey, payload + 1, devnonce, nwkskey, appskey);

    int i;
    MSG("\n=================================================================\n");
    MSG("PAYLOAD:");
    for (i = 0; i <= index; i++) {
        printf("%02X", payload[i]);
    }

    MSG("\nappnonce:%02X, devaddr:%08X, mic:%08X\n", appNonce[0], devaddr, mic);

	/*encrypt join accept message*/
	LoRaMacJoinEncrypt(payload + 1, (uint16_t)JOIN_ACC_SIZE - 1, appkey, frame_payload + 1);
	frame_payload[0] = payload[0];
    MSG("\nENPAYLOAD:");
    for (i = 0; i < JOIN_ACC_SIZE; i++) {
        printf("%02X", frame_payload[i]);
    }
    MSG("\n===============================================================\n");
}

/* ---------------------------------------------------------------------------- */
/* ---------- process string message ------------------------------------------ */

int compare_msg_down(const void* data, const void* key) {
	return strcmp(((struct msg_down*)data)->gwaddr, (const char*)key);
}

void assign_msg_down(void* data, const void* msg) {
	struct msg_down* data_x = (struct msg_down*)data;
	struct msg_down* msg_x = (struct msg_down*)msg;
	data_x->gwaddr = msg_x->gwaddr;
	data_x->json_string = msg_x->json_string;
}

void copy_msg_down(void* data, const void* msg) {
	struct msg_down* data_x = (struct msg_down*)data;
	struct msg_down* msg_x = (struct msg_down*)msg;
	strcpy(data_x->json_string, msg_x->json_string);
	strcpy(data_x->gwaddr, msg_x->gwaddr);
}

void destroy_msg_down(void* msg) {
	struct msg_down* message = (struct msg_down*)msg;
	free(message->json_string);
	free(message->gwaddr);
}


void i82hexstr(uint8_t* uint, char* str, int size) {
	/*in case that the str has a value,strcat() seems not safe*/
	memset(str, 0, size * 2 + 1);
	char tempstr[3];
	int i;
	for (i = 0; i < size; i++) {
		snprintf(tempstr, sizeof(tempstr), "%02X", uint[i]);
		strcat(str, tempstr);
	}
}

void str2hex(uint8_t* dest, char* src, int len) {
        int i;
        char ch1;
        char ch2;
        uint8_t ui1;
        uint8_t ui2;

        for(i = 0; i < len; i++) {
                ch1 = src[i*2];
                ch2 = src[i*2 + 1];
                ui1 = toupper(ch1) - 0x30;
                if (ui1 > 9)
                        ui1 -= 7;
                ui2 = toupper(ch2) - 0x30;
                if (ui2 > 9)
                        ui2 -= 7;
                dest[i] = ui1*16 + ui2;
        }
}

/* -------------------------------------------------------------*/
/* ----- common function ---------------------------------------*/

/*reverse memory copy*/
void revercpy( uint8_t *dst, const uint8_t *src, int size) {
    dst = dst + ( size - 1 );
    while( size-- ) {
        *dst-- = *src++;
    }
}

/*timer function*/
void set_timer(int sec,int msec) {
	/*it seems the select() will restart when interrupted by SIGINT*/
	struct timeval timeout;
	timeout.tv_sec = sec;
	timeout.tv_usec = msec;
	select(0, NULL, NULL, NULL, &timeout);
}

/*get TCP socket and bind it*/
void tcp_bind(const char* servaddr, const char* port, int* listenfd) {
	struct addrinfo hints;
	struct addrinfo *results,*r;
	int i;
	char host_name[64];
	char service_name[64];
	int listen;

	/*try to open and bind TCP listening socket*/
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	if (getaddrinfo(servaddr, port, &hints, &results) != 0) {
		MSG("ERROR: [down] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr, port, gai_strerror(i));
		exit(EXIT_FAILURE);
	}
	r = results;
	do{
		listen = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (listen < 0)
			continue;
		if (bind(listen, r->ai_addr, r->ai_addrlen) == 0)
			break;				/*success*/
		close(listen);		/*bind error,close and try next one*/
	} while ((r = r->ai_next) != NULL);

	if (r == NULL) {
		MSG("ERROR: [down] failed to open listening socket to any of server %s addresses (port %s)\n", servaddr, port);
		i = 1;
		for (r = results; r != NULL; r = r->ai_next) {
			getnameinfo(r->ai_addr, r->ai_addrlen, host_name, sizeof(host_name), service_name, sizeof(service_name), NI_NUMERICHOST);
			MSG("INFO: [down] result %i host:%s service:%s\n", i, host_name, service_name);
			i++;
		}
		exit(EXIT_FAILURE);
	}
	*listenfd = listen;
	freeaddrinfo(results);
}

/*get the socket fd and connect to the server*/
void tcp_connect(const char* servaddr, const char* port, int* sockfd, bool* exit_sig) {
	/*some  variables for building TCP sockets*/
	struct addrinfo hints;
	struct addrinfo *results;
	struct addrinfo *r;
	int i;
	char host_name[64];
	char service_name[64];
	int sock;
	const int on = 1;

	/*try to open and connect socket */
	while (! *exit_sig) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		if (getaddrinfo(servaddr, port, &hints, &results) != 0) {
			MSG("ERROR: [up] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr, port, gai_strerror(i));
			exit(EXIT_FAILURE);
		}
		for (r = results; r != NULL; r = r->ai_next) {
			sock = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
			if (sock < 0)
				continue;
			setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
			if (connect(sock, r->ai_addr, r->ai_addrlen) == 0)
				break;
			close(sock);/*connect error,close and try again*/
		}
		if (r == NULL) {
			MSG("ERROR: [up] failed to open socket to any of server %s addresses (port %s)\n", servaddr, port);
			i = 1;
			for (r = results; r != NULL; r = r->ai_next){
				getnameinfo(r->ai_addr, r->ai_addrlen, host_name, sizeof(host_name), service_name, sizeof(service_name), NI_NUMERICHOST);
				MSG("INFO: [up] result %i host:%s service:%s\n", i, host_name, service_name);
				i++;
			}
		} else {
			/*the connection to the server is established, break*/
			*sockfd = sock;
			MSG("INFO: connect to the server %s addresses (port %s) successfully\n", servaddr, port);
			freeaddrinfo(results);
			break;
		}
		freeaddrinfo(results);
		set_timer(SOCKET_INTERVAL_S, SOCKET_INTERVAL_MS);
	}
}

/*get the udp socket fd and bind it*/
void udp_bind(const char* servaddr, const char* port, int* sockfd, int type) {
	/*some  variables for building UDP sockets*/
	int i;
	struct addrinfo hints;
	struct addrinfo *results;
	struct addrinfo *r;
	char host_name[64];
	char service_name[64];
	int sock;

	/*try to open and bind UDP socket */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	if (getaddrinfo(servaddr, port, &hints, &results) != 0) {
		MSG("ERROR: [up] getaddrinfo on address %s (PORT %s) returned %s\n", servaddr, port, gai_strerror(i));
		exit(EXIT_FAILURE);
	}
	for (r = results; r != NULL; r = r->ai_next) {
		sock = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (sock < 0)
			continue;
		if (bind(sock, r->ai_addr, r->ai_addrlen) == 0)
			break;
		close(sock);/*bind error,close and try again*/
	}

	if (r == NULL) {
		MSG("ERROR: [up] failed to open socket to any of server %s addresses (port %s)\n", servaddr, port);
		i = 1;
		for (r = results; r != NULL; r = r->ai_next) {
			getnameinfo(r->ai_addr, r->ai_addrlen, host_name, sizeof(host_name), service_name, sizeof(service_name), NI_NUMERICHOST);
			MSG("INFO: [up] result %i host:%s service:%s\n", i, host_name, service_name);
			i++;
		}
		exit(EXIT_FAILURE);
	}

    if (type) { /* push */
        if ((setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void *)&push_timeout_half, sizeof(push_timeout_half))) != 0) {
            MSG("ERROR~ [up] setsockopt returned %s\n", strerror(errno));
            sock = -1;
        }
    } else {
        if ((setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void *)&pull_timeout, sizeof(pull_timeout))) != 0) {
            MSG("ERROR~ [up] setsockopt returned %s\n", strerror(errno));
            sock = -1;
        }
    }

	*sockfd = sock;
	freeaddrinfo(results);
}



