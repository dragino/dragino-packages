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

#include "handle.h"
#include "db.h"
    
/* log level */
int DEBUG_INFO = 1;
int DEBUG_DEBUG = 0;
int DEBUG_WARNING = 0;
int DEBUG_ERROR = 1;
int DEBUG_JOIN = 0;
int DEBUG_UPDW = 0;
int DEBUG_SQL = 1;

struct context cntx; /* sqlite3 database context */

/* --- PRIVATE VARIABLES ------------------------------------------- */
/*network configuration variables*/
static char netserv_addr[64] = STR(GW_SERV_ADDR);

//static char gwserv_addr[64]=STR(GW_SERV_ADDR);
static char netserv_port_push[8] = STR(GW_PORT_PUSH);
static char netserv_port_pull[8] = STR(GW_PORT_PULL);

/* network sockets */
static int sockfd_push;/*socket for upstream from gateway*/
static int sockfd_pull;/*socket for downstream to gateway*/


/*linked list used for storing json sting for application server and network controller*/
static linked_list gw_list;

/*mutex lock for controlling access to the linked list*/
static pthread_mutex_t mx_gw_list = PTHREAD_MUTEX_INITIALIZER;

/*mutex lock for controlling access to the database*/
static pthread_mutex_t mx_db = PTHREAD_MUTEX_INITIALIZER;

/*exit signal*/
static bool exit_sig = false;

/* measurements to establish statistics */
static pthread_mutex_t mx_push = PTHREAD_MUTEX_INITIALIZER; /* control access to the push_data upstream measurements */
static uint32_t nb_push_rcv = 0; /* count push_data packets received */
static uint32_t tmst_g;

/* --- PRIVATE FUNCTIONS ------------------------------------------- */
/*signal handle function*/
static void signal_handle(int signo);

/* threads*/
void  thread_down(void);/*thread for send downstream to gateway*/
void  thread_up_handle(void*);/*thread for handling message in upstream from gateway */

