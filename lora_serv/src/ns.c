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

/* --- PRIVATE VARIABLES ------------------------------------------- */
/*network configuration variables*/
static char appserv_addr[64] = STR(APP_SERV_ADDR);
static char netserv_addr[64] = STR(NET_SERV_ADDR);
static char ncserv_addr[64] = STR(NC_SERV_ADDR);

//static char gwserv_addr[64]=STR(GW_SERV_ADDR);
static char netserv_port_push[8] = STR(NET_PORT_PUSH);
static char netserv_port_pull[8] = STR(NET_PORT_PULL);
static char appserv_port[8] = STR(APP_PORT_UP);
static char netserv_port_foras[8] = STR(APP_PORT_DOWN);
static char ncserv_port[8] = STR(NC_PORT_UP);
static char netserv_port_fornc[8] = STR(NC_PORT_DOWN);

/* network sockets */
static int sockfd_app_up;/* socket for upstream to application server*/
static int sockfd_nc_up; /* socket for  upstream to network control server*/
static int sockfd_push;/*socket for upstream from gateway*/
static int sockfd_pull;/*socket for downstream to gateway*/
static int listenfd_app_down;/*listening sockfd for downstream from application server*/
static int listenfd_nc_down;/*listening sockfd for downstream from network controller*/

static int connfd_as_down;/*connected sockfd for downstream from application server*/
static int connfd_nc_down;/*connected sockfd for downstream from network controller*/
static int connfds_as[CONNFD_NUM_MAX];
static int connfds_nc[CONNFD_NUM_MAX];
static int conn_num_as = 0;/*the number of tcp connections with application servers*/
static int conn_num_nc = 0;/*the number of tcp connections with application servers*/

/*linked list used for storing json sting for application server and network controller*/
//static pnode as_list_head;/*head of the linked list storing json string for upstream to AS*/
//static pnode nc_list_head;/*head of the linked list storing json string for upstream to NC*/
//static pnode gw_list_head;/*head of the linked list storing json string for downstream to gateway*/
static linked_list as_list;
static linked_list nc_list;
static linked_list gw_list;
static linked_list delay_list;
static linked_list join_list;

/*mutex lock for controlling access to the linked list*/
static pthread_mutex_t mx_as_list = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mx_nc_list = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mx_gw_list = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mx_delay_list = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mx_join_list = PTHREAD_MUTEX_INITIALIZER;

/*mutex lock for controlling access to the database*/
static pthread_mutex_t mx_db = PTHREAD_MUTEX_INITIALIZER;

/*mutex lock for controlling rewriting the global variable conn_num_as&&conn_num_nc*/
static pthread_mutex_t mx_conn_as;
static pthread_mutex_t mx_conn_nc;

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
void  thread_up(void);/* thread for receiving and confirming upstream from gateway*/
void  thread_down(void);/*thread for send downstream to gateway*/
void  thread_as_up(void);/*thread for handling upstream communicating with application server*/
void  thread_nc_up(void);/*thread for handling upstream communicating with network controller*/
void  thread_as_down(void);/*thread for handling downstream communicating with application server*/
void  thread_nc_down(void);/*thread for handling downstream communicating with network controller*/
void  thread_up_handle(void*);/*thread for handling message in upstream from gateway */
void  thread_as_down_handle(void*);/*thread for recving and handling message from one application server*/
void  thread_nc_down_handle(void*);/*thread for recving and handling message from one network controller*/
void  thread_list_check(void*);/*thread for checking the delay_list and packet data if needed*/

