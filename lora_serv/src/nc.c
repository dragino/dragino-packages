#include "handle.h"

/* --- PRIVATE VARIABLES ------------------------------------------- */
/*network configuration variables*/
static char ncserv_addr[64] = STR(NC_SERV_ADDR);
static char netserv_addr[64] = STR(NET_SERV_ADDR);
static char port_up[8] = STR(NC_PORT_UP);
static char port_down[8] = STR(NC_PORT_DOWN);

/* network sockets */
static int listenfd_up;/* listening socket for upstream from network server*/
static int connfd_up;/*connecting socket for upstream form network server*/
static int sockfd_down;/* socket for downstream to network server*/
static int connfds[CONNFD_NUM_MAX];/*array for connected socket file descriptor*/
static int conn_num = 0;

/*mutex*/
static pthread_mutex_t mx_conn = PTHREAD_MUTEX_INITIALIZER;/*mutex lock for controlling rewriting gloable variable conn_num*/
static pthread_mutex_t mx_cmd_list = PTHREAD_MUTEX_INITIALIZER;/*mutex lock for conrolling access to the linked list storing commands*/
static pthread_mutex_t mx_trans_list = PTHREAD_MUTEX_INITIALIZER;/*mutex lock for controlling access to the linked list storing transfering arguments*/
static pthread_mutex_t mx_rxdelay_list = PTHREAD_MUTEX_INITIALIZER;/*mutex lock for controlling access to the linked list storing receive window delay*/

/*exit signal*/
static bool exit_sig = false;
static linked_list  cmd_list;/*the linked list storing commands*/
static linked_list  trans_list;/*the linked list storing transfering arguments*/
static linked_list  rxdelay_list;/*the linked list storing the receive window delay*/

/* --- PRIVATE FUNCTIONS ------------------------------------------- */
/*signal handle function*/
static void signal_handle(int signo);

/* threads*/
static void  thread_up();/* thread for establishing connection with network server in upstream direction*/
static void  thread_down();/*thread for sending downstream to network server*/
static void  thread_up_handle(void* arg);/*thread for receiving and handling upstream message*/