/* ----------------------------------------------------------------------------------------------------- */
/* ------------------------------------------- MAIN FUNCTION ------------------------------------------- */
int main(int argc, char** argv) {

    int i;
	int pkt_no = 0;
	int msg_len;/*length of buffer received*/

	uint8_t gweui[8];
    char gweui_hex[17] = {'\0'};

	uint8_t buff_push[BUFF_SIZE]; /* buffer to receive  upstream packets */
	uint8_t buff_push_ack[4];/*buffer to confirm the upstream packets*/

	struct pkt_info pkt_info;

	struct sockaddr_in cliaddr;
	socklen_t addrlen;

	memset(&cliaddr, 0, sizeof(cliaddr));
	addrlen = sizeof(cliaddr);

	/* threads*/
	pthread_t th_down;/*thread for downstream to gateway*/
    pthread_t th_up_handle;/*thread for handing the json string in upstream*/

	struct sigaction sigact;

	/* display host endianness infomation*/
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		MSG_DEBUG(DEBUG_INFO, "INFO~ Little endian host\n");
	#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		MSG_DEBUG(DEBUG_INFO, "INFO~ Big endian host\n");
	#else
		MSG_DEBUG(DEBUG_INFO, "INFO~ Host endiannes is unknown\n");
	#endif

	/*configure the signal handling*/
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = signal_handle;
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	/*create the linked list for GW Donlink*/
	list_init(&gw_list);

	/*try to open and bind udp socket*/
	udp_bind(netserv_addr, netserv_port_push, &sockfd_push, 1);
	udp_bind(netserv_addr, netserv_port_pull, &sockfd_pull, 0);
        
	/*create threads*/

	if (pthread_create(&th_down, NULL, (void*(*)(void*))thread_down, NULL) !=0 ) {
		MSG_DEBUG(DEBUG_ERROR, "ERROR: [main] impossible to create thread for downstream communicating from gateway\n");
		exit(EXIT_FAILURE);
	}

    if (!db_init(DBPATH, &cntx)) {
		MSG_DEBUG(DEBUG_ERROR, "ERROR: [main] can't create database context\n");
		exit(EXIT_FAILURE);
    }

	while (!exit_sig) {        /* main thread, dispatch PUSH_DATA */
		msg_len = recvfrom(sockfd_push, buff_push, sizeof(buff_push), 0, (struct sockaddr*)&cliaddr, &addrlen);
		if (msg_len < 0) {
			//MSG("WARNING: [up] thread_up recv returned %s\n", strerror(errno));
			continue;
		}

		if (msg_len == 0) {
			if (exit_sig == true){
				/*thread_up socket is shut down*/
				break;
			} else {
				MSG_DEBUG(DEBUG_WARNING, "WARNING: [up] the data size is 0 ,just ignore\n");
				continue;
			}
		}
		/* if the datagram does not respect the format of PUSH DATA, just ignore it */
		if (msg_len < 12 || buff_push[0] != VERSION || buff_push[3] != PKT_PUSH_DATA){
            /* too short for GW <-> MAC protocol */
<<<<<<< HEAD
			MSG_DEBUG(DEBUG_WARNING, "WARNING: [up] push data invalid, ignore!\n");
=======
			MSG("WARNING: [up] push data invalid, ignore!\n");
>>>>>>> ecfb381ade916363034db6f48f1fa3f2c57029ab
			continue;
		}

        memset(gweui, 0, sizeof(gweui));
        memset(gweui_hex, 0, sizeof(gweui_hex));

		memcpy(gweui, buff_push + 4, 8);
		i82hexstr(gweui, gweui_hex, 8);

        /* lookupgweui: select gweui from gws where gweui = ? */
        if (!db_lookup_gweui(cntx.lookupgweui, gweui_hex)) {
<<<<<<< HEAD
	        MSG_DEBUG(DEBUG_WARNING, "WARNING: [up] GWEUI(%s) NOT REGISTER !\n", gweui_hex);
=======
	        MSG_DEBUG(DEBUG_INFO, "WARNING: [up] GWEUI(%s) NOT REGISTER !\n", gweui_hex);
>>>>>>> ecfb381ade916363034db6f48f1fa3f2c57029ab
            continue;
        }

		if (buff_push[3] == PKT_PUSH_DATA) {
			pkt_no++;
			pthread_mutex_lock(&mx_push);
			nb_push_rcv++;
			pthread_mutex_unlock(&mx_push);

			/*send PUSH_ACK to confirm the PUSH_DATA*/
			memcpy(buff_push_ack, buff_push, 3);
			buff_push_ack[3] = PKT_PUSH_ACK;

			if (sendto(sockfd_push, buff_push_ack, sizeof(buff_push_ack), 0, (struct sockaddr*)&cliaddr, addrlen) < 0)
				MSG_DEBUG(DEBUG_WARNING, "WARNING: [thread_up] send push_ack for packet_%d fail\n", pkt_no);
                        
            memset(&pkt_info, 0, sizeof(struct pkt_info));
			memcpy(pkt_info.pkt_payload, buff_push + 12, msg_len - 12);
			strcpy(pkt_info.gwaddr, inet_ntoa(cliaddr.sin_addr));
			strcpy(pkt_info.gweui_hex, gweui_hex);
			pkt_info.pkt_no = pkt_no;
			if (pthread_create(&th_up_handle, NULL, (void*)thread_up_handle, (void*)&pkt_info) != 0) {
				MSG_DEBUG(DEBUG_WARNING, "WARNING: [thread_up] impossible to create thread thread_up_handle\n");
                continue;
			}
		}

	}

	if (exit_sig == true) {
		/*shutdown all socket to alarm the threads from system call(like recvfrom()) */
		shutdown(sockfd_push, SHUT_RDWR);
		shutdown(sockfd_pull, SHUT_RDWR);

		/*free linked list*/
		list_destroy(&gw_list, destroy_msg_down);

        /*free database struct*/
        db_destroy(&cntx);
	}
	/*wait for threads to finish*/
	pthread_join(th_down, NULL);
	MSG_DEBUG(DEBUG_INFO, "INFO~ the main program on network server exit successfully\n");
	exit(EXIT_SUCCESS);
}