/* ----------------------------------------------------------------------------------------------------- */
/* ------------------------------------------- MAIN FUNCTION ------------------------------------------- */
int main(int argc, char** argv) {

	int pkt_no = 0;
	int msg_len;/*length of buffer received*/

	uint8_t gwEui[8];
        char gwEui_hex[17] = {'\0'};
        char output[MAXSTRSIZE] = {'\0'};

	uint8_t buff_push[BUFF_SIZE]; /* buffer to receive  upstream packets */
	uint8_t buff_push_ack[4];/*buffer to confirm the upstream packets*/

	pthread_t th_up_handle;/*thread for handing the json string in upstream*/

	struct pkt_info pkt_info;

        struct context cntx; /* sqlite3 database context */

	struct sockaddr_in cliaddr;
	socklen_t addrlen;

	bzero(&cliaddr, sizeof(cliaddr));
	addrlen = sizeof(cliaddr);

	/* threads*/
	pthread_t  th_down;/*thread for downstream to gateway*/
	pthread_t  th_as_up;/*thread for upstream communicating with application server*/
	pthread_t  th_nc_up;/*thread for upstream communicating with network controller*/
	pthread_t  th_as_down;/*thread for downstream communicating with application server*/
	pthread_t  th_nc_down;/*thread for downstream communicating with network controller*/

	int i;
	struct sigaction sigact;

	/* display host endianness infomation*/
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		MSG("INFO: Little endian host\n");
	#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		MSG("INFO: Big endian host\n");
	#else
		MSG("INFO: Host endiannes is unknown\n");
	#endif

	/*create the linked list for NC and AS*/
	//as_list_head=create_linklist();
	//nc_list_head=create_linklist();
	//gw_list_head=create_linklist();
	list_init(&as_list);
	list_init(&nc_list);
	list_init(&gw_list);
	list_init(&delay_list);
	list_init(&join_list);

	/*try to open and bind udp socket*/
	udp_bind(netserv_addr, netserv_port_push, &sockfd_push);
	udp_bind(netserv_addr, netserv_port_pull, &sockfd_pull);
        
	/*initialize the array of connected socket*/
	for(i=0; i < CONNFD_NUM_MAX; i++){
		connfds_as[i] = -1;
		connfds_nc[i] = -1;
	}

	/*configure the signal handling*/
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_NOMASK;
	sigact.sa_handler = signal_handle;
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	/*create threads*/

	if (pthread_create(&th_down, NULL, (void*(*)(void*))thread_down, NULL) !=0 ) {
		MSG("ERROR: [main] impossible to create thread for downstream communicating from gateway\n");
		exit(EXIT_FAILURE);
	}
	if (pthread_create(&th_as_up, NULL, (void*(*)(void*))thread_as_up, NULL) != 0) {
		MSG("ERROR: [main] impossible to create thread for upstream communicating with application server\n");
		exit(EXIT_FAILURE);
	}
	if (pthread_create(&th_nc_up, NULL, (void*(*)(void*))thread_nc_up, NULL) != 0) {
		MSG("ERROR: [main] impossible to create thread for upstream communicating with network controller\n");
		exit(EXIT_FAILURE);
	}
	if (pthread_create(&th_as_down, NULL, (void*(*)(void*))thread_as_down, NULL) != 0) {
		MSG("ERROR: [main] impossible to create thread for downstream communicating with application server\n");
		exit(EXIT_FAILURE);
	 }
	if (pthread_create(&th_nc_down, NULL, (void*(*)(void*))thread_nc_down, NULL) != 0) {
		MSG("ERROR: [main] impossible to create thread for downstream communicating with network controller\n");
		exit(EXIT_FAILURE);
	}

	while (!exit_sig) {        /* main thread, dispatch PUSH_DATA and PULL_DATA */
		msg_len = recvfrom(sockfd_push, buff_push, sizeof(buff_push), 0, (struct sockaddr*)&cliaddr, &addrlen);
		if (msg_len < 0) {
			MSG("WARNING: [up] thread_up recv returned %s\n", strerror(errno));
			break;
		}

		if (msg_len == 0) {
			if (exit_sig == true){
				/*thread_up socket is shut down*/
				break;
			} else {
				MSG("WARNING: [up] the data size is 0 ,just ignore\n");
				continue;
			}
		}
		/* if the datagram does not respect the format of PUSH DATA, just ignore it */
		if (msg_len < 12 || buff_push[0] != VERSION || ((buff_push[3] != PKT_PUSH_DATA) && buff_push[3] != PKT_PULL_DATA)){
                        /* too short for GW <-> MAC protocol */
			MSG("WARNING: [up] push data invalid, ignore!\n");
			continue;
		}

		revercpy(gwEui, buff_push + 4, 8);
		i8_to_hexstr(gwEui, gwEui_hex, 8);

                /* selectgweui: select gweui from gateways where gweui = ? */
                if (!db_lookup_str(cntx->selectgweui, gwEui_hex, output)) {
			MSG("WARNING: [up] Not a valid GatewayEUI, ignore!\n");
                        continue;
                }

		if (buff_push[3] == PKT_PUSH_DATA) {
			MSG("INFO: [up]receive a push data\n");
			pkt_no++;
			pthread_mutex_lock(&mx_push);
			nb_push_rcv++;
			pthread_mutex_unlock(&mx_push);

			/*send PUSH_ACK to confirm the PUSH_DATA*/
			memcpy(buff_push_ack, buff_push, 3);
			buff_push_ack[3] = PKT_PUSH_ACK;

			if (sendto(sockfd_push, buff_push_ack, sizeof(buff_push_ack), 0, (struct sockaddr*)&cliaddr, addrlen) < 0)
				MSG("WARNING: [thread_up] send push_ack for packet_%d fail\n", pkt_no);
                        
			memcpy(pkt_info.pkt_payload, buff_push + 12, msg_len - 12);
			strcpy(pkt_info.gwaddr, inet_ntoa(cliaddr.sin_addr));
			pkt_info.pkt_no = pkt_no;
			if (pthread_create(&th_up_handle, NULL, (void*)thread_up_handle, (void*)&pkt_info) != 0) {
				MSG("ERROR: [thread_up] impossible to create thread for forwarding\n");
				exit(EXIT_FAILURE);
			}
		}

	}

	if (exit_sig == true) {
		/*shutdown all socket to alarm the threads from system call(like recvfrom()) */
		shutdown(sockfd_push, SHUT_RDWR);
		shutdown(sockfd_pull, SHUT_RDWR);
		shutdown(sockfd_app_up, SHUT_RDWR);
		shutdown(sockfd_nc_up, SHUT_WR);

		/*shutdown all listening sockfd, TCP */
		shutdown(listenfd_app_down, SHUT_RDWR);
		shutdown(listenfd_nc_down, SHUT_RDWR);

		/*shutdown all connected sockfd*/
		for (i = 0; i < CONNFD_NUM_MAX; i++) {
			if (connfds_as[i] == -1) {
				break;
			}
			shutdown(connfds_as[i], SHUT_RDWR);
		}
		for (i = 0; i < CONNFD_NUM_MAX; i++) {
			if (connfds_nc[i] == -1) {
				break;
			}
			shutdown(connfds_nc[i], SHUT_RDWR);
		}
		/*free linked list*/
		list_destroy(&as_list, destroy_msg);
		list_destroy(&nc_list, destroy_msg);
		list_destroy(&gw_list, destroy_msg_down);
		list_destroy(&delay_list, destroy_msg_delay);
		list_destroy(&join_list, NULL);
	}
	/*wait for threads to finish*/
	pthread_join(th_up, NULL);
	pthread_join(th_down, NULL);
	pthread_join(th_as_up, NULL);
	pthread_join(th_nc_up, NULL);
	pthread_join(th_as_down, NULL);
	pthread_join(th_nc_down, NULL);
	MSG("INFO: the main program on network server exit successfully\n");
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
	struct pkt_info *pkt = (struct pkt_info*)pkt_info;

	/*json parsing variables*/
	JSON_Value  *root_val = NULL;
	JSON_Array  *rxpk_arr = NULL;
	JSON_Object *rxpk_obj = NULL;
	JSON_Value  *val = NULL;

	int i, j, size;
	char content[BUFF_SIZE];
	const char* str;
	uint8_t payload[256];/*256 is the max size defined in the specification*/
	char rxpk_name[7];
	char msgname[64];
	char tempstr[32];
	char tempdata[300];
	int pkt_no = pkt->pkt_no;
	struct metadata meta_data;
	struct jsondata json_result;/*keep the result of analyzing message from gateway*/
	struct msg msg_to_as;
	struct msg msg_to_nc;
	struct msg_join data_join;
	int msg_size;
	pthread_t th_check;
	struct th_check_arg th_arg;

	/*parse JSON*/
	bzero(content, sizeof(content));
	root_val = json_parse_string_with_comments((const char*)(pkt->pkt_payload));
	if (root_val == NULL) {
		MSG("WARNING: [up] packet_%d push_data contains invalid JSON\n", pkt_no);
		json_value_free(root_val);
	}
	rxpk_arr = json_object_get_array(json_value_get_object(root_val), "rxpk");
	if (rxpk_arr == NULL) {
		MSG("WARNING: [up] packet_%d push_data contains no \"rxpk\" array in JSON\n", pkt_no);
		json_value_free(root_val);
	}

	/*traverse the rxpk array*/
	snprintf(msgname, sizeof(msgname), "###############push data_%d:###############\n", pkt_no);
	strcat(content, msgname);
	i = 0;
	while ((rxpk_obj = json_array_get_object(rxpk_arr, i)) != NULL) {
		bzero(&json_result, sizeof(json_result));
		bzero(&meta_data, sizeof(meta_data));
		strcpy(meta_data.gwaddr, pkt->gwaddr);
		snprintf(rxpk_name, sizeof(rxpk_name), "rxpk_%d", i);
		strcat(content, rxpk_name);
		val = json_object_get_value(rxpk_obj, "tmst");
		if (val != NULL) {
			meta_data.tmst = (uint32_t)json_value_get_number(val);
			tmst_g = meta_data.tmst;
			snprintf(tempstr, sizeof(tempstr), " tmst:%.0f", json_value_get_number(val));
			strcat(content, tempstr);
		}
		val = json_object_get_value(rxpk_obj, "time");
		if (val != NULL) {
			strcpy(meta_data.time, json_value_get_string(val));
			strcat(content, " time:");
			strcat(content, json_value_get_string(val));
		}
		val = json_object_get_value(rxpk_obj, "chan");
		if (val != NULL){
			meta_data.chan = (uint8_t)json_value_get_number(val);
			snprintf(tempstr, sizeof(tempstr), " chan:%.0f", json_value_get_number(val));
			strcat(content, tempstr);
		}
		val = json_object_get_value(rxpk_obj, "rfch");
		if (val != NULL) {
			meta_data.rfch = (uint8_t)json_value_get_number(val);
			snprintf(tempstr, sizeof(tempstr), " rfch:%.0f", json_value_get_number(val));
			strcat(content, tempstr);
		}
		val = json_object_get_value(rxpk_obj, "freq");
		if (val != NULL) {
			meta_data.freq = (double)json_value_get_number(val);
			snprintf(tempstr, sizeof(tempstr), " freq:%.6lf", json_value_get_number(val));
			strcat(content, tempstr);
		}
		val = json_object_get_value(rxpk_obj, "stat");
		if (val != NULL){
			meta_data.stat = (uint8_t)json_value_get_number(val);
			snprintf(tempstr, sizeof(tempstr), " stat:%.0f", json_value_get_number(val));
			strcat(content, tempstr);
		}
		val = json_object_get_value(rxpk_obj, "modu");
		if (val != NULL){
			strcpy(meta_data.modu, json_value_get_string(val));
			strcat(content, " modu:");
			strcat(content, json_value_get_string(val));
			if (strcmp("LORA", json_value_get_string(val))==0) {
				val = json_object_get_value(rxpk_obj, "datr");
				if (val != NULL){
					strcpy(meta_data.datrl, json_value_get_string(val));
					strcat(content, " datr:");
					strcat(content, json_value_get_string(val));
				}
				val = json_object_get_value(rxpk_obj, "codr");
				if (val !=NULL ){
					strcpy(meta_data.codr, json_value_get_string(val));
					strcat(content, " codr:");
					strcat(content, json_value_get_string(val));
				}
				val = json_object_get_value(rxpk_obj, "lsnr");
				if(val != NULL){
					meta_data.lsnr = (float)json_value_get_number(val);
					snprintf(tempstr, sizeof(tempstr), " lsnr:%.2f", json_value_get_number(val));
					strcat(content, tempstr);
				}
			} else if (strcmp("FSK", json_value_get_string(val)) == 0) {
				val = json_object_get_value(rxpk_obj, "datr");
				if (val != NULL ) {
					meta_data.datrf = (uint32_t)json_value_get_number(val);
					snprintf(tempstr, sizeof(tempstr), " datr:%.2f", json_value_get_number(val));
					strcat(content, tempstr);
				}
			}
		}
		val = json_object_get_value(rxpk_obj, "rssi");
		if (val != NULL) {
			meta_data.rssi = (float)json_value_get_number(val);
			snprintf(tempstr, sizeof(tempstr), " rssi:%.0f", json_value_get_number(val));
			strcat(content, tempstr);
		}
		val = json_object_get_value(rxpk_obj, "size");
		if (val != NULL) {
			meta_data.size = (uint16_t)json_value_get_number(val);
			size = json_value_get_number(val);
			snprintf(tempstr, sizeof(tempstr), " size:%d", size);
			strcat(content, tempstr);
		}
		val = json_object_get_value(rxpk_obj, "data");
		if (val != NULL) {
			str = json_value_get_string(val);
			if (b64_to_bin(str, strlen(str), payload, sizeof(payload)) != size){
				MSG("WARNING: [up] in packet_%d rxpk_%d mismatch between \"size\" and the real size once converter to binary\n", pkt_no, i);
			}
			strcat(content, " data:");
			for (j=0; j<size; j++){
				snprintf(tempdata, sizeof(tempdata), "0x%02x ", payload[j]);
				strcat(content, tempdata);
			}
		} else {
			MSG("WARNING: [up] in packet_%d rxpk_%d contains no data\n", pkt_no, i);
		}
		MSG("%s\n", content);
		/*analysis the MAC payload content*/
		pthread_mutex_lock(&mx_db);
		ns_msg_handle(&json_result, &meta_data, payload);/*this function will access the database*/
		pthread_mutex_unlock(&mx_db);
		/*create a thread to check the linked list waiting for packet*/
		if (json_result.to != IGNORE && json_result.join == false) {
			th_arg.devaddr = json_result.devaddr;
			th_arg.tmst = meta_data.tmst;
			pthread_create(&th_check, NULL,(void*)thread_list_check, (void*)&th_arg);
		}
		if (json_result.to != IGNORE && json_result.join == true) {
			strcpy(data_join.deveui_hex, json_result.deveui_hex);
			data_join.tmst = meta_data.tmst;
			pthread_mutex_lock(&mx_join_list);
			list_search_and_update(&join_list, data_join.deveui_hex, &data_join, sizeof(struct msg_join), compare_msg_join, assign_msg_join);
			pthread_mutex_unlock(&mx_join_list);
		}
		/*store the json string in the linked list*/
		if (json_result.to == APPLICATION_SERVER || json_result.to == BOTH) {
			msg_to_as.json_string = malloc(strlen(json_result.json_string_as) + 1);
			strcpy(msg_to_as.json_string, json_result.json_string_as);
			msg_size = sizeof(msg_to_as);
			pthread_mutex_lock(&mx_as_list);
			list_insert_at_tail(&as_list, &msg_to_as, msg_size, assign_msg);
			pthread_mutex_unlock(&mx_as_list);
		}
		if (json_result.to == NETWORK_CONTROLLER || json_result.to == BOTH) {
			/*the memory will be free when the linked node is deleted*/
			msg_to_nc.json_string = malloc(strlen(json_result.json_string_nc) + 1);
			strcpy(msg_to_nc.json_string, json_result.json_string_nc);
			msg_size = sizeof(msg_to_nc);
			pthread_mutex_lock(&mx_nc_list);
			list_insert_at_tail(&nc_list, &msg_to_nc, msg_size, assign_msg);
			pthread_mutex_unlock(&mx_nc_list);
		}
                
		i++;

		if (i >= NB_PKT_MAX) break;

		bzero(content, sizeof(content));
	}
	json_value_free(root_val);
	pthread_exit("pthread_up_handle exit");
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

	bzero(&cliaddr,sizeof(cliaddr));
	addrlen = sizeof(cliaddr);
	data.gwaddr = malloc(16);
	data.json_string = malloc(JSON_MAX);
	/*receive pull_data message from gateway*/
	while (!exit_sig) {
		msg_len = recvfrom(sockfd_pull, buff_pull, sizeof(buff_pull), 0, (struct sockaddr*)&cliaddr, &addrlen);
		if (msg_len < 0) {
			MSG("WARNING: [down] thread_down recv returned %s\n", strerror(errno));
			break;
		}
		if (msg_len == 0) {
			if(exit_sig == true){
				/*thread_down socket is shut down*/
				break;
			} else {
				MSG("WARNING: [down] the data size is 0 ,just ignore\n");
				continue;
			}
		}
		/* if the datagram does not respect the format of PULL DATA, just ignore it */
		if (msg_len < 4 || buff_push[0] != VERSION || ((buff_push[3] != PKT_PUSH_DATA) && buff_push[3] != PKT_PULL_DATA)){
			MSG("WARNING: [down] pull data invalid,just ignore\n");
			continue;
		}
		if (buff_pull[3] == PKT_PULL_DATA) {
			pkt_no++;
			strcpy(gwaddr, inet_ntoa(cliaddr.sin_addr));
			pthread_mutex_lock(&mx_gw_list);
			//flag=find_node_x(gw_list_head,gwaddr,jsonstr);
			flag = list_search_and_delete(&gw_list, gwaddr, &data, compare_msg_down, copy_msg_down, destroy_msg_down);
			pthread_mutex_unlock(&mx_gw_list);
			if (flag == false) {
				memcpy(buff_pull_ack, buff_pull, 3);
				buff_pull_ack[3] = PKT_PULL_ACK;
				if (sendto(sockfd_pull, buff_pull_ack, sizeof(buff_pull_ack), 0, (struct sockaddr*)&cliaddr, addrlen) < 0)
					MSG("WARNING: [thread_down] send pull_ack for packet_%d fail\n", pkt_no);
			} else {
				strcpy(jsonstr, data.json_string);
				memcpy(buff_pull_resp, buff_pull, 3);
				buff_pull_resp[3] = PKT_PULL_RESP;
				memcpy(buff_pull_resp + 4, jsonstr, strlen(jsonstr) + 1);
				if (sendto(sockfd_pull, buff_pull_resp, sizeof(buff_pull_resp), 0, (struct sockaddr*)&cliaddr, addrlen) < 0)
					MSG("WARNING: [thread_up] send pull_resp for packet_%d fail\n", pkt_no);
				MSG("INFO: [down] send pull_resp successfully\n");
			}
		}
	}
	free(data.gwaddr);
	free(data.json_string);
	MSG("INFO: thread_down exit successfully\n");
}