/*-----------------------------------------------------------------------------------*/
/* ------------------------------------ MAIN FUNCTION ------------------------------ */
int main(int argc, char** argv) {
	/* threads*/
	pthread_t  th_up;
	pthread_t  th_down;
	struct sigaction sigact;
	int i;
	int id;/*choice ID*/
	char json_data[JSON_MAX];
	int flag;

	/*variables for generating mac commands*/
	unsigned datarate;
	unsigned txPower;
	unsigned chMask;
	unsigned maskCtl;
	unsigned nbRep;
	unsigned devAddr;
	unsigned maxDCycle;
	unsigned rx1DRoffset;
	unsigned rx2Datarate;
	unsigned frequency;
	unsigned chIndex;
	unsigned maxDatarate;
	unsigned minDatarate;
	unsigned delay;
	unsigned base=1;
	struct msg cmd;

	/*initialize the array of connected socket*/
	for (i = 0; i < CONNFD_NUM_MAX; i++) {
		connfds[i] = -1;
	}
	/*configure the signal handling*/
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_NOMASK;
	sigact.sa_handler = signal_handle;
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	/*create the linked list to store json string*/
	list_init(&cmd_list);
	list_init(&trans_list);
	list_init(&rxdelay_list);
	/*create threads*/
	if (pthread_create(&th_up, NULL, (void*(*)(void*))thread_up, NULL) != 0 ) {
		MSG("ERROR: [main] impossible to create thread for upstream communicating from network server\n");
		exit(EXIT_FAILURE);
	}
	if (pthread_create(&th_down, NULL, (void*(*)(void*))thread_down, NULL) != 0 ) { 
                MSG("ERROR: [main] impossible to create thread for downstream communicating to network server\n");
		exit(EXIT_FAILURE);
	}

	/*read the input command*/
	while (!exit_sig) {
			bzero(json_data, sizeof(json_data));
			MSG("ENTER THE COMMAND ID AS FOLLOWS:\n");
			MSG("1 LINK ADR REQUEST\n");
			MSG("2 DUTY CYCLE REQUEST\n");
			MSG("3 RX PARAMETER SETUP REQUEST\n");
			MSG("4 DEV STATUS REQUEST\n");
			MSG("5 NEW CHANNEL REQUEST\n");
			MSG("6 RX TIMING SETUP REQUEST\n");
			if ((scanf("%d", &id)) != 1){
				MSG("ERROR: [main] invalid input\n");
				setbuf(stdin,NULL);
				continue;
			}
			switch (id) {
				case 1: {
					MSG("[DataRate TxPower ChannelMask MaskControl NbRep DeviceAddress]\n");
					if (scanf("%u %u %u %u %u %u", &datarate, &txPower, &chMask, &maskCtl, &nbRep, &devAddr) !=6 ) {
						MSG("ERROR: [main] invalid input\n");
						setbuf(stdin, NULL);
						continue;
					}
					if (datarate > 15 || txPower > 15 || chMask > (1<<16-1) || maskCtl > 7 || nbRep > 15) {
						MSG("ERROR: [main] input out of range \n");
						continue;
					}
					flag = command_handle(SRV_MAC_LINK_ADR_REQ, (uint32_t)devAddr,json_data,(uint8_t)datarate,(uint8_t)txPower,(uint16_t)chMask,(uint8_t)maskCtl,(uint8_t)nbRep);
					break;
				}
				case 2:{
					MSG("[MaxDCycle DeviceAddress]\n");
					if(scanf("%u %u",&maxDCycle,&devAddr)!=2){
						MSG("ERROR: [main] invalid input\n");
						setbuf(stdin,NULL);
						continue;
					}
					if(maxDCycle>255){
						MSG("ERROR: [main] input out of range \n");
						continue;
					}
					flag=command_handle(SRV_MAC_DUTY_CYCLE_REQ,(uint32_t)devAddr,json_data,(uint8_t)maxDCycle);
					break;
				}
				case 3:{
					MSG("[RX1DRoffset RX2DataRate Frequency DeviceAddress]\n");
					if(scanf("%u %u %u %u",&rx1DRoffset,&rx2Datarate,&frequency,&devAddr)!=4){
						MSG("ERROR: [main] invalid input\n");
						setbuf(stdin,NULL);
						continue;
					}
					if(rx1DRoffset>7||rx2Datarate>15||frequency>(base<<24)-1){
						MSG("ERROR: [main] input out of range \n");
						continue;
					}
					flag=command_handle(SRV_MAC_RX_PARAM_SETUP_REQ,(uint32_t)devAddr,json_data,(uint8_t)rx1DRoffset,(uint8_t)rx2Datarate,(uint32_t)frequency);
					if(flag!=-1){
						struct msg_trans data;
						data.devaddr=(uint32_t)devAddr;
						data.rx1_dr=(uint8_t)(0+rx1DRoffset);
						data.rx2_dr=(uint8_t)rx2Datarate;
						data.rx2_freq=(uint32_t)frequency;
						pthread_mutex_lock(&mx_trans_list);
						list_insert_at_tail(&trans_list,&data,sizeof(struct msg_trans),assign_msg_trans);
						pthread_mutex_unlock(&mx_trans_list);
					}
					break;
				}
				case 4:{
					MSG("[DeviceAddress]\n");
					if(scanf("%u",&devAddr)!=1){
						MSG("ERROR: [main] invalid input\n");
						setbuf(stdin,NULL);
						continue;
					}
					flag=command_handle(SRV_MAC_DEV_STATUS_REQ,(uint32_t)devAddr,json_data);
					break;
				}
				case 5:{
					MSG("[ChIndex Frequency MaxDatarate MinDatarate DeviceAddress]\n");
					if(scanf("%u %u %u %u %u",&chIndex,&frequency,&maxDatarate,&minDatarate,&devAddr)!=5){
						MSG("ERROR: [main] invalid input\n");
						setbuf(stdin,NULL);
						continue;
					}
					if(chIndex>255||frequency>(1<<24)-1||maxDatarate>15||minDatarate>15){
						MSG("ERROR: [main] input out of range\n");
						continue;
					}
					flag=command_handle(SRV_MAC_NEW_CHANNEL_REQ,(uint32_t)devAddr,json_data,(uint8_t)chIndex,(uint32_t)frequency,(uint8_t)maxDatarate,(uint8_t)minDatarate);
					break;
				}
				case 6:{
					MSG("[Delay DeviceAddress]\n");
					if(scanf("%u %u",&delay,&devAddr)!=2){
						MSG("ERROR: [main] invalid input\n");
						setbuf(stdin,NULL);
						continue;
					}
					if(delay>15){
						MSG("ERROR: [main] input out of range\n");
						continue;
					}
					flag=command_handle(SRV_MAC_RX_TIMING_SETUP_REQ,(uint32_t)devAddr,json_data,(uint8_t)delay);
					if(flag!=-1){
						struct msg_rxdelay data;
						data.devaddr=(uint32_t)devAddr;
						data.delay=(uint8_t)delay;
						pthread_mutex_lock(&mx_rxdelay_list);
						list_insert_at_tail(&rxdelay_list,&data,sizeof(struct msg_rxdelay),assign_msg_rxdelay);
						pthread_mutex_unlock(&mx_rxdelay_list);
					}
					break;
				}
				default:{
					MSG("WARNING: [main] please choose a right number\n");
					continue;
				}
			}
			if(flag==-1){
				continue;
			}
			/*store the command package sending to network server*/
			cmd.json_string=malloc(strlen(json_data)+1);
			strcpy(cmd.json_string,json_data);
			pthread_mutex_lock(&mx_cmd_list);
			list_insert_at_tail(&cmd_list,&cmd,sizeof(struct msg),assign_msg);
			pthread_mutex_unlock(&mx_cmd_list);
	}
	if(exit_sig==true){
		/*shutdown all socket to alarm the threads from system call(like recv()) */
		shutdown(listenfd_up,SHUT_RDWR);
		/*shutdown all connected sockfd*/
		for(i=0;i<CONNFD_NUM_MAX;i++){
			if(connfds[i]==-1){
				break;
			}
			shutdown(connfds[i],SHUT_RDWR);
		}
		shutdown(sockfd_down,SHUT_RDWR);
		/*free linked list*/
		list_destroy(&cmd_list,destroy_msg);
		list_destroy(&trans_list,NULL);
		list_destroy(&rxdelay_list,NULL);
	}
	pthread_join(th_up,NULL);
	pthread_join(th_down,NULL);
	MSG("INFO: the main program on network controller exit successfully\n");
	exit(EXIT_SUCCESS);
}