void signal_handle(int signo){
	if(signo == SIGINT || signo == SIGTERM || signo == SIGQUIT) {
		exit_sig = true;
		MSG("######################waiting for exiting#######################\n");
	}
	return;
}

/*handle upstream and recognize the type */
void thread_up_handle(void* pkt_info) {
	/* when this thread exits,it will automatically release all resources*/
	pthread_detach(pthread_self());

    struct pkt_info pkt;     
    memcpy(&pkt, pkt_info, sizeof(struct pkt_info)); /* copy the data from main thread, this data may be change */

	/*json parsing variables*/
	JSON_Value  *root_val = NULL;
	JSON_Array  *rxpk_arr = NULL;
	JSON_Object *rxpk_obj = NULL;
	JSON_Value  *val = NULL;

	int i, j, size;
	const char* str;
	uint8_t payload[256];/*256 is the max size defined in the specification*/
	char tempstr[32];

	struct metadata meta_data;
	struct jsondata json_result;/*keep the result of analyzing message from gateway*/

<<<<<<< HEAD
    MSG_DEBUG(DEBUG_INFO, "INFO~ [handle-up] Handle(%d): %s\n", pkt.pkt_no, pkt.pkt_payload);
=======
    MSG_DEBUG(DEBUG_INFO, "~INFO~ Handle(%d): %s\n", pkt.pkt_no, pkt.pkt_payload);
>>>>>>> ecfb381ade916363034db6f48f1fa3f2c57029ab

	/*parse JSON*/

	root_val = json_parse_string_with_comments((const char*)(pkt.pkt_payload));
	if (root_val == NULL) {
<<<<<<< HEAD
		MSG("WARNING: [handel-up] packet_%d push_data contains invalid JSON\n", pkt.pkt_no);
=======
		MSG("WARNING: [up] packet_%d push_data contains invalid JSON\n", pkt.pkt_no);
>>>>>>> ecfb381ade916363034db6f48f1fa3f2c57029ab
        goto thread_out;
	}

	rxpk_arr = json_object_get_array(json_value_get_object(root_val), "rxpk");
	if (rxpk_arr == NULL) {
<<<<<<< HEAD
	    //rxpk_arr = json_object_get_array(json_value_get_object(root_val), "stat");
        //if (rxpk_arr != NULL) 
         //   MSG_DEBUG(DEBUG_INFO, "INFO~ [handel-up] Receive a packet_%d for gw status\n", pkt.pkt_no);
=======
	    json_value_free(root_val);
	    root_val = json_parse_string_with_comments((const char*)(pkt.pkt_payload));
	    if (root_val == NULL)
            goto thread_out;
	    rxpk_arr = json_object_get_array(json_value_get_object(root_val), "stat");
        if (rxpk_arr != NULL) 
            MSG_DEBUG(DEBUG_INFO, "Receive a packet_%d push_data of gw status\n", pkt.pkt_no);
>>>>>>> ecfb381ade916363034db6f48f1fa3f2c57029ab
        goto thread_out;
	}

	/*traverse the rxpk array*/

	i = 0;

	while ((rxpk_obj = json_array_get_object(rxpk_arr, i)) != NULL) {
		memset(&json_result, 0, sizeof(json_result));
		memset(&meta_data, 0, sizeof(meta_data));
		strcpy(meta_data.gwaddr, pkt.gwaddr);
		strcpy(meta_data.gweui_hex, pkt.gweui_hex);


		val = json_object_get_value(rxpk_obj, "tmst");
		if (val != NULL) {
<<<<<<< HEAD
			meta_data.tmst = (uint32_t)json_value_get_number(val);
=======
			meta_data.tmst = (uint8_t)json_value_get_number(val);
>>>>>>> ecfb381ade916363034db6f48f1fa3f2c57029ab
		}

		val = json_object_get_value(rxpk_obj, "chan");
		if (val != NULL) {
			meta_data.chan = (uint8_t)json_value_get_number(val);
		}

		val = json_object_get_value(rxpk_obj, "rfch");
		if (val != NULL) {
			meta_data.rfch = (uint8_t)json_value_get_number(val);
		}
        
		val = json_object_get_value(rxpk_obj, "freq");
		if (val != NULL) {
			meta_data.freq = (double)json_value_get_number(val);
		}
        
		val = json_object_get_value(rxpk_obj, "stat");
		if (val != NULL){
			meta_data.stat = (uint8_t)json_value_get_number(val);
		}
        
		val = json_object_get_value(rxpk_obj, "modu");
		if (val != NULL){
			strcpy(meta_data.modu, json_value_get_string(val));
			if (strcmp("LORA", json_value_get_string(val))==0) {
				val = json_object_get_value(rxpk_obj, "datr");
				if (val != NULL){
					strcpy(meta_data.datrl, json_value_get_string(val));
				}
				val = json_object_get_value(rxpk_obj, "codr");
				if (val !=NULL ){
					strcpy(meta_data.codr, json_value_get_string(val));
				}
				val = json_object_get_value(rxpk_obj, "lsnr");
				if(val != NULL){
					meta_data.lsnr = (float)json_value_get_number(val);
				}
			} else if (strcmp("FSK", json_value_get_string(val)) == 0) {
				val = json_object_get_value(rxpk_obj, "datr");
				if (val != NULL ) {
					meta_data.datrf = (uint32_t)json_value_get_number(val);
				}
			}
		}
        
		val = json_object_get_value(rxpk_obj, "rssi");
		if (val != NULL) {
			meta_data.rssi = (float)json_value_get_number(val);
		}
        
		val = json_object_get_value(rxpk_obj, "size");
		if (val != NULL) {
			meta_data.size = (uint16_t)json_value_get_number(val);
			size = json_value_get_number(val);
		}
        
		val = json_object_get_value(rxpk_obj, "data");
		if (val != NULL) {
			str = json_value_get_string(val);
			if (b64_to_bin(str, strlen(str), payload, sizeof(payload)) != size){
<<<<<<< HEAD
				MSG_DEBUG(DEBUG_WARNING, "WARNING: [handel-up] in packet_%d rxpk_%d mismatch between \"size\" and the real size once converter to binary\n", pkt.pkt_no, i);
			}
		} else {
			MSG_DEBUG(DEBUG_WARNING, "WARNING: [handel-up] in packet_%d rxpk_%d contains no data\n", pkt.pkt_no, i);
=======
				MSG_DEBUG(DEBUG_WARNING, "WARNING: [up] in packet_%d rxpk_%d mismatch between \"size\" and the real size once converter to binary\n", pkt.pkt_no, i);
			}
		} else {
			MSG_DEBUG(DEBUG_WARNING, "WARNING: [up] in packet_%d rxpk_%d contains no data\n", pkt.pkt_no, i);
>>>>>>> ecfb381ade916363034db6f48f1fa3f2c57029ab
		}

		/*analysis the MAC payload content*/
		pthread_mutex_lock(&mx_db);
		ns_msg_handle(&json_result, &meta_data, payload);/*this function will access the database*/
		pthread_mutex_unlock(&mx_db);
                
        /* insert msg_down into gw_list */

		if (json_result.to != IGNORE) {
			pthread_mutex_lock(&mx_gw_list);
			list_insert_at_tail(&gw_list, json_result.msg_down, sizeof(struct msg_down), assign_msg_down);
            free(json_result.msg_down);
			pthread_mutex_unlock(&mx_gw_list);
		}
                
		if (++i >= NB_PKT_MAX) break;

	}