void thread_as_up(void) {
	//char pkt_send[JSON_MAX];
	struct pkt pkt_send;
	struct msg* data;
	reconnect: tcp_connect(appserv_addr, appserv_port, &sockfd_app_up, &exit_sig);
	while (!exit_sig) {
		/*read the content of linked list and send it*/
		bzero(&pkt_send, sizeof(pkt_send));
		pthread_mutex_lock(&mx_as_list);
		while (list_is_empty(&as_list) == false) {
			data = (struct msg*)((as_list.head)->data);
			strcpy(pkt_send.json_content, data->json_string);
			MSG(">>>>\n");
			if (send(sockfd_app_up, &pkt_send, sizeof(pkt_send), 0) < 0) {
				MSG("WARNING: [as_up] failed to send data to application server\n");
				MSG("INFO: trying to reconnect to the application server...\n");
				pthread_mutex_unlock(&mx_as_list);
				goto reconnect;
			} else {
				list_delete_at_head(&as_list, destroy_msg);
			}
		}
		pthread_mutex_unlock(&mx_as_list);
		/*set timer for preparing data in thread_up_handle */
		set_timer(SEND_INTERVAL_S, SEND_INTERVAL_MS);
	}

	MSG("INFO: thread_as_up exit successfully\n");
}
void thread_nc_up(void) {
	struct pkt pkt_send;
	struct msg* data;
	reconnect:tcp_connect(ncserv_addr, ncserv_port, &sockfd_nc_up, &exit_sig);
	while (!exit_sig) {
		/*read the content of linked list and send*/
		bzero(&pkt_send, sizeof(pkt_send));
		pthread_mutex_lock(&mx_nc_list);
		while (list_is_empty(&nc_list) == false) {
			data = (struct msg*)(((nc_list.head)->data));
			MSG(">>>>>>>>\n");
			strcpy(pkt_send.json_content, data->json_string);
			if (send(sockfd_nc_up, &pkt_send, sizeof(pkt_send), 0) < 0){
				MSG("WARNING: [nc_up] failed to send data to network controller\n");
				MSG("INFO: trying to reconnect to network controller...\n");
				pthread_mutex_unlock(&mx_nc_list);
				goto reconnect;
			} else {
				list_delete_at_head(&nc_list, destroy_msg);
			}
		}
		pthread_mutex_unlock(&mx_nc_list);
		/*set timer for preparing data in thread_up_handle */
		set_timer(SEND_INTERVAL_S, SEND_INTERVAL_MS);
	}
	MSG("INFO: thread_nc_up exit successfully\n");
}
/*receive the downstream from application server */
void thread_as_down() {
	int j;
	struct sockaddr_in cliaddr;
	socklen_t clilen;
	pthread_t th_as_down_handle;
	tcp_bind(netserv_addr, netserv_port_foras, &listenfd_app_down);
	listen(listenfd_app_down, LISTENQ);
	while (!exit_sig) {
		if ((connfd_as_down = accept(listenfd_app_down, (struct sockaddr*)&cliaddr, &clilen)) < 0) {
			if (exit_sig != true) {
				MSG("ERROR: [down] thread_as_dwon failed to accept:%s", strerror(errno));
			} else {
				MSG("INFO: listening socketfd is shut down\n");
			}
			break;
		}
		/*number of connections add 1*/
		pthread_mutex_lock(&mx_conn_as);
		conn_num_as++;
		pthread_mutex_unlock(&mx_conn_as);
		struct arg arg;
		arg.connfd = connfd_as_down;
		connfds_as[j] = connfd_as_down;
		/*create threads*/
		if (pthread_create(&th_as_down_handle, NULL, (void*)thread_as_down_handle, (void*)&arg) != 0) {
			MSG("ERROR: [thread_up] impossible to create thread for receiving and handling upstream message from application server\n");
			exit(EXIT_FAILURE);
		}
		if (conn_num_as == CONNFD_NUM_MAX) {
			break;
		}
	}
	close(listenfd_app_down);
}