void signal_handle(int signo) {
	if (signo==SIGINT || signo==SIGTERM || signo == SIGQUIT) {
		exit_sig=true;
		MSG("######################waiting for exiting#######################\n");
	}
	return;
}

void thread_up() {
	struct sockaddr_in cliaddr;
	socklen_t clilen;
	int i;
	pthread_t th_up_handle;
	/*try to open and bind TCP listening socket*/
	tcp_bind(ncserv_addr, port_up, &listenfd_up);
	listen(listenfd_up, LISTENQ);
	while (!exit_sig) {
		if ((connfd_up = accept(listenfd_up, (struct sockaddr*)&cliaddr, &clilen)) < 0) {
			if (exit_sig != true) {
				printf("ERROR: [up] thread_up failed to accept:%s", strerror(errno));
			} else {
				MSG("INFO: listening socketfd is shut down\n");
			}
			break;
		}
		pthread_mutex_lock(&mx_conn);
		conn_num++;
		pthread_mutex_unlock(&mx_conn);
		struct arg arg;
		arg.connfd = connfd_up;
		connfds[i] = connfd_up;
		/*create threads*/
		if (pthread_create(&th_up_handle, NULL, (void*)thread_up_handle, (void*)&arg) != 0) {
			MSG("ERROR: [up] impossible to create thread for receiving and handling upstream message\n");
			exit(EXIT_FAILURE);
		}
		i++;
		if (conn_num == CONNFD_NUM_MAX) {
			break;
		}
	}
	close(listenfd_up);
	MSG("INFO: thread_up exit successfully\n");
}