thread_out:
    //sprintf(tempstr, "EXIT HANDLE PKT%d", pkt.pkt_no);
<<<<<<< HEAD
    MSG_DEBUG(DEBUG_INFO, "INFO~ [handel-up] EXIT HANDLE PKT%d\n", pkt.pkt_no);
=======
    MSG_DEBUG(DEBUG_INFO, "INFO~ EXIT HANDLE PKT%d\n", pkt.pkt_no);
>>>>>>> ecfb381ade916363034db6f48f1fa3f2c57029ab
	json_value_free(root_val);
	pthread_exit(NULL);
}

void thread_down(void) {
	int pkt_no = 0;
	int msg_len;/*length of buffer received*/
	uint8_t buff_pull[12]; /* buffer of PULL_DATA*/
	uint8_t buff_pull_ack[4];/*buffer to confirm the PULL_DATA*/
	uint8_t buff_pull_resp[JSON_MAX + 4];/*buffer of PULL_RESP*/
	struct sockaddr_in cliaddr;
	socklen_t addrlen;
	char gwaddr[16];
	char jsonstr[JSON_MAX];
	struct msg_down data;
	bool flag;

	memset(&cliaddr, 0, sizeof(cliaddr));
	addrlen = sizeof(cliaddr);
	data.gwaddr = malloc(16);
	data.json_string = malloc(JSON_MAX);
	/*receive pull_data message from gateway*/
	while (!exit_sig) {
		msg_len = recvfrom(sockfd_pull, buff_pull, sizeof(buff_pull), 0, (struct sockaddr*)&cliaddr, &addrlen);
		if (msg_len < 0) {
			//MSG("WARNING: [down] thread_down recv returned %s\n", strerror(errno));
			continue;
		}
		if (msg_len == 0) {
			if(exit_sig == true){
				/*thread_down socket is shut down*/
				break;
			} else {
				MSG_DEBUG(DEBUG_WARNING, "WARNING: [down] the data size is 0 ,just ignore\n");
				continue;
			}
		}
		/* if the datagram does not respect the format of PULL DATA, just ignore it */
		if (msg_len < 4 || buff_pull[0] != VERSION || buff_pull[3] != PKT_PULL_DATA){
<<<<<<< HEAD
			MSG_DEBUG(DEBUG_WARNING, "WARNING: [down] pull data invalid,just ignore\n");
=======
			MSG("WARNING: [down] pull data invalid,just ignore\n");
>>>>>>> ecfb381ade916363034db6f48f1fa3f2c57029ab
			continue;
		}


		if (buff_pull[3] == PKT_PULL_DATA) {
			pkt_no++;
			strcpy(gwaddr, inet_ntoa(cliaddr.sin_addr));
			pthread_mutex_lock(&mx_gw_list);
			flag = list_search_and_delete(&gw_list, gwaddr, &data, compare_msg_down, copy_msg_down, destroy_msg_down);
			pthread_mutex_unlock(&mx_gw_list);
			if (flag == false) {
				memcpy(buff_pull_ack, buff_pull, 3);
				buff_pull_ack[3] = PKT_PULL_ACK;
				if (sendto(sockfd_pull, buff_pull_ack, sizeof(buff_pull_ack), 0, (struct sockaddr*)&cliaddr, addrlen) < 0)
					MSG_DEBUG(DEBUG_WARNING, "WARNING: [down] send pull_ack for packet_%d fail\n", pkt_no);
			} else {
				strcpy(jsonstr, data.json_string);
				memcpy(buff_pull_resp, buff_pull, 3);
				buff_pull_resp[3] = PKT_PULL_RESP;
				memcpy(buff_pull_resp + 4, jsonstr, strlen(jsonstr) + 1);
				if (sendto(sockfd_pull, buff_pull_resp, sizeof(buff_pull_resp), 0, (struct sockaddr*)&cliaddr, addrlen) < 0)
					MSG_DEBUG(DEBUG_WARNING, "WARNING: [dwon] send pull_resp for packet_%d fail\n", pkt_no);
				MSG("INFO: [down] send pull_resp successfully\n");
			}
		}
	}
	free(data.gwaddr);
	free(data.json_string);
	MSG("INFO: thread_down exit successfully\n");
}