void thread_as_down_handle(void* arg) {
	pthread_detach(pthread_self());

	struct arg* conn = (struct arg*)arg;
	//char pkt_recv[JSON_MAX];
	int msg_len;
	int pkt_no = 0;
	struct pkt pkt_recv;
	struct msg_down msg_to_gw;
	struct msg_join data_join;
	int msg_size;
	unsigned int class;
	uint32_t  tmst;
	int delay;

	/*json parsing variables*/
	JSON_Value  *root_val = NULL;
	JSON_Object *join_obj = NULL;
	JSON_Object *accept_obj = NULL;
	JSON_Value  *val = NULL;
	JSON_Value  *root_val_x = NULL;
	JSON_Object *root_obj_x = NULL;

	char content[1024];/*1024 is big enough*/
	char tempstr[64];
	char msgname[64];
	char gwaddr[16];
	char appeui_hex[17];/* 2*8+1 */
	char deveui_hex[17];
	char nwkskey_hex[33];/* 2*16+1 */
	char frame_payload[FRAME_LEN];
	char json_data[512];

        sqlite3* db;

	struct timespec time;/*storing local timestamp*/

	while (!exit_sig) {
		if ((msg_len = recv(conn->connfd, &pkt_recv, sizeof(pkt_recv), 0)) < 0){
			MSG("WARNING: [up] thread_as_down_handle recv returned %s\n", strerror(errno));
			break;
		}
		if (msg_len == 0) {
			if (exit_sig == true) {
				/*socket is shut down*/
				MSG("INFO: the connected socket is shut down\n ");
			} else {
				/*number of connections -1*/
				pthread_mutex_lock(&mx_conn_as);
				conn_num_as--;
				pthread_mutex_unlock(&mx_conn_as);
				MSG("WARNING: [down] the application server is close,the corresponding connected socket is %d \n", conn->connfd);
			}
			break;
		}
//		MSG(pkt_recv);
//		MSG("\n");
		pkt_no++;

		/*handling join accpet message*/
		bzero(content, sizeof(content));
		root_val = json_parse_string_with_comments((const char*)(pkt_recv.json_content));
		if (root_val == NULL) {
			MSG("WARNING: [down] message_%d downstream from application server contains invalid JSON\n", pkt_no);
			json_value_free(root_val);
		}
		join_obj = json_object_get_object(json_value_get_object(root_val), "join");
		if (join_obj == NULL) {
			MSG("WARNING: [down] message_%d downstream from application server contains no \"join\" object \n", pkt_no);
			json_value_free(root_val);
		} else {
			snprintf(msgname, sizeof(msgname),"###############join_accept_%d:###############\n", pkt_no);
			strcat(content, msgname);
			val = json_object_get_value(join_obj, "gwaddr");
			if (val != NULL) {
				strcpy(gwaddr, json_value_get_string(val));
				snprintf(tempstr, sizeof(tempstr), "gwaddr:%s", json_value_get_string(val));
				strcat(content, tempstr);
			}
			val = json_object_get_value(join_obj, "appeui");
			if (val != NULL) {
				strcpy(appeui_hex, json_value_get_string(val));
				snprintf(tempstr, sizeof(tempstr), " appeui:%s", json_value_get_string(val));
				strcat(content, tempstr);
			}
			val = json_object_get_value(join_obj, "deveui");
			if (val != NULL) {
				strcpy(deveui_hex, json_value_get_string(val));
				snprintf(tempstr, sizeof(tempstr), " deveui:%s", json_value_get_string(val));
				strcat(content, tempstr);
			}
			accept_obj = json_object_get_object(join_obj, "accept");
			if (accept_obj == NULL) {
				MSG("WARNING:[down] join-accept message received contains no \"accept\" in \"join\"\n");
			} else {
				val = json_object_get_value(accept_obj, "frame");
				if (val != NULL) {
					strcpy(frame_payload, json_value_get_string(val));
					snprintf(tempstr, sizeof(tempstr), " frame:%s", json_value_get_string(val));
					strcat(content, tempstr);
				}
				val = json_object_get_value(accept_obj, "nwkskey");
				if (val != NULL){
					strcpy(nwkskey_hex, json_value_get_string(val));
					/*try to update the nwkskey in the table nsdevinfo*/
                                        if (sqlite3_open("/tmp/loraserv", &db)) {
                                                fprintf(stderr, "ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
                                                sqlite3_close(db);
						json_value_free(root_val);
						bzero(&pkt_recv, sizeof(pkt_recv));
						continue;
                                        }

					pthread_mutex_lock(&mx_db);

					if (update_db_by_deveui(db, "nwkskey", "nsdevinfo", deveui_hex, nwkskey_hex, 0) == FAILED) {
						/*this join-accpt message will be ignored*/
						MSG("WARNING: [down] update the database failed\n");
						pthread_mutex_unlock(&mx_db);
						json_value_free(root_val);
						bzero(&pkt_recv, sizeof(pkt_recv));
                                                sqlite3_close(db);
						continue;
					}
					if (query_db_by_deveui_uint(db, "class", "nsdevinfo", deveui_hex, &class) == FAILED) {
						MSG("WARNING: [down] update the database failed\n");
						pthread_mutex_unlock(&mx_db);
						json_value_free(root_val);
						bzero(&pkt_recv, sizeof(pkt_recv));
                                                sqlite3_close(db);
						continue;
					}
					pthread_mutex_unlock(&mx_db);
                                        sqlite3_close(db);
					snprintf(tempstr, sizeof(tempstr), " nwkskey:%s", json_value_get_string(val));
					strcat(content, tempstr);
					if (class != CLASS_C) {
						pthread_mutex_lock(&mx_join_list);
						if (list_search(&join_list, &deveui_hex, &data_join, compare_msg_join, copy_msg_join)==false) {
							pthread_mutex_unlock(&mx_join_list);
							json_value_free(root_val);
							bzero(&pkt_recv,sizeof(pkt_recv));
							continue;
						}
						pthread_mutex_unlock(&mx_join_list);
						tmst = data_join.tmst;
						delay = JOIN_ACCEPT_DELAY;
					} else {
						tmst = 0;
						delay = NO_DELAY;
					}
					/*packet the message sending to gateway*/
					if (serialize_msg_to_gw(frame_payload, 17, deveui_hex, json_data, gwaddr, tmst, delay) == false) {
						json_value_free(root_val);
						bzero(&pkt_recv, sizeof(pkt_recv));
						continue;
					}
					msg_to_gw.gwaddr = malloc(strlen(gwaddr) + 1);
					msg_to_gw.json_string = malloc(strlen(json_data) + 1);
					strcpy(msg_to_gw.gwaddr, gwaddr);
					strcpy(msg_to_gw.json_string, json_data);
					msg_size = sizeof(msg_to_gw);
					pthread_mutex_lock(&mx_gw_list);
					list_insert_at_tail(&gw_list, &msg_to_gw, msg_size, assign_msg_down);
					pthread_mutex_unlock(&mx_gw_list);
				}
			}
		}
		json_value_free(root_val);
		MSG("%s\n",content);
		bzero(&pkt_recv, sizeof(pkt_recv));
	}
	close(conn->connfd);
}

/*receive the downstream from network controller*/
void thread_nc_down() {
	int j;
	struct sockaddr_in cliaddr;
	socklen_t clilen;
	pthread_t th_nc_down_handle;
	tcp_bind(netserv_addr, netserv_port_fornc, &listenfd_nc_down);
	listen(listenfd_nc_down, LISTENQ);
	while (!exit_sig) {
		if ((connfd_nc_down = accept(listenfd_nc_down, (struct sockaddr*)&cliaddr, &clilen)) < 0) {
			if (exit_sig != true) {
				MSG("ERROR: [down] thread_nc_dwon failed to accept:%s",strerror(errno));
			} else {
				MSG("INFO: listening socketfd is shut down\n");
			}
			break;
		}
		/*number of connections add 1*/
		pthread_mutex_lock(&mx_conn_nc);
		conn_num_nc++;
		pthread_mutex_unlock(&mx_conn_nc);
		struct arg arg;
		arg.connfd = connfd_nc_down;
		connfds_nc[j] = connfd_nc_down;
		/*create threads*/
		if (pthread_create(&th_nc_down_handle, NULL, (void*)thread_nc_down_handle, (void*)&arg) != 0) {
			MSG("ERROR: [thread_up] impossible to create thread for receiving and handling downstream message from network controller\n");
			exit(EXIT_FAILURE);
		}
		if (conn_num_nc == CONNFD_NUM_MAX) {
			break;
		}
	}
	close(listenfd_nc_down);
}

void thread_nc_down_handle(void* arg) {
	pthread_detach(pthread_self());
	struct arg* conn = (struct arg*)arg;
	int msg_len;
	int pkt_no = 0;
	struct pkt pkt_recv;
	unsigned int class;/*device class*/
	struct msg_down msg_to_gw;
	int msg_size = 0;

	/*json parsing variables*/
	JSON_Value  *root_val = NULL;
	JSON_Object *app_obj = NULL;
	JSON_Object *control_obj = NULL;
	JSON_Value  *val = NULL;

	char content[512];
	char tempstr[64];
	char msgname[64];
	char gwaddr[16];
	char deveui_hex[17];
	uint32_t devAddr;
	int size;
	const char* frame_payload;
	char json_data[512];
	struct msg_delay data_delay;

        sqlite3 *db;

	while (!exit_sig) {
		if ((msg_len = recv(conn->connfd, &pkt_recv, sizeof(pkt_recv), 0)) < 0){
			MSG("WARNING: [up] thread_nc_down_handle recv returned %s\n", strerror(errno));
			break;
		}
		if (msg_len == 0) {
			if (exit_sig == true) {
				/*socket is shut down*/
				MSG("INFO: the connected socket is shut down\n ");
			} else {
				/*number of connections -1*/
				pthread_mutex_lock(&mx_conn_nc);
				conn_num_nc--;
				pthread_mutex_unlock(&mx_conn_nc);
				MSG("WARNING: [down] the network controller is close,the corresponding connected socket is %d \n", conn->connfd);
			}
			break;
		}
		pkt_no++;
		/*handling the message*/
		bzero(content, sizeof(content));
		root_val = json_parse_string_with_comments((const char*)(pkt_recv.json_content));
		if (root_val == NULL) {
			MSG("WARNING: [down] message_%d downstream from network controller contains invalid JSON\n", pkt_no);
			json_value_free(root_val);
		}
		app_obj = json_object_get_object(json_value_get_object(root_val), "app");
		if (app_obj == NULL) {
			MSG("WARNING: [down] message_%d downstream from network controller contains no \"app\" object \n", pkt_no);
			json_value_free(root_val);
		} else {
			snprintf(msgname, sizeof(msgname), "###############command_data_%d:###############\n", pkt_no);
			strcat(content, msgname);
			val = json_object_get_value(app_obj, "devaddr");
			if (val != NULL) {
				devAddr = (uint32_t)json_value_get_number(val);
				snprintf(tempstr, sizeof(tempstr), "devaddr:%.0f", json_value_get_number(val));
				strcat(content, tempstr);
			}
			control_obj = json_object_get_object(app_obj, "control");
			if (control_obj == NULL) {
				MSG("WARNING:[down] message_%d received contains no \"control\" in \"app\"\n", pkt_no);
			} else {
				val = json_object_get_value(control_obj, "size");
				if (val != NULL) {
					size = json_value_get_number(val);
					snprintf(tempstr, sizeof(tempstr)," size:%.0f", json_value_get_number(val));
					strcat(content, tempstr);
				}
				val = json_object_get_value(control_obj, "frame");
				if (val != NULL) {
					frame_payload = json_value_get_string(val);
					snprintf(tempstr, sizeof(tempstr), " frame:%s", json_value_get_string(val));
					strcat(content, tempstr);
					/*
					 * TODO
					 * get the gateway address from the database
					*/
					/*packet the message sending to gateway*/
					/*query the deveui according to the devaddr*/

                                        if (sqlite3_open("/tmp/loraserv", &db)) {
                                                fprintf(stderr, "ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
                                                sqlite3_close(db);
						json_value_free(root_val);
						bzero(&pkt_recv, sizeof(pkt_recv));
						continue;
                                        }

					if (query_db_by_addr_str(db, "deveui", "nsdevinfo", devAddr, deveui_hex) == FAILED){
						MSG("WARNING: [down] query the database failed\n");
						json_value_free(root_val);
						bzero(&pkt_recv, sizeof(pkt_recv));
                                                sqlite3_close(db);
						continue;
					}

					if(query_db_by_deveui_uint(db, "class", "nsdevinfo", deveui_hex, &class) == FAILED){
						MSG("WARNING: [down] query the database failed\n");
						json_value_free(root_val);
						bzero(&pkt_recv, sizeof(pkt_recv));
                                                sqlite3_close(db);
						continue;
					}

                                        sqlite3_close(db);

					/* for CLASS A devices
					 * store the command message in the linked list
					 * wait for the next upstream transmission of end device
					 */
					if (class != CLASS_C) {
						data_delay.devaddr = devAddr;
						strcpy(data_delay.deveui_hex,deveui_hex);
						data_delay.frame=malloc(strlen(frame_payload) + 1);
						strcpy(data_delay.frame, frame_payload);
						data_delay.size = size;
						pthread_mutex_lock(&mx_delay_list);
						list_insert_at_tail(&delay_list, &data_delay, sizeof(struct msg_delay), assign_msg_delay);
						pthread_mutex_unlock(&mx_delay_list);
					}
					/*for CLASS C devices
					 * no need to wait for the next upstream
					 */
					else {
						if (serialize_msg_to_gw(frame_payload, size, deveui_hex, json_data, gwaddr, 0, NO_DELAY) == false) {
							json_value_free(root_val);
							bzero(&pkt_recv, sizeof(pkt_recv));
							continue;
						}
						msg_to_gw.gwaddr = malloc(strlen(gwaddr) + 1);
						msg_to_gw.json_string = malloc(strlen(json_data) + 1);
						strcpy(msg_to_gw.gwaddr, gwaddr);
						strcpy(msg_to_gw.json_string, json_data);
						msg_size = sizeof(msg_to_gw);
						pthread_mutex_lock(&mx_gw_list);
						list_insert_at_tail(&gw_list, &msg_to_gw, msg_size, assign_msg_down);
						pthread_mutex_unlock(&mx_gw_list);
					}
				}
			}
		}
		json_value_free(root_val);
		MSG("%s\n",content);
		bzero(&pkt_recv, sizeof(pkt_recv));
	}
	close(conn->connfd);
}

void thread_list_check(void* arg) {
	pthread_detach(pthread_self());
	struct msg_delay data_delay;
	struct th_check_arg* th_arg;
	char gwaddr[17];
	char json_data[JSON_MAX];
	struct msg_down msg_to_gw;
	int msg_size = 0;
	unsigned int delay_raw;
	unsigned int delay;

        sqlite3 *db;

	th_arg = (struct th_check_arg*)arg;
	data_delay.frame = malloc(512);
	pthread_mutex_lock(&mx_delay_list);
	if (list_search_and_delete(&delay_list, &(th_arg->devaddr), &data_delay, compare_msg_delay, copy_msg_delay, destroy_msg_delay) == true) {
		pthread_mutex_unlock(&mx_delay_list);
                if (sqlite3_open("/tmp/loraserv", &db)) {
                        fprintf(stderr, "ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
			delay = RECV_DELAY;
                }

		if (query_db_by_deveui_uint(db, "delay", "transarg", data_delay.deveui_hex, &delay_raw) == FAILED) {
			MSG("WARNING: [down] query the database failed\n");
			delay = RECV_DELAY;
		}

                sqlite3_close(db);

		delay = (delay_raw + 1) * 1000000;

		if (serialize_msg_to_gw(data_delay.frame, data_delay.size, data_delay.deveui_hex, json_data, gwaddr, th_arg->tmst, delay) == true) {
			msg_to_gw.gwaddr = malloc(strlen(gwaddr) + 1);
			msg_to_gw.json_string = malloc(strlen(json_data) + 1);
			strcpy(msg_to_gw.gwaddr, gwaddr);
			strcpy(msg_to_gw.json_string, json_data);
			msg_size = sizeof(msg_to_gw);
			pthread_mutex_lock(&mx_gw_list);
			list_insert_at_tail(&gw_list, &msg_to_gw, msg_size, assign_msg_down);
			pthread_mutex_unlock(&mx_gw_list);
		}
	} else
		pthread_mutex_unlock(&mx_delay_list);
	free(data_delay.frame);
}