void thread_down(){
	struct pkt pkt_send;
	reconnect:tcp_connect(netserv_addr, port_down, &sockfd_down, &exit_sig);
	while (!exit_sig) {
			/*read the content of linked list and send*/
			pthread_mutex_lock(&mx_cmd_list);
			while (list_is_empty(&cmd_list) == false) {
				struct msg* temp = (struct msg*)((cmd_list.head)->data);
				strcpy(pkt_send.json_content, temp->json_string);
				if (send(sockfd_down, &pkt_send, sizeof(pkt_send), 0) < 0) {
					MSG("WARNING: [down] failed to send data to network server\n");
					MSG("INFO: trying to reconnect to the network server\n");
					pthread_mutex_unlock(&mx_cmd_list);
					goto reconnect;
				} else {
					MSG("INFO: [down]send command message successfully\n");
					list_delete_at_head(&cmd_list, destroy_msg);
				}
			}
			pthread_mutex_unlock(&mx_cmd_list);
			/*set timer for preparing data in thread_up_handle */
			set_timer(SEND_INTERVAL_S, SEND_INTERVAL_MS);
		}
		MSG("INFO: thread_down exit successfully\n");
}

void thread_up_handle(void* arg) {
	pthread_detach(pthread_self());
	struct arg* conn = (struct arg*)arg;
	int msg_len;
	int index=0;
	int i;
	struct command_info cmd_info;
	struct pkt pkt_recv;
	char deveui_hex[17];
	struct msg_trans data_trans;
	struct msg_rxdelay data_rxdelay;

        sqlite3* db;

	bzero(&cmd_info, sizeof(cmd_info));
	bzero(&pkt_recv, sizeof(pkt_recv));
	bzero(&data_trans, sizeof(data_trans));
	bzero(&data_rxdelay, sizeof(data_rxdelay));
	start: while (!exit_sig) {
		if ((msg_len=recv(conn->connfd, &pkt_recv, sizeof(pkt_recv), 0)) < 0) {
			MSG("WARNING: [up] thread_up_handle recv returned %s\n", strerror(errno));
			break;
		}
		if (msg_len == 0) {
			if (exit_sig == true) {
				/*socket is shut down*/
				MSG("INFO: the connected socket is shut down\n ");
			} else {
				pthread_mutex_lock(&mx_conn);
				conn_num--;
				pthread_mutex_unlock(&mx_conn);
				MSG("WARNING: [up] the client is close,the corresponding connected socket is %d \n",conn->connfd);
			}
			break;
		}
		//MSG(pkt_recv.json_content);
		nc_msg_handle(pkt_recv.json_content, index, &cmd_info);

		for (i=0; i<cmd_info.cmd_num; i++){
			if (cmd_info.type[i] == MOTE_MAC_RX_PARAM_SETUP_ANS) {
				pthread_mutex_lock(&mx_trans_list);
				if (list_search_and_delete(&trans_list, &(cmd_info.devaddr), &data_trans, compare_msg_trans, copy_msg_trans, NULL) == false) {
					pthread_mutex_unlock(&mx_trans_list);
					MSG("WARNING: [up] find node failed\n");
					index++;
					bzero(&pkt_recv, sizeof(pkt_recv));
					bzero(&cmd_info, sizeof(cmd_info));
					goto start;
				}
				pthread_mutex_unlock(&mx_trans_list);
				/* update the table transarg*/
				if (cmd_info.isworked[i] == true) {
					/*query the deveui according to the devaddr*/
                                        
                                        if (sqlite3_open("/tmp/loraserv", &db)) {
                                                fprintf(stderr, "ERROR: Can't open database: %s\n", sqlite3_errmsg(db));
                                                sqlite3_close(db);
						bzero(&pkt_recv, sizeof(pkt_recv));
						bzero(&cmd_info, sizeof(cmd_info));
						goto start;
                                        }
					if (query_db_by_addr_str(db, "deveui", "nsdevinfo", data_trans.devaddr, deveui_hex) == FAILED) {
						MSG("WARNING: [up] query the database failed\n");
						bzero(&pkt_recv, sizeof(pkt_recv));
						bzero(&cmd_info, sizeof(cmd_info));
                                                sqlite3_close(db);
						goto start;
					}
					if (update_db_by_deveui_uint(db, "rx1datarate", "transarg", deveui_hex, data_trans.rx1_dr) == FAILED ||
							update_db_by_deveui_uint(db, "rx2datarate", "transarg", deveui_hex, data_trans.rx2_dr) == FAILED ||
							update_db_by_deveui_uint(db, "rx2frequency", "transarg", deveui_hex, data_trans.rx2_freq) == FAILED){
						MSG("WARNING: [up] update the database failed\n");
						bzero(&pkt_recv,sizeof(pkt_recv));
						bzero(&cmd_info,sizeof(cmd_info));
                                                sqlite3_close(db);
						goto start;
					}
                                        sqlite3_close(db);
				}
			}

			if (cmd_info.type[i] == MOTE_MAC_RX_TIMING_SETUP_ANS) {
				pthread_mutex_lock(&mx_rxdelay_list);
				if (list_search_and_delete(&rxdelay_list, &(cmd_info.devaddr), &data_rxdelay, compare_msg_rxdelay, copy_msg_rxdelay, NULL) == false) {
					pthread_mutex_unlock(&mx_rxdelay_list);
					MSG("WARNING: [up] find node failed\n");
					index++;
					bzero(&pkt_recv, sizeof(pkt_recv));
					bzero(&cmd_info, sizeof(cmd_info));
					goto start;
				}
				pthread_mutex_unlock(&mx_rxdelay_list);
				/* update the table transarg*/
				if (cmd_info.isworked[i] == true) {
					/*query the deveui according to the devaddr*/
                                        if (sqlite3_open("/tmp/loraserv", &db)) {
						MSG("WARNING: [up] open database failed\n");
						bzero(&pkt_recv, sizeof(pkt_recv));
						bzero(&cmd_info, sizeof(cmd_info));
						goto start;
                                        }
					if (query_db_by_addr_str(db, "deveui", "nsdevinfo", data_rxdelay.devaddr, deveui_hex) == FAILED) {
						MSG("WARNING: [up] query the database failed\n");
						bzero(&pkt_recv, sizeof(pkt_recv));
						bzero(&cmd_info, sizeof(cmd_info));
                                                sqlite3_close(db);
						goto start;
					}
					if (update_db_by_deveui_uint(db, "delay", "transarg", deveui_hex, data_rxdelay.delay) == FAILED){
						MSG("WARNING: [up] update the database failed\n");
						bzero(&pkt_recv, sizeof(pkt_recv));
						bzero(&cmd_info, sizeof(cmd_info));
                                                sqlite3_close(db);
						goto start;
					}
                                        sqlite3_close(db);
				}
			}
		}
		index++;
		bzero(&pkt_recv, sizeof(pkt_recv));
		bzero(&cmd_info, sizeof(cmd_info));
	}
	close(conn->connfd);
}
