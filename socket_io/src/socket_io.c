#include <stdio.h>
#include <errno.h> 
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <linux/net.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <uci.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <net/if.h>


#define SOCKET_IO_REV   "0.1"
#define HW_VER "0.1"	

#define TIMEOUT	100000L   	/* in us */
#define SOCKET_BUFLEN 1500	/* One standard MTU unit size */ 
#define PORT 9930
#define OUTPUTS_NUM	4		/* We have that many gpio outputs */
#define INPUTS_NUM 	4       /* We have that many gpio inputs */
#define STR_MAX		100		/* Maximum string length */
#define MSG_MAX     500     /* Maximum UDP message length */
#define UDP_ARGS_MAX 20		/* we can have that much arguments ('/' separated) on the UDP datagram */ 
#define SIODS_MAX 100     	/* we may have that many SIOD devices in the mesh */ 
#define SECSINDAY (24*60*60) /* That many seconds in a day */
#define DAYSINWEEK (7) 		/* That many days in a week*/ 
#define RULES_MAX	10		/* We may have not more than that many rules in the PLC table */

#define REL0	16			/* GPIOs controlling the outputs */
#define REL1    28			/* The relay address is [REL0 REL1]  so REL1 is LSB */
#define S_R		15
#define PULSE   1

#define FB0		18			/* GPIO Feedbacks from the relay outputs*/
#define FB1     21
#define FB2     22
#define FB3     27

#define IN0     19          /* GPIO Inputs */
#define IN1     20
#define IN2     23
#define IN3     24

#define IVRSETTIMEOUT 10    /* We have 1 sec to get Put message after IVRSet message */

struct GST_nod {
    int siod_id;                /* ID of the SIOD */
    unsigned char gpios;        /* the gpio byte for the siod_id. Check GPIOs variable */
};
struct GST_nod GST[SIODS_MAX];  /* Keeps the status of all IOs of including the local one at the first location 
								   the list is terminated by a zero siod_id member */


struct IPT_nod {
    int siod_id;                /* ID of the SIOD */
    unsigned long IPaddress;    /* IP address we can use to send message to this SIOD */
};
struct IPT_nod IPT[SIODS_MAX];  /* Keeps the IP addresses of all SIODs which have ever sent some data to us. 
								   The local IP address is not included in this table. 
								   The list is terminated by a zero siod_id member */

struct {
	int	n;						/* amount of active rules */
	char *rules[RULES_MAX];		/* Keep the rules in string form */
	int triggered[RULES_MAX];	/* Notifies if rule has triggered */
} PLCT;


int strfind(const char *s1, const char *s2);
int process_udp(char *datagram);
void RemoveSpaces(char* source);
int extract_args(char *datagram, char *args[], int *n_args);
char *strupr(char *s);
unsigned long long MACaddress_str2num(char *MACaddress);
void MACaddress_num2str(unsigned long long MACaddress, char *MACaddress_str);
unsigned long IPaddress_str2num(char *IPaddress);
void IPaddress_num2str(unsigned long IPaddress, char *IPaddress_str);
unsigned long long eth0MAC(void);
unsigned long long eth1MAC(void);
unsigned long long wifiMAC(void);
int uciget(const char *param, char *value);
int uciset(const char *param, const char *value);
int ucidelete(const char *param);
int uciadd_list(const char *param, const char *value);
void ucicommit(void);
void restartnet(void);
void uptime(char *uptime);
int getsoftwarever(char *ver);
int broadcast(char *msg);
int unicast(char *msg);
int gpios_init(void);
int setgpio(char *X, char *Y);
int getgpio(char *X, char *Y);
void intHandler(int dummy);
unsigned char GSTchecksum(struct GST_nod *gst);
void GSTadd(struct GST_nod *gst, unsigned short siod_id, unsigned char gpios);
void GSTdel(struct GST_nod *gst, unsigned short siod_id);
int GSTget(struct GST_nod *gst, unsigned short siod_id, unsigned char *gpios);
int GSTset(struct GST_nod *gst, unsigned short siod_id, unsigned char gpios);
void GSTprint(struct GST_nod *gst, char *str);
void byte2binarystr(int n, char *str);
unsigned char binarystr2byte(char *str);
int IPTget(struct IPT_nod *ipt, unsigned short siod_id, unsigned long *IPaddress);
void IPTset(struct IPT_nod *gst, unsigned short siod_id, unsigned long IPaddress);
int ParseTimeRange(char *TimeRangeStr);
int CheckTimeRange(void);
int PLCadd(char *rule);
int PLCdel(char *AAAA1, char *X1, char *Y1);
void PLCprint(char *);
void PLCexec(void);
void bcast_init(void);
void restart_asterisk(void);
void asterisk_config_write(char *SIPRegistrar1, char *AuthenticationName1, char *Password1, char *SIPRegistrar2, char *AuthenticationName2, char *Password2);
void asterisk_uptime(char *uptime);
int hashit(char *cmd);
void IVRSetTimer(void);
void getIP(char *);
void getIPMask(char *);

int IVRSet_counter;

enum 		   {ConfigBatmanReq, ConfigBatmanRes, ConfigBatman, ConfigReq, ConfigRes, Config, \
	  			RestartNetworkService, RestartAsterisk, ConfigAsterisk, AsteriskStatReq, \
				AsteriskStatRes, ConfigNTP, Set, PLC, PLCReq, PLCRes, TimeRange, TimeRangeOut, \
				Get, Put, GSTCheckSumReq, GSTCheckSum, GSTReq, GSTdata, Ping, PingRes, \
				IVRGetReq, IVRGetRes, IVRSetReq, IVRSetRes};
char *cmds[30]={"ConfigBatmanReq", "ConfigBatmanRes", "ConfigBatman", "ConfigReq", "ConfigRes", "Config", \
      			"RestartNetworkService", "RestartAsterisk", "ConfigAsterisk", "AsteriskStatReq", \
				"AsteriskStatRes","ConfigNTP", "Set", "PLC",  "PLCReq", "PLCRes", "TimeRange", "TimeRangeOut", \
				"Get", "Put", "GSTCheckSumReq", "GSTCheckSum", "GSTReq", "GSTdata", "Ping", "PingRes", \
				"IVRGetReq", "IVRGetRes", "IVRSetReq", "IVRSetRes"};

int verbose=0; 	/* get value from the command line */


char SIOD_ID[STR_MAX];      	/* Our SIOD ID */

unsigned char GPIOs;        	/* We keep the status of the 8 IOs in this byte   */
                            	/*  GPIOS = [IN3 IN2 IN1 IN0 OUT3 OUT2 OUT1 OUT0] */
                            	/*           MSB                            LSB   */

unsigned long IPADR;			/* Store our IP address */

/* listening socket */
int udpfd;
struct sockaddr_in servaddr, cliaddr;

/* socket for the brodcasting messages*/
int bcast_sockfd;
struct sockaddr_in bcast_servaddr;


/* global file descriptors so we don't have to open aand close all the time */
int fd_in0, fd_in1, fd_in2, fd_in3, fd_fb0, fd_fb1, fd_fb2, fd_fb3, fd_rel0, fd_rel1, fd_s_r, fd_pulse;
int IOs[OUTPUTS_NUM+INPUTS_NUM];

/* Time Range definitions */
struct {
	char Date[STR_MAX];	//To keep the TimeRange text parameter
	char Time[STR_MAX];	//To keep the TimeRange text parameter
	struct tm start;	//Start broken time 
	struct tm end;		//End broken time 

} TIMERANGE;


int main(int argc, char **argv){

	int n, nready; 
	char datagram[SOCKET_BUFLEN];
	fd_set rset;
	socklen_t addrlen;
	struct timeval	timeout;
	int res;	


	/* CTR-C handler */
	//signal(SIGINT, intHandler);


	/* Splash ============================================================ */

	/* No timing restrictions for the outputs ============================ */
	ParseTimeRange("/");

	/* We don't have rules in the PLC table ============================== */
	PLCT.n=0;
	PLCT.triggered[0]=0;

	/* Check for verbosity argument */
	if(argc>1) {
		if(!strcmp(argv[1], "-v")){
			verbose=1;
			printf("socket_io - rev %s (verbose)\n", SOCKET_IO_REV);		
		} else if(!strcmp(argv[1], "-vv")){
			verbose=2;
			printf("socket_io - rev %s (very verbose)\n", SOCKET_IO_REV);
		} else if(!strcmp(argv[1], "-vvv")){
            verbose=3;
            printf("socket_io - rev %s (very very verbose)\n", SOCKET_IO_REV);
        }

	} else
		printf("socket_io - rev %s\n", SOCKET_IO_REV);

	/* get SIOD_ID ======================================================= */
	uciget("siod.siod_id.id", SIOD_ID); 

	/* Our IP address */
	{
		char IPAddress[STR_MAX];
		//uciget("network.bat.ipaddr", IPAddress);
		getIP(IPAddress);
		if(IPAddress[0] != '\0')
			IPADR = IPaddress_str2num(IPAddress);
	}
	/* Init. local IOs, outputs set as per the previous relay feedbacks == */
	gpios_init();
	
	/* Insert the local gpios data in GST================================= */
	GSTadd(GST, atoi(SIOD_ID), GPIOs);
	
	/* Initialize the broadcasting socket  =============================== */
	bcast_init();


	/* Start UDP Socket server and listening for commands ================ */
	udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(udpfd == -1){
                perror("socket() failed");
		exit(-1);
	}

	//enable reception of broadcasting data
	//enabled = 1;
    //setsockopt(udpfd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled));

	/* Prepare the address */
	memset(&servaddr, 0, sizeof(servaddr)); 	
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);

	if(bind(udpfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1){
		perror("bind() failed");
		exit(-1);
	}

	if(verbose) printf("Listening on port %d\n", PORT);

	/* broadcasst Put message so all nodes syncronize their GST ========== */
	{
		char msg[STR_MAX], Y[9];
		byte2binarystr(GPIOs, Y);
		sprintf(msg, "JNTCIT/Put/%s//%s", SIOD_ID, Y);
    	if(verbose==2) printf("Sent: %s\n", msg);
    	broadcast(msg);	
	}

	for ( ; ; ) {

		/* descritors set prepared */ 
        	FD_ZERO(&rset);
        	FD_SET(udpfd, &rset);

		/* Set the timeout */
		timeout.tv_sec  = 0;
		timeout.tv_usec = TIMEOUT;

		nready = select(udpfd+1, &rset, NULL, NULL, &timeout);
		if (nready < 0) {
			printf("Error or signal\n");
			if (errno == EINTR)
				continue; /* back to for() */
			else {
      				perror("select() failed");
				exit(-1);
			}
		} else if (nready) {
			/* We have data to read */
				
			addrlen=sizeof(cliaddr);
			if((n = recvfrom(udpfd, datagram, SOCKET_BUFLEN, 0, (struct sockaddr *)&cliaddr, &addrlen))<0){
				/* System error */

				perror("recvfrom() failed");
				exit(-1);
			} else if (n>0){
				/* We have got an n byte datagram */
			
				/* ignore our own  broadcast messages */
				if (IPADR == cliaddr.sin_addr.s_addr){
					if(verbose==2) printf("Ignore our broadcast message\n");
					continue;
				}

				/* Process the datagram */ 
				datagram[n] = '\0';
				process_udp(datagram);

				/* Send it back */
				//sendto(udpfd, datagram, n, 0, &cliaddr, addrlen);		
	
			} else {
				/* The socket closed, this should not happen with UDP */
				printf("The socket closed ?!?!\n");
			}
			
		} else {
			/* Expected to happens each 100ms or so */
			
			//Check PLC rule
			PLCexec();

			//Timer for the IVRSet message
			IVRSetTimer();
		}
	}

	return(0);

}

/* 
 * Closes gpio descriptors on CTR-C 
 */
/*
void intHandler(int dummy) {
	int i;

	if(verbose) printf("Closing/free all open file descriptors and PLC rules\n");	

	close(fd_in0); close(fd_in1); close(fd_in2); close(fd_in3); 
	close(fd_fb0); close(fd_fb1); close(fd_fb2); close(fd_fb3); 
	close(fd_rel0); close(fd_rel1); close(fd_s_r); close(fd_pulse);

	close(udpfd); close(bcast_sockfd);
	
	// free PLC rules memory 
	for(i=0; i<PLCT.n; i++) free(PLCT.rules[i]);	

	exit(0);
}
*/


/* 
 * process the data coming from the udp socket
 */
int process_udp(char *datagram){
		
	int n_args;
	char *args[UDP_ARGS_MAX], msg[MSG_MAX];

	if(verbose==2) printf("In process_udp() \n");


	/* We process only datagrams starting with JNTCIT */
	if ((strlen(datagram)<7) || strncmp(datagram, "JNTCIT/", 7)){
		if(verbose==2) printf("Unrelated datagram => %s\n", datagram);
		return 0;
	} else
		datagram = datagram + 7;

	/* UDP pre processing */
	RemoveSpaces(datagram);
	//datagram = strupr(datagram);
	
	/* extract arguments */
	extract_args(datagram, args, &n_args);

	if(verbose==3) {
		int i;
		printf("UDP arguments: ");
		for(i=0;i<n_args;i++) 
			printf("%s ", args[i]);
		printf("\n");
	}


	switch(hashit(args[0])){
		/*
		Message: JNTCIT/ConfigBatmanReq
		Type: Broadcast
		Arguments: 
		Description: The CFG broadcasts this packet if he wants to get Batman configuration information 
					 for all the SIODs in the mesh. Note that the UDP messages are not passing thru 
					 if the Batman mesh is not properly setup, so for an initial Batman configuration 
					 it may be useful to use wired Ethernet switch connected to WAN of the SIODs.		
		*/
		case ConfigBatmanReq:{

                char MACAddress[STR_MAX], BSSID[STR_MAX], Encryption[STR_MAX], Passphrase[STR_MAX], Enable[STR_MAX];

                if(verbose==2) printf("Rcv: ConfigBatmanReq\n");

                MACaddress_num2str(wifiMAC(), MACAddress);
                uciget("wireless.ah_0.bssid", BSSID);
                uciget("wireless.ah_0.encryption", Encryption);
                uciget("wireless.ah_0.key", Passphrase);
                uciget("wireless.ah_0.disabled", Enable);
				Enable[0]=(Enable[0]=='0')?'1':'0'; //Invert the logic

                sprintf(msg, "JNTCIT/ConfigBatmanRes/%s/%s/%s/%s/%s", MACAddress, BSSID, Encryption, Passphrase, Enable);

                if(verbose==2) printf("Sent: %s\n", msg);

                broadcast(msg);

			}
			break;
        /*
		Message: JNTCIT/ConfigBatmanRes/MACAddress/BSSID/Encryption/Passphrase/WANbridge
		Type: Broadcast
		Arguments:
			MACAddress:			0a:ba:ff:10:20:30 (MAC of br-bat0 used as reference)
			BSSID:				02:ca:ff:ee:ba:be
			Encryption:			WPA2
			Passphrase:			S10D
		Description: SIOD is broadcasting this response in the network. To be able to create Batman-adv mesh 
					 all the WiFi parameters of each SIODs should match. All of the SIODs are configured 
					 as mesh entry point bridge between wan and wlan. Note that the UDP messages are not passing 
					 thru if the Batman mesh is not properly setup, so for an initial Batman configuration 
					 it may be useful to use wired Ethernet switch connected to WAN of the SIODs.
		*/  
        case ConfigBatmanRes:{
			/* Do nothing at the moment  */
			char *MACaddress = args[1];
			
			if(verbose==2) printf("Rcv: ConfigBatmanRes\n");

            }
            break;
		/*
		Message: JNTCIT/ConfigBatman/MACAddress/BSSID/Encryption/Passphrase/Enable
	     	 	 JNTCIT/ConfigBatman//BSSID/Encryption/Passphrase/Enable
		Type: Broadcast
		Arguments:
			MACAddress:(WiFi)(optional)	0a:ba:ff:10:20:30
			BSSID:					02:ca:ff:ee:ba:be
			Encryption:				WPA2
			Passphrase:				S10D
			Enable					1 if enabled, 0 if disabled
		Description: SIOD is broadcasting this response in the network. To be able to create Batman-adv mesh all the WiFi 
				 parameters of each SIODs should match. All of the SIOD are configured as mesh entry point, 
				 bridge between wan and wlan. Note that the UDP messages are not passing thru if the Batman mesh 
				 is not properly setup, so for an initial Batman configuration it may be useful to use wired Ethernet switch 
				 connected to WAN of the SIODs. Disconnect the wired switch after enabling the WiFi mesh to avoid 
				loop in the network. Only SIODs with matching MACAddress process the message. 
				MACAddress is an optional argument. If it is omitted all receiving SIODs will process the message. 		
		*/
        case ConfigBatman:{

                char *MACAddress, *BSSID, *Encryption, *Passphrase, *Enable;

                if(verbose==2) printf("Rcv: ConfigBatman\n");

				MACAddress=args[1]; BSSID=args[2]; Encryption=args[3];  Passphrase=args[4]; Enable=args[5];

                if(n_args != 6) {
                    printf("Wrong format of ConfigBatman message\n");
                    return -1;
                }

                if(MACAddress[0] == '\0' || wifiMAC() == MACaddress_str2num(MACAddress)) { /* Our MAC address matches or empty MAC, so process the packet */
                	uciset("wireless.ah_0.bssid", BSSID);
                	uciset("wireless.ah_0.encryption", Encryption);
                	uciset("wireless.ah_0.key", Passphrase);
                	uciset("wireless.ah_0.disabled", (Enable[0]=='0')?"1":"0");
				
					ucicommit();
				
					if(verbose) printf("WiFi Config commited\n");

                    sprintf(msg, "JNTCIT/200");
                    if(verbose==2) printf("Sent: %s\n", msg);
                    unicast(msg);
				}
            }
            break;
		/*
		Message: JNTCIT/ConfigReq
		Type: Broadcast
		Arguments: 
		Description: The CFG broadcasts this packet if he wants to get information for all the SIODs in the mesh.  
		*/
        case ConfigReq:{
				//We send our network parameters, check ConfigRes
				char MACAddress[STR_MAX], Uptime[STR_MAX], SoftwareVersion[STR_MAX], IPAddress[STR_MAX], IPMask[STR_MAX], Gateway[STR_MAX], DNS1[STR_MAX], DNS2[STR_MAX], DHCP[STR_MAX];
				int offset;

				if(verbose==2) printf("Rcv: ConfigReq\n");

				MACaddress_num2str(wifiMAC(), MACAddress);
            	uptime(Uptime);
            	getsoftwarever(SoftwareVersion);
				//uciget("network.bat.ipaddr", IPAddress);
            	//uciget("network.bat.netmask", IPMask);
				getIP(IPAddress);
				getIPMask(IPMask);
            	uciget("network.bat.gateway", Gateway);
            	uciget("network.bat.dns", msg);
				sscanf(msg, "%s %s", DNS1, DNS2);
				uciget("network.bat.proto", DHCP);
				
				sprintf(msg, "JNTCIT/ConfigRes/%s/%s/SIOD/%s/%s/%s/%s/%s/%s/%s/%s/%s", MACAddress, Uptime, HW_VER, SoftwareVersion, SIOD_ID, IPAddress, IPMask, Gateway, DNS1, DNS2, DHCP);

				if(verbose==2) printf("Sent: %s\n", msg);

				broadcast(msg);

            }
            break;
		/*
		Message: JNTCIT/ConfigRes/MACAddress/UpTime/UnitType/HardwareVersion/SoftwareVersion/AAAA/IPAddress/IPMask/Gateway/DNS1/DNS2/DHCP
		JNTCIT/ConfigRes/MACAddress/UpTime/UnitType/HardwareVersion/SoftwareVersion/////////
		Type: Broadcast
		Arguments: some of the arguments are optional. In this case back slash / still signifies the parameter place holder
			MACAddress:			0a:ba:ff:10:20:30
			Uptime: (Linux in seconds)	123.43		
			UnitType: 			SIOD, AsteriskPC, Intercom
			HardwareVersion: 		0.1
			SoftwareVersion: 		0.5(it is convenient to get this from /etc/banner)
			AAAA: (optional)		SIOD ID Only if the UnitType is SIOD
			IPAddress:(optional)		10.10.0.55
			IPMask:(optional)		255.255.255.0
			Gateway:(optional)		10.10.0.1
			DNS1:(optional)		8.8.8.8
			DNS2:(optional)		4.4.4.4
			DHCP:(optional)		static, dhcp
		Description: All devices in the network except the configuration PC broadcast this ConfigRes message in response to the Config message. 
					 This way all the nodes (not only the CFG PC) can fill their table with the available nodes in the mesh and their configuration. 
					 SIODs have WiFi and WAN Ethernet port which ar bridged into br-bat interface. br-bat is the interface which is actually configured.  
					 Optionally this message can be send unsolicited on the initial power up of the SIOD. If the SIOD is still not configured then  
					 IPAddress,  IPMask, Gateway, DNS1,DNS2 and DHCP are empty
		*/
        case ConfigRes:{
				//Only update our IPT at the moment		
				char *AAAA;

				if(verbose==2) printf("Rcv: ConfigRes\n");

				AAAA=args[6];
				if(AAAA[0]!='\0'){
                	/* add the message source IPaddress to our IPT */
                	IPTset(IPT, atoi(AAAA), cliaddr.sin_addr.s_addr);	
				}
				
				

            }
            break;
		/*
		Message: JNTCIT/Config/MACAddress/IPAddress/IPMask/Gateway/DNS1/DNS2/DHCP
		Type: Broadcast
		Arguments: some of the arguments are optional. In this case back slash / still signifies the parameter place holder
			MACAddress:		0a:ba:ff:10:20:30
			IPAddress:		10.10.0.55
			IPMask:		255.255.255.0
			Gateway:(optional)	10.10.0.1
			DNS1:(optional)	8.8.8.8
			DNS2:(optional)	4.4.4.4
			DHCP:			static, dhcp
	
		Description: CFG sends this packet and only the SIOD device with matching MACAddress will process it. 
					 SIOD will update its network configuration file. In order to apply the new settings 
					 RestartNetworkService message is used. SIODs have WiFi and WAN Ethernet port which are 
					 bridged into br-bat interface. br-bat is the interface which is actually configured 
					 by this command.
		*/
        case Config:{
				//We set our network parameters
				char *MACAddress, *IPAddress, *IPMask, *Gateway, *DNS1, *DNS2, *DHCP;
				unsigned long long MACAddress_num;

				if(n_args != 8) {
					printf("Wrong format of Config message\n");
					return -1;
				}
				MACAddress=args[1]; IPAddress=args[2]; IPMask=args[3]; Gateway=args[4]; DNS1=args[5]; DNS2=args[6]; DHCP=args[7];

				MACAddress_num = MACaddress_str2num(MACAddress);
				if(wifiMAC() == MACAddress_num){ /* We set  parameters*/

                    uciset("network.bat.ipaddr", IPAddress);
                    uciset("network.bat.netmask", IPMask);
                    uciset("network.bat.gateway", Gateway);
                	ucidelete("network.bat.dns");
                	uciadd_list("network.bat.dns", DNS1);
					uciadd_list("network.bat.dns", DNS2);
                    uciset("network.bat.proto", DHCP);

					ucicommit();
			
					if(verbose) printf("bat Config commited\n");


                    sprintf(msg, "JNTCIT/200");
                	if(verbose==2) printf("Sent: %s\n", msg);
                	unicast(msg);
				}

            }
            break;
		/*
		Message: JNTCIT/RestartNetworkService/MACAddress
	     		 JNTCIT/RestartNetworkService/
		Type: Broadcast
		Arguments: the MACAddress  argument is optional
			MACAddress:	(optional)	0a:ba:ff:10:20:30
	
		Description: CFG sends this packet and only the device with this MACAddress will restart its network services. 
				     MACAddress is an optional argument. If it is omitted all receiving SIODs will process the message. 
		*/
        case RestartNetworkService:{
				//We restart network services
				char *MACAddress;
				char IPAddress[STR_MAX];

				MACAddress=args[1];				

				if(verbose==2) printf("Rcv: RestartNetworkService\n");
				
				if(*MACAddress == '\0' || wifiMAC() == MACaddress_str2num(MACAddress)) {

					if(verbose) printf("Restarting the network service\n");

					restartnet();

        			//uciget("network.bat.ipaddr", IPAddress); //Update our IPADR
					getIP(IPAddress);
        			if(IPAddress[0] != '\0')
            			IPADR = IPaddress_str2num(IPAddress);
					else
						IPADR=0;

                    sprintf(msg, "JNTCIT/200");
                    if(verbose==2) printf("Sent: %s\n", msg);
                    unicast(msg);
				}				

            }
            break;
		/*
		Message: JNTCIT/RestartAsterisk
		Type: Unicast
		Arguments: 
		Description: CFG sends this packet to a device which have to restart its Asterisk server.
		*/
        case RestartAsterisk:{

				if(verbose==2) printf("Rcv: RestartAsterisk\n");

				restart_asterisk(); 

            }
            break;
		/*
		Message:JNTCIT/ConfigAsterisk/SIPRegistrar1/AuthenticationName1/Password1/SIPRegistrar2/AuthenticationName2/Password2
		Type: Unicast
		Arguments: some of the arguments are optional. In this case back slash / still signifies the parameter place holder
			SIPRegistrar1:(optional)Main	10.10.0.10
			AuthenticationName1:(optional)	Iana
			Password1:(optional)			1234
			SIPRegistrar2(optional) Backup	20.20.20.20
			AuthenticationName2:(optional)	Eva
			Password2:(optional)			5678
	
	
		Description: the SIOD will adjust its Asterisk configuration files as per the supplied arguments. SIOD Asterisk is configured as a simple SIP client. 
			It means it will not have SIP or other VoIP clients register to it and it will not support dialplan. SIOD Asterisk will register as a SIP user 
			to a single SIP server. If SIOD receive SIP call it will terminate it by an IVR (for IO control purpose). The SIOD Asterisk should qualify the 
			registration to Main registrar. If Main registration fails SIOD will try to register to the Backup registrar. If the Main registrar came up SIOD 
			Asterisk will re-connect to it again. Only SIOD devices process this packet.
		*/ 
        case ConfigAsterisk:{
				char *SIPRegistrar1, *AuthenticationName1, *Password1, *SIPRegistrar2, *AuthenticationName2, *Password2;

				if(verbose==2) printf("Rcv: ConfigAsterisk\n");

				SIPRegistrar1=args[1]; AuthenticationName1=args[2]; Password1=args[3]; SIPRegistrar2=args[4]; AuthenticationName2=args[5]; Password2=args[6];

				asterisk_config_write(SIPRegistrar1, AuthenticationName1, Password1, SIPRegistrar2, AuthenticationName2, Password2);

            }
            break;
		/*
		Message: JNTCIT/AsteriskStatReq
		Type: Unicast
		Arguments: 
		Description: CFG sends this packet to a device which have to report its Asterisk status. 
		*/
		case AsteriskStatReq:{
				char SIPRegistrar1[STR_MAX], AuthenticationName1[STR_MAX], Password1[STR_MAX], SIPRegistrar2[STR_MAX], AuthenticationName2[STR_MAX], Password2[STR_MAX];	
				char ast_uptime[STR_MAX];			

				if(verbose==2) printf("Rcv: AsteriskStatReq\n");

				asterisk_uptime(ast_uptime);

				printf("uptime = %s\n", ast_uptime);

				if(ast_uptime[0] == '\0'){
						
					sprintf(msg, "JNTCIT/AsteriskStatRes/NotRunning////////");

				} else {
				
					/* Read trunk infomation from the Asterisk configuration files */  
					ini_gets("trunk1", "host", "", SIPRegistrar1, STR_MAX, "/etc/asterisk/sip.conf");
					ini_gets("trunk1", "username", "", AuthenticationName1, STR_MAX, "/etc/asterisk/sip.conf");
					ini_gets("trunk1", "secret", "", Password1, STR_MAX, "/etc/asterisk/sip.conf");

                	ini_gets("trunk2", "host", "", SIPRegistrar2, STR_MAX, "/etc/asterisk/sip.conf");
                	ini_gets("trunk2", "username", "", AuthenticationName2, STR_MAX, "/etc/asterisk/sip.conf");
                	ini_gets("trunk2", "secret", "", Password2, STR_MAX, "/etc/asterisk/sip.conf");

					sprintf(msg, "JNTCIT/AsteriskStatRes/Running/%s/%s/%s/%s/%s/%s/%s/", ast_uptime, SIPRegistrar1, AuthenticationName1, Password1, SIPRegistrar2, AuthenticationName2, Password2);

				}

				if(verbose==2) printf("Sent: %s\n", msg);

				unicast(msg);

            }
            break;
		/*
		Message:JNTCIT/AsteriskStatRes/AsteriskState/AsteriskUpTime/SIPRegistrar1/AuthenticationName1/Password1/SIPRegistrar2/AuthenticationName2/Password2/
	    	JNTCIT/AsteriskStatRes/NotRunning////////	
		Type: Unicast
		Arguments: some of the arguments are optional. In this case back slash / still signifies the parameter place holder
			AsteriskState:				NotRunning, Running
			AsteriskUpTime:(optional)		20min
			SIPRegistrar1:(optional)		10.10.0.10
			AuthenticationName1:(optional)	Iana
			Password1:(optional)			1234
			SIPRegistrar2:(optional)		10.10.0.11
			AuthenticationName2:(optional)	Eva
			Password2:(optional)			5678
	
	
		Description: if SIOD doesn't have active Asterisk it responds with NotRunning and all the other arguments are empty. SIOD Asterisk is configured as 
			a simple SIP client. It means it will not have SIP or other VoIP clients register to it and it will not support dialplan. SIOD Asterisk will 
			register as a SIP user to a single SIP server. If SIOD receive SIP call it will terminate it by an IVR (for IO control purpose). The SIOD Asterisk 
			should qualify the registration to Main registrar. If Main registration fails SIOD will try to register to the Backup registrar. If the Main 
			registrar came up SIOD Asterisk will re-connect to it again. Only CFG process this packet. 
		*/
        case AsteriskStatRes:{
			
				if(verbose==2) printf("Rcv: AsteriskStatRes\n");
				
				/* For the moment we do nothing */
            }
            break;
        case ConfigNTP:{
				char *NTPServer0, *NTPServer1, *NTPServer2, *NTPServer3, *enable_disable, *SyncTime;

				if(verbose==2) printf("Rcv: ConfigNTP\n");

                if(n_args != 7) {
                    printf("Wrong format of ConfigNTP message\n");
                    return -1;
                }				
				NTPServer0=args[1]; NTPServer1=args[2]; NTPServer2=args[3]; NTPServer3=args[4]; enable_disable=args[5]; SyncTime=args[6];

				ucidelete("system.ntp.server");
				ucicommit();

				uciadd_list("system.ntp.server", NTPServer0);
                if (*NTPServer1) uciadd_list("system.ntp.server", NTPServer1);                
				if (*NTPServer2) uciadd_list("system.ntp.server", NTPServer2);                
				if (*NTPServer3) uciadd_list("system.ntp.server", NTPServer3);
				uciset("system.ntp.enable_server", enable_disable);
				//SyncTime - ignore for now
				ucicommit();

				if(verbose) printf("NTP configurations updated\n");				

            }
            break;
		/*
		Message: /JNTCIT/Set/X/Y
	     		 /JNTCIT/Set//YYYY	
		Type: Unicast
		Arguments: 
			X:(optional)	number of an output [0, 1, .. 3]. Current version of SIOD supports 4 outputs. 
							X is an optional argument. If omitted it is assumed that YYYY specifies the state of all
 							outputs. 
			Y:  			Active/not active [0, 1]
			YYYY:			represents 4 digit binary number. LSB specifies the state of the first output, MSB of the 4th
							output.     
		Description: This command is process only by the SIOD type of devices. As a result a requested output is activated/deactivated. 
					 On success Put package is broadcasted. If the package arrives in the out of time range moment 
					 no output will be updated and the SIOD will broadcast a TimeRangeOut package. 
		*/
        case Set:{

				int res;
				char *X, *Y;
			
				if(verbose==2) printf("Rcv: Set\n");
	
				X=args[1]; Y=args[2];

				if(CheckTimeRange()){	
					res = setgpio(X, Y); //In addition it updates outputs state in GST if successful
				
					if(!res){
						sprintf(msg, "JNTCIT/Put/%s/%s/%s", SIOD_ID, X, Y);

						if(verbose==2) printf("Sent: %s\n", msg);

						broadcast(msg);


					}	
				} else {
					//Send TimeRangeOut to the caller
					sprintf(msg, "JNTCIT/TimeRangeOut/%s/%s/%s", SIOD_ID, TIMERANGE.Date, TIMERANGE.Time);

					if(verbose==2) printf("Sent: %s\n", msg);

					unicast(msg);

				}

            }
            break;
		/*
		Message: /JNTCIT/PLC/AAAA1/X1/Y1/AAAA2/X2/Y2/and_or/AAAA3/X3/Y3
	     		 /JNTCIT/PLC/AAAA1/X1/Y1///////		
		Type: Unicast
		Arguments:
			AAAA1:			ID of the SIOD to be updated when PLC rule triggers
			X1:				number of local or remote output [0, 1, .. 3]
			Y1:  				Active/not active [0, 1]
			AAAA2:(optional) 		ID of the SIOD to be checked in the PLC rule
			X2:(optional)			number of an IO (outputs (0,1,.. 3) or inputs (4,5,.. 7)) 
			Y2:(optional)  			State which triggers the PLC rule Active/not active [0, 1]
			and_or:(optional)		Can be 'and' or 'or'
			AAAA3:(optional)		ID of the SIOD to be checked in the PLC rule
			X3:(optional)			number of an IO (outputs (0,1,.. 3) or inputs (4,5,.. 7)) 
			Y3:(optional)  			State which triggers the PLC rule Active/not active [0, 1]

		Description: This command is adding a PLC rule into the local SIOD PLC table. Each SIOD is keeping its local table with PLC rules 
					 which may be empty or have one or more PLC rules. Logical conjunction(and) and logical disjunction (or) 
					 of two not necessary local  IOs is supported. Good practice is to have PLC rules having at least one local IO argument. 
					 Triggering of a rule may happen on a local input change, receiving Set or Put package. If the target SIOD 
					 has a time range specified then the rule output can be updated only within this time range. 
					If the local output rule triggers in out of the defined time range a TimeRangeOut package is broadcasted. 
					If local output is updated the SIOD will broadcast a Put package in the mesh so all SIODs update their GST table. 
					If a remote output has to be updated a Set packet is send to the target SIOD. That target SIOD may respond with Put or 
					TimeRangeOut package. If all optional arguments are omitted the PLC rule defined by the triple (AAAA1, X1, Y1) 
					is removed from PLC table. Maximum of 10 PLC rules is supported.  If SIOD already has 10 rules it will respond with 
					PLCRes message.
		*/
        case PLC:{
				char *AAAA1, *X1, *Y1, *AAAA2, *X2, *Y2, *and_or, *AAAA3, *X3, *Y3;

                if(verbose==2) printf("Rcv: PLC\n");
					
				AAAA1=args[1]; X1=args[2]; Y1=args[3]; AAAA2=args[4], X2=args[5]; Y2=args[6]; and_or=args[7]; AAAA3=args[8]; X3=args[9]; Y3=args[10];	

				if(AAAA2[0] != '\0'){   //Add a rule
					char rule[STR_MAX];
					
					sprintf(rule, "%s/%s/%s/%s/%s/%s/%s/%s/%s/%s", AAAA1, X1, Y1, AAAA2, X2, Y2, and_or, AAAA3, X3, Y3);

					PLCadd(rule);

				} else{				 //delete a rule

					PLCdel(AAAA1, X1, Y1);
				}

            }
            break;
		/*
		Message: /JNTCIT/PLCReq
		Type: Unicast
		Arguments:
		Description: CFG or other node in the mesh can unicast this message if he want to get the set of the PLC rules from a given SIOD.
		*/
        case PLCReq:{
				char PLCstr[MSG_MAX];
				//Send our PLC table

                if(verbose==2) printf("Rcv: PLCReq\n");

				PLCprint(PLCstr);

				sprintf(msg, "/JNTCIT/PLCRes/%s", PLCstr);

				if(verbose==2) printf("Sent: %s\n", msg);

				unicast(msg);					
            }
            break;
		/*
		Message: /JNTCIT/PLCRes/AAAA11/X11/Y11/AAAA21/X21/Y21/and_or1/AAAA31/X31/Y31/AAAA12/X12/Y12/AAAA22/X22/Y22/and_or2/AAAA32/X31/Y32 ...
		Type: Unicast
		Arguments:
			AAAA11:		ID of the SIOD to be updated when PLC rule triggers
			X11:			number of local or remote output [0, 1, .. 3]
			Y11:  			Active/not active [0, 1]
			AAAA21 		ID of the SIOD to be checked in the PLC rule
			X21			number of an IO (outputs (0,1,.. 3) or inputs (4,5,.. 7)) 
			Y21  			State which triggers the PLC rule Active/not active [0, 1]
			and_or1		Can be 'and' or 'or'
			AAAA31		ID of the SIOD to be checked in the PLC rule
			X31			number of an IO (outputs (0,1,.. 3) or inputs (4,5,.. 7)) 
			Y31  			State which triggers the PLC rule Active/not active [0, 1]
			...	

		Description: This message provides information of the PLC rules stored in the SIOD. Maximum of 10 PLC rules per SIOD is supported. 
		*/
        case PLCRes:{

                if(verbose==2) printf("Rcv: PLCReq\n");
				
				//Do nothing
            }
            break;
		/*
		Message: /JNTCIT/TimeRange/Date/Time

		Examples:
	     	Daylight     			
	     	/JNTCIT/TimeRange//7:00:00-23:59:59
	     	Night 	
	     	/JNTCIT/TimeRange//0:0:0-6:59:59
	     	Saturday	
	     	/JNTCIT/TimeRange/6/
	     	Sunday	
	     	/JNTCIT/TimeRange/0/0:0:0-23:59:59
	     	Arbitrary single range 	
	     	/JNTCIT/TimeRange/20July2015/14:00:00-14:59:59
	     	To clear the time range
	     	/JNTCIT/TimeRange///			
		Type: Unicast
		Arguments:
			Date:(optional)		Exact date 20July2015 or day in the week index (Monday 1, Sunday 0). Date ranges
								like 20July2015-25July2015 or 3-5 is also supported. Optional argument. If omitted 
								any date assumed.
			Time:(optional)		Time range specification in a format. hh1:mm1:ss1- hh2:mm2:ss2. If the second time 
								point is smaller then the first one (and the Date doesn't specify a range) it is assumed 
								that the second time is a sample from the next day. Optional argument, if omitted any 
								time withing specified Date assumed.    			

		Description: This command is defining a Time Range for the SIOD. This package overwrites previously defined time range. 
					 A PLC rule can have timing constrain enabled in which case the rule output is adjusted only if withing 
					 the defined SIOD time range. If the rule triggers but out of time range then TimeRangeOut package 
					 is broadcasted. Set command is working only if within the time range, otherwise  TimeRangeOut is broadcasted
		*/
        case TimeRange:{

				char *Date, *Time;
				char TimeRangeStr[STR_MAX];

                if(verbose==2) printf("Rcv: TimeRange\n");

				Date=args[1]; Time[2];
				
				sprintf(TimeRangeStr, "%s/%s", Date, Time);

				ParseTimeRange(TimeRangeStr);

            }
            break;
		/*
		Message: /JNTCIT/TimeRangeOut/AAAA/Date/Time
		Type: Broadcast
		Arguments:
			AAAA:	ID of the local SIOD
			Date:		Exact date 20July2015 or day in the week index (Monday 1, Sunday 7). Date ranges
						like 20July2015-25July2015 or 3-5 is also supported. Optional argument. If omitted 
						any date assumed.
			Time:		Time range specification in a format. hh1:mm1:ss1- hh2:mm2:ss2. If the second time 
						point is smaller then the first one (and the Date doesn't specify a range) it is assumed
						that the second time is a sample from the next day. Optional argument, if omitted any 
						range withing specified Date assumed.  			

		Description: This packet is broadcasted each time a local output needs to be adjusted at the moment 
					 outside of a defined time range (see Set and PLC packages). 
					 If SIOD doesn't have time range TimeRangeOut is never sent.
		*/ 
        case TimeRangeOut:{
				char *AAAA;

                if(verbose==2) printf("Rcv: TimeRangeOut\n");

				AAAA = args[1];

                /* add the message source IPaddress to our IPT */
                IPTset(IPT, atoi(AAAA), cliaddr.sin_addr.s_addr);

            }
            break;        
		/*
		Message: /JNTCIT/Get/X
	     		 /JNTCIT/Get/	
		Type: Unicast
		Arguments: 
				   X:(optional)		number of an output/input [0, 1, .. 7] optional argument, 
									if omitted the state of all IOs are requested 	
		Description: This command is process only by the SIOD type of devices.  Put package with the requested IO value is send back to the requester. 
		*/
		case Get:{

                int res;
                char *X, Y[STR_MAX];

                if(verbose==2) printf("Rcv: Get\n");

                X=args[1];

                res = getgpio(X, Y); //In addition if successful the function updates GST

                if(!res){
                    sprintf(msg, "JNTCIT/Put/%s/%s/%s", SIOD_ID, X, Y);

                    if(verbose==2) printf("Sent: %s\n", msg);

					unicast(msg);
                }
            }
            break;        
		/*
		Message: /JNTCIT/Put/AAAA/X/Y
	     		 /JNTCIT/Put/AAAA//YYYYYYYY	
		Type: Broadcast
		Arguments: All arguments are mandatory
			AAAA:	ID of the SIOD
			X:(optional)	number of an output /input [0, 1, .. 7]
							optional argument. If omitted it is assumed that YYYYYYYY specifies the state of all
							IOs. 
			Y:    			Active/not active [0, 1]
			YYYYYYYY:		8 digit binary number.(We have 8 IOs per SIOD) LSB specifies the state of the first IO, 				
							MSB of the 8th IO.
		Description: Each SIOD device in the mesh is holding the whole information of IOs for all SIODs. 
					 The information is maintained by the Global Status Table (GST). GST should stay in sync for all SIODs. 
					 This packet is filling a line in the GST for the given SIOD ID and IO number.  
		*/
		case Put:{
				char *AAAA, *X, *Y;
				unsigned char gpios;
				int res, fifofd;

                if(verbose==2) printf("Rcv: Put\n");

				AAAA=args[1]; X=args[2]; Y=args[3];

				/* Update the local GST with the information from the message */
				if(!strcmp(AAAA, SIOD_ID)) {
					if(verbose==2) printf("Ignoring Put message for our SIOD_ID\n");	
					break;  				
				}

				/* add the message source IPaddress to our IPT */
                IPTset(IPT, atoi(AAAA), cliaddr.sin_addr.s_addr);

				if(X[0] != '\0'){ // Empty X
					res=GSTget(GST, atoi(AAAA), &gpios);					
					if(res == -1){
						printf("/JNTCIT/Put/AAAA/X/Y message ignored as we don't have SIOD_ID=%s in the GST\n", AAAA);
						break; 
					}
					
					GSTset(GST, atoi(AAAA), (Y[0]=='1')?(gpios|(1<<atoi(X))):(gpios&~(1<<atoi(X))));

				} else {
					GSTadd(GST, atoi(AAAA), binarystr2byte(Y));

					if(IVRSet_counter) {	//the Put message is due to IVRSetReq message
                    	fifofd=open("/tmp/ivrfifo", O_WRONLY|O_NONBLOCK);
                    	sprintf(msg, "JNTCIT/IVRGetRes/%s/%s/%s",AAAA,X,Y);
                    	if(verbose==2) printf("Sent: %s\n", msg);
                    	write(fifofd, msg, strlen(msg));
                    	close(fifofd);
					}
				}	
			
            }
            break;
		/*	
		Message: /JNTCIT/GSTCheckSumReq
		Type: Broadcast
		Arguments: 	
		Description: Each SIOD device in the mesh is holding the whole information of IOs for all SIODs. 
					 The information is maintained by the Global Status Table (GST). GST should stay in sync for all SIODs. 
					 Once in a while each SIOD (or CFG) can broadcast this request for a GST check sum. 
					 After receiving the check sum from the other nodes based on some heuristics it can decide to request 
					 a GST from a certain SIOD.
        */
		case GSTCheckSumReq:{
				
                if(verbose==2) printf("Rcv: GSTCheckSumReq\n");

				/* Send our GST check sum */
				sprintf(msg, "JNTCIT/GSTCheckSum/%s/%d", SIOD_ID, GSTchecksum(GST));
				if(verbose==2) printf("Sent: %s\n", msg);
				broadcast(msg);
            }
           	break; 
		/*
		Message: /JNTCIT/GSTCheckSum/AAAA/Sum
		Type: Broadcast
		Arguments: All arguments are mandatory
			AAAA:		SIOD ID	
			Sum:		The Check Sum of the GST
		Description: Each SIOD device in the mesh is holding the whole information of IOs for all SIODs. 
					 The information is maintained by the Global Status Table (GST). GST should stay in sync for all SIODs. 
					 Once in a while each SIOD (or CFG) can broadcast this request for a GST check sum. 
					 After receiving the check sum from the other nodes based on some heuristics it can decide to request 
					 a GST from a certain SIOD. With this package all SIODs broadcasts the check sum of the GST they hold. 
					 GST checksum is defined as an algebraic sum of all octets modulo 256 in the GST data. See the GST packet example.
		*/
		case GSTCheckSum:{

				char *AAAA, *Sum;
				unsigned char sum;

                if(verbose==2) printf("Rcv: GSTCheckSum\n");
								
				AAAA=args[1]; Sum=args[2];

				/* add the message source IPaddress to our IPT */
                IPTset(IPT, atoi(AAAA), cliaddr.sin_addr.s_addr);
					
				/* For now just print if our GST checksum matches */
				sum = GSTchecksum(GST);
				if(atoi(Sum) == sum)
					printf("Got checksum which match with ours\n");
				else
					printf("From %s got checksum %s. It differs from our GST checksum %d\n", AAAA, Sum, sum);

            }
            break;
		/*
		Message: /JNTCIT/GSTReq
		Type: Unicast
		Arguments: 	
		Description: Each SIOD device in the mesh is holding the whole information of IOs for all SIODs. 
					 The information is maintained by the Global Status Table (GST). GST should stay in sync for all SIODs. 
					 A SIOD requests the GST from a certain SIOD using this message. Note that no special efforts have been made 
					 to sort the data records in the GST, so they may be not bit exact in all SIODs. They should represent however 
					 the same SIOD states. The algebraic checksum used is invariant to swapping of the records. 
					 If the checksum of the two GST is the same it is assumed that their GST are the same.
		*/
		case GSTReq:{

				char GSTtextdata[STR_MAX];

                if(verbose==2) printf("Rcv: GSTReq\n");
			
				/* Send our GST */
				GSTprint(GST, GSTtextdata);
				sprintf(msg, "JNTCIT/GSTdata/%s", GSTtextdata);
				if(verbose==2) printf("Sent: %s\n", msg);
				unicast(msg);

            }
            break;
		/*
		Message: /JNTCIT/GSTdata/Data
		Type: Unicast
		Arguments: 	Data is mandatory argument
		Data: 	GST sent in the UDP message body. The following text format is used  
				SIOD_ID0, IO_STATE; ...
				Example:
 				1000,255;1001,14; ...
				For a SIOD_ID=1000 all the outputs (IO3..IO0) are set to be active and all the inputs IO7..IO4 reads 
				as logic high.
				For a SIOD_ID=1001 all the outputs (IO3..IO0) are set to be inactive except IO3 and all the inputs
				(IO7..IO4) reads as logic low except IO4.
				The above partial GST data has check sum (algebraic sum of all data octets module 256) 228
		Description: Each SIOD device in the mesh is holding the whole information of IOs for all SIODs. 
					 The information is maintained by a data structure called Global Status Table (GST). 
					 GSTs should stay in sync for all SIODs. This message pass the whole GST data. 
					 Note that no special efforts have been made to sort the data records in the GST, 
					 so they may be not bit exact in all SIODs. They should represent however the same SIOD states. 
					 The algebraic checksum used is invariant to swapping of the records. 
					 If the checksum of the two GST is the same it is assumed that they are the same.
		*/ 
        case GSTdata:{

				char *Data;

                if(verbose==2) printf("Rcv: GSTdata\n");

				Data=args[1];

				/* For now just display the received GST data */
				printf("%s\n", Data);

            }
            break;
		/*
		Message: /JNTCIT/Ping/AAAA  (broadcast)
	     		 /JNTCIT/Ping/	    (unicast)
		Type: Unicast or Broadcast
		Arguments:
			AAAA:(optional)		SIOD ID 	
		Description: Each SIOD is keeping IP Address, SIOD ID pair in a local table. 
					 Sometimes it is convenient to be able to retrieve this information from the mesh. 
					 If a mesh node knows SIOD IP address is available in the mesh and would like to get
					 SIOD ID it can unicast Ping message without AAAA argument. 
					 If an IP address for a given SIOD ID is requested the node may broadcast Ping message with AAAA argument. 
					 Mesh will respond with PingRes message.
		*/
        case Ping:{

				char *AAAA, IPAddressWiFi[STR_MAX];

                if(verbose==2) printf("Rcv: Ping\n");

				if(n_args != 2 ) {
					printf("Wrong format of Ping message\n");
					break;
				}

				AAAA = args[1];

				if(AAAA[0]=='\0' || !strcmp(AAAA, SIOD_ID)) { //AAAA is empty or our SIOD_ID matches so we we need to respond
					sprintf(msg, "JNTCIT/PingRes/%s", SIOD_ID);
					if(verbose==2) printf("Sent: %s\n", msg);
					unicast(msg);											
				}

			}
            break;
		/*
		Message: /JNTCIT/PingRes/AAAA/IPAddressWiFi/IPAddressWAN
	     		 /JNTCIT/PingRes/AAAA/IPAddressWiFi/
		Type: Broadcast or Broadcast
		Arguments:
			AAAA:					local SIOD ID
			IPAddressWiFi:			local WiFi IP address.
			IPAddressWAN:(optional)	local WAN IP address, this argument is not used if WAN is not configured as
									mesh entry point
  		 	
		Description: SIOD responding to Ping message. In the data of the message the SIOD ID and WiFi IP address are included. 
					 If the SIOD uses its WAN as batman-adv bridge then the WAN IP address is returned too.
		*/
        case PingRes:{
				//TBD We will reconsider using of the IPAddressWAN here
				char *AAAA;

                if(verbose==2) printf("Rcv: PingRes\n");

				AAAA = args[1];

				/* add the message source IPaddress to our IPT */
				IPTset(IPT, atoi(AAAA), cliaddr.sin_addr.s_addr);

			}
            break;
        /*                                                                                                                                                                                         
        Message: JNTCIT/IVRGetReq/AAAA/X/Y                                                                                                                                                         
        Type: Unicast (local host)                                                                                                                                                                 
        Arguments:                                                                                                                                                                                 
            AAAA: SIOD ID                                                                                                                                                                          
            X:    number of an IO [0, 1, .. 7]. Current version of SIOD supports 7 IOs.                                                                                                            
        Description: Asterisk IVR sends this message (to the local socket_io listener) if IO state from the SIOD mesh is required.                                                                 
                     The local socket_io  return IO state information using GST. This command is process only by the SIOD type of devices.                                                         
        */                                                                                                                                                                                         
        case IVRGetReq:{                                                                                                                                                                           
                char *AAAA, *X;
				unsigned char gpios;                                                                                                                                                                    
                int fifofd,n;      
                                                                                                                                                                             
                if(verbose==2) printf("Rcv: IVRGetReq\n");                                                                                                                                         
                                                                                                                                                                                                   
                AAAA = args[1]; X = args[2];                                                                                                                                                       
                
                fifofd=open("/tmp/ivrfifo", O_WRONLY|O_NONBLOCK);                                                                                                                                                                   
				if(GSTget(GST, atoi(AAAA), &gpios)){
                    sprintf(msg, "JNTCIT/IVRGetRes///");  //SIOD not available in the GST, assumed not available in the mesh
                    if(verbose==2) printf("Sent: %s\n", msg);
                    write(fifofd, msg, strlen(msg));					
				} else {
														 //SIOD available
                    sprintf(msg, "JNTCIT/IVRGetRes/%s/%s/%d",AAAA,X,(gpios>>(X[0]-'0'))&1);
                    if(verbose==2) printf("Sent: %s\n", msg);
                    n=write(fifofd, msg, strlen(msg));
				}                                                                                                                                                                                        
                close(fifofd);                                                                                                                                                                                   
            }                                                                                                                                                                                      
            break;                                                                                                                                                                                 
        /*                                                                                                                                                                                         
        Message: JNTCIT/IVRGetRes/AAAA/X/Y                                                                                                                                                         
                 JNTCIT/IVRGetRes///                                                                                                                                                               
        Type: Unicast (local host)                                                                                                                                                                 
        Arguments:                                                                                                                                                                                 
            AAAA:(optional)     SIOD ID                                                                                                                                                            
            X:(optional)        number of an IO [0, 1, .. 7]. Current version of SIOD supports 7 IOs.                                                                                              
            Y:(optional)        Active/not active [0, 1]                                                                                                                                           
        Description: socket_io responds with this message (to the local Asterisk IVR). If AAAA not available in GST AAAA, X and Y are empty.                                                       
        */                                                                                                                                                                                         
        case IVRGetRes:{                                                                                                                                                                           
                                                                                                                                                                                                   
                if(verbose==2) printf("Rcv: IVRGetRes\n");                                                                                                                                         
                                                                                                                                                                                                   
                //We should never get this                                                                                                                                                         
                                                                                                                                                                                                   
            }                                                                                                                                                                                      
            break;
		/*
		Message: JNTCIT/IVRSetReq/AAAA/X/Y	
		Type: Unicast (local host)
		Arguments: 
			AAAA: SIOD ID
			X:	  number of an IO [0, 1, .. 3]. Current version of SIOD supports 4 outputs.
			Y:  	 Active/not active [0, 1] 
		Description: Asterisk IVR sends this message (to the local socket_io listener) if output state change of a SIOD in the mesh is required. 
					 The local socket_io  broadcasts Set message and if it gets a Put message sends IVRSetRes to the IVR. This command is process 
					 only by the SIOD type of devices.
		*/ 
        case IVRSetReq:{
                char *AAAA, *X, *Y;
                unsigned char gpios;
                int fifofd,n, res;

                if(verbose==2) printf("Rcv: IVRSetReq\n");

                AAAA = args[1]; X = args[2]; Y = args[3];

				if(!strcmp(AAAA, SIOD_ID)){
                	if(CheckTimeRange()){
                    	res = setgpio(X, Y); //In addition it updates outputs state in GST if successful
                    	if(!res){
                        	sprintf(msg, "JNTCIT/Put/%s/%s/%s", SIOD_ID, X, Y);
                        	if(verbose==2) printf("Sent: %s\n", msg);
                        	broadcast(msg);

							fifofd=open("/tmp/ivrfifo", O_WRONLY|O_NONBLOCK);
                    		sprintf(msg, "JNTCIT/IVRSetRes/%s/%s/%s", AAAA, X, Y);
                    		if(verbose==2) printf("Sent: %s\n", msg);
                    		write(fifofd, msg, strlen(msg));
                    		close(fifofd);
                    	}
                	} else {
                    	//Send TimeRangeOut to the caller
						fifofd=open("/tmp/ivrfifo", O_WRONLY|O_NONBLOCK);
                    	sprintf(msg, "JNTCIT/IVRSetRes/TimeRangeOut");
                    	if(verbose==2) printf("Sent: %s\n", msg);
                    	write(fifofd, msg, strlen(msg));
                    	close(fifofd);                	}

                } else if(GSTget(GST, atoi(AAAA), &gpios)){     
					fifofd=open("/tmp/ivrfifo", O_WRONLY|O_NONBLOCK);
                    sprintf(msg, "JNTCIT/IVRSetRes///");  //SIOD not available in the GST, assumed not available in the mesh
                    if(verbose==2) printf("Sent: %s\n", msg);
                    write(fifofd, msg, strlen(msg));
					close(fifofd);
                } else {
                										  //SIOD available
                    //broadcast Put message
                    sprintf(msg, "JNTCIT/Put/%s/%s/%s", AAAA, X, Y);
                    if(verbose==2) printf("Sent: %s\n", msg);
                    broadcast(msg);
					IVRSet_counter=1;						 //Start IVRSet timer
                }
            }
            break;
		/*
		Message: JNTCIT/IVRSetRes/AAAA/X/Y
	     		 JNTCIT/IVRSetRes/// 	
		Type: Unicast (local host)
		Arguments: 
			AAAA: SIOD ID
			X:	  number of an IO [0, 1, .. 3]. Current version of SIOD supports 4 outputs.
			Y:  	 Active/not active [0, 1]  
		Description: socket_io sends this message to Asterisk IVR to confirm output change. JNTCIT/IVRSetRes/// is send if SIOD AAAA 
					 is not available in the mesh. JNTCIT/IVRGetRes/TimeRangeOut  is send if output adjustment is trying in the out 
					 of the SIOD specified range. This command is process only by the SIOD type of devices.
		*/
        case IVRSetRes:{

                if(verbose==2) printf("Rcv: IVRSetRes\n");

                //We should never get this                                                                                                                                                         

            }
            break;
                                                                                                                                                                                 
	}

	return 0;

}

/*
 * Removes spaces in string
 */
void RemoveSpaces(char* source)
{
	char *i = source;
	char *j = source;
  
	while(*j != 0){
    		*i = *j++;
    		if(*i != ' ') i++;
  	}

  	*i = 0;
}

/*
 * The function searches for the posible match of s2 inside s1
 * Returns 0 if match found 
 */
int strfind(const char *s1, const char *s2){

	int i, len1, len2;

	len1=strlen(s1);
	len2=strlen(s2);
	for(i=0; i<len1; i++){

		if(s1[i]==s2[0]){
			if (!strncmp(&(s1[i]), s2, len2)){
				break;
			}
		}
	}

	return (i==len1)?-1:0;
}
/*
 * The function extracts the arguments from the UDP datagram. 
 * Standard separator '/' is assumed
 */
int extract_args(char *datagram, char *args[], int *n_args){

	int i, len;
	
	*n_args=1;
	args[*n_args-1]=datagram;
	len=strlen(datagram);	
	for(i=0;i<len; i++){
		if(datagram[i] == '/'){
			args[(*n_args)++]=&datagram[i+1];
			datagram[i]='\0';
		}
	}
	
	return 0;	
}

/*
 * Calculates index of the command string so we can use C switch   
 */
int hashit(char *cmd) {

	int i;
	for(i=0; i<sizeof(cmds)/sizeof(char *); i++){
		if(!strcmp(cmd, cmds[i])) return i; 
	}

	return -1;
}

/*
 * Convert MAC address in string format xx:xx:xx:xx:xx:xx into u64 value   
 */
unsigned long long MACaddress_str2num(char *MACaddress){
	
	unsigned char mac[6];
	int ret;	

	ret=sscanf(MACaddress, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[5], &mac[4], &mac[3], &mac[2], &mac[1], &mac[0]);
	if(ret == 6)
		return (unsigned long long)mac[0] | ((unsigned long long)mac[1]<<8) | ((unsigned long long)mac[2]<<16) | ((unsigned long long)mac[3]<<24) | ((unsigned long long)mac[4]<<32) | ((unsigned long long)mac[5]<<40);
	else {
		if(verbose) printf("Wrong MAC address format.\n");
		return 0;
	}
		
}

/*
 * Convert MAC address from a u64 value into string format xx:xx:xx:xx:xx:xx
 * the string should be allocated by the caller
 */
void MACaddress_num2str(unsigned long long MACaddress, char *MACaddress_str){


    sprintf(MACaddress_str, "%02x:%02x:%02x:%02x:%02x:%02x", (unsigned char)(MACaddress>>40), (unsigned char)(MACaddress>>32), (unsigned char)(MACaddress>>24),
															 (unsigned char)(MACaddress>>16), (unsigned char)(MACaddress>>8), (unsigned char)(MACaddress));

}

/*
 * Convert IP address in string format d.d.d.d into u32 value   
 */
unsigned long IPaddress_str2num(char *IPaddress){

    unsigned int ip[4];
    int ret;

    ret=sscanf(IPaddress, "%u.%u.%u.%u", &ip[3], &ip[2], &ip[1], &ip[0]);
    if(ret == 4)
        return (unsigned long)(ip[0]&0xff) | ((unsigned long)(ip[1]&0xff)<<8) | ((unsigned long)(ip[2]&0xff)<<16) | ((unsigned long)(ip[3]&0xff)<<24);
    else {
        if(verbose) printf("Wrong IP address format.\n");
        return 0;
    }

}

/*
 * Convert IP address from a u32 value into string format d.d.d.d
 * the string should be allocated by the caller
 */
void IPaddress_num2str(unsigned long IPaddress, char *IPaddress_str){


    sprintf(IPaddress_str, "%d.%d.%d.%d", (unsigned char)(IPaddress>>24), (unsigned char)(IPaddress>>16), (unsigned char)(IPaddress>>8), (unsigned char)(IPaddress));

}

/*
 * Convert string to upper case
 */
char *strupr(char *s){ 
	unsigned c; 
    unsigned char *p = (unsigned char *)s; 
    while (c = *p) *p++ = toupper(c);

	return s; 
}


/*
 * Retreive local eth0 MAC address
 */ 
unsigned long long eth0MAC(void){
	int fd;
	char mac[18];
	
	fd = open("/sys/class/net/eth0/address", O_RDONLY);

	read(fd, mac, 18);

	close(fd);

	return MACaddress_str2num(mac);
}

/*
 * Retreive local eth1 MAC address
 */
unsigned long long eth1MAC(void){
    int fd;
    char mac[18];

    fd = open("/sys/class/net/eth1/address", O_RDONLY);

    read(fd, mac, 18);

    close(fd);

    return MACaddress_str2num(mac);
}

/*
 * Retreive local WiFi MAC address
 */
unsigned long long wifiMAC(void){
    int fd;
    char mac[18];

    fd = open("/sys/class/net/br-bat/address", O_RDONLY);

    read(fd, mac, 18);

    close(fd);

    return MACaddress_str2num(mac);
}

/*
 * Execute uci get command to retreive a value from the openwrt configuration files 
 * Value should have at least STR_MAX bytes alocated.
 * returns 0 on success
 */
int uciget(const char *param, char *value){

    FILE *fp;
	char *ret, str[100];
	int i, len;	



	sprintf(str, "uci get %s", param);

    fp=popen(str,"r");
    ret=fgets(value, STR_MAX, fp);
    pclose(fp);

	len=strlen(value);
	if(value[len-1]=='\r' || value[len-1]=='\n') value[len-1]='\0';

	if(ret==NULL){
		value[0]=' ';	//return a space string so we can still create valid UDP message 
		value[0]='\0';
		return -1;
	}else
		return 0;
}

/*
 * Execute uci set command to update value to the openwrt configuration files
 * Note that you have to commit the change afterwords
 * returns 0 on success
 */
int uciset(const char *param, const char *value){

    FILE *fp;
    char str[100];
	char dummy[STR_MAX];

    sprintf(str, "uci set %s=%s 2>&1", param, value);

    fp=popen(str,"r");
    fgets(dummy, STR_MAX, fp);
    pclose(fp);

    if(!strfind(dummy, "Invalid"))
        return -1;
    else
        return 0;
}


/*
 * Execute uci delete command in order to delete an option 
 * or all list items from  the openwrt configuration files
 * Note that you have to commit the change afterwords
 * returns 0 on success
 */
int ucidelete(const char *param){

    FILE *fp;
    char str[100];
    char dummy[STR_MAX];

    sprintf(str, "uci delete %s 2>&1", param);

    fp=popen(str,"r");
    fgets(dummy, STR_MAX, fp);
    pclose(fp);

    if(!strfind(dummy, "Invalid"))
        return -1;
    else
        return 0;
}


/*
 * Execute uci add_list command in order to add new item to the list 
 * Note that you have to commit the change afterwords
 * returns 0 on success
 */
int uciadd_list(const char *param, const char *value){

    FILE *fp;
    char str[100];
    char dummy[STR_MAX];

    sprintf(str, "uci add_list %s=%s 2>&1", param, value);

    fp=popen(str,"r");
    fgets(dummy, STR_MAX, fp);
    pclose(fp);

    if(!strfind(dummy, "Invalid"))
        return -1;
    else
        return 0;
}


/*
 * Commit all changes (in the config files) done by uci set commands
 */
void ucicommit(void){

    FILE *fp;

    fp=popen("uci commit","r");
    pclose(fp);
}


/*
 * Restart network services
 */
void restartnet(void){

    FILE *fp;

	char dummy[STR_MAX];

	close(bcast_sockfd); //Close broadcasting socket

    fp=popen("/etc/init.d/network restart 2>&1","r");

	while(fgets(dummy, STR_MAX, fp) != NULL){
		printf("%s\n", dummy);

	}

    if(pclose(fp)==-1)
		printf("Issue reloading network services");


	bcast_init();	//Initialize broadcasting socket again	

}



/*
 * Get the uptime in seconds.
 * Value should have at least STR_MAX bytes alocated.
 *
 */
void uptime(char *uptime){

    FILE *fp;
	int len;

    fp=popen("cut -d ' ' -f 1 </proc/uptime","r");

    fgets(uptime, STR_MAX, fp);

	len=strlen(uptime);
    if(uptime[len-1]=='\r' || uptime[len-1]=='\n') uptime[len-1]='\0';

}

/*
 * Get the software version.
 * ver should have at least STR_MAX bytes alocated.
 */
int getsoftwarever(char *ver){

    FILE *fp;
	char *ret;	
	int len;

    fp=popen("cat /etc/banner | grep 'Version: .*'| cut -f3- -d' '","r");
    ret=fgets(ver, STR_MAX, fp);
    pclose(fp);

	len=strlen(ver);
    if(ver[len-1]=='\r' || ver[len-1]=='\n') ver[len-1]='\0';
	if(ver[len-2]==' ') ver[len-2]='\0';

    if(ret==NULL)
        return -1;
    else
        return 0;
}


/*
 * Broadcast UDP message
 */
int broadcast(char *msg){

	return(sendto(bcast_sockfd,msg,strlen(msg),0, (struct sockaddr *)&bcast_servaddr,sizeof(bcast_servaddr)));

}


/*
 * Unicast UDP message
 */
int unicast(char *msg){

	if(verbose==3) {
		char IPaddress_str[STR_MAX];
		IPaddress_num2str(cliaddr.sin_addr.s_addr, IPaddress_str);
		printf("unicast to %s\n", IPaddress_str);
	}
	
	cliaddr.sin_port = htons(PORT); // Make sure we send on proper port

	return(sendto(udpfd,msg,strlen(msg),0, (struct sockaddr *)&cliaddr,sizeof(cliaddr)));
}


/*
 * Initialize SIOD GPIOs. 
 * The function opens 'value' file descriptors
 */
int gpios_init(void){

	int fd, n, /*fb0, fb1, fb2, fb3*/ out0, out1, out2, out3;
	char value[2], str[STR_MAX];

	GPIOs=0;

	/* Export the GPIOS */
	fd = open("/sys/class/gpio/export", O_WRONLY);
	n = snprintf(str, STR_MAX, "%d", REL0);
	write(fd, str, n);
	n = snprintf(str, STR_MAX, "%d", REL1);
	write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", S_R);
    write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", PULSE);
    write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", FB0);
    write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", FB1);
    write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", FB2);
    write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", FB3);
    write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", IN0);
    write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", IN1);
    write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", IN2);
    write(fd, str, n);
    n = snprintf(str, STR_MAX, "%d", IN3);
    write(fd, str, n);
	close(fd);

	/* Configure inputs */
	snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", FB0);
	fd = open(str, O_WRONLY);
	write(fd, "in", 2);
	close(fd);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", FB1);
    fd = open(str, O_WRONLY);
    write(fd, "in", 2);
    close(fd);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", FB2);
    fd = open(str, O_WRONLY);
    write(fd, "in", 2);
    close(fd);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", FB3);
    fd = open(str, O_WRONLY);
    write(fd, "in", 2);
    close(fd);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", IN0);
    fd = open(str, O_WRONLY);
    write(fd, "in", 2);
    close(fd);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", IN1);
    fd = open(str, O_WRONLY);
    write(fd, "in", 2);
    close(fd);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", IN2);
    fd = open(str, O_WRONLY);
    write(fd, "in", 2);
    close(fd);    
	snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", IN3);
    fd = open(str, O_WRONLY);
    write(fd, "in", 2);
    close(fd);


	/* read the inputs */
	snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", IN0);
	fd_in0 = open(str, O_RDONLY);
	read(fd_in0, value, 2); value[2]='\0';
	GPIOs |= ((!atoi(value)) << 4);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", IN1);
    fd_in1 = open(str, O_RDONLY);
    read(fd_in1, value, 2); value[2]='\0';
    GPIOs |= ((!atoi(value)) << 5);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", IN2);
    fd_in2 = open(str, O_RDONLY);
    read(fd_in2, value, 2); value[2]='\0';
    GPIOs |= ((!atoi(value)) << 6);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", IN3);
    fd_in3 = open(str, O_RDONLY);
    read(fd_in3, value, 2); value[2]='\0';
    GPIOs |= ((!atoi(value)) << 7);

	
	/* read the relay feedbacks */
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", FB0);
    fd_fb0 = open(str, O_RDONLY);
    //read(fd_fb0, value, 2); value[2]='\0';
    //fb0=atoi(value);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", FB1);
    fd_fb1 = open(str, O_RDONLY);
    //read(fd_fb1, value, 2); value[2]='\0';
    //fb1=atoi(value);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", FB2);
    fd_fb2 = open(str, O_RDONLY);
    //read(fd_fb2, value, 2); value[2]='\0';
    //fb2=atoi(value);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", FB3);
    fd_fb3 = open(str, O_RDONLY);
    //read(fd_fb3, value, 2); value[2]='\0';
    //fb3=atoi(value);


	/* configure outputs */
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", REL0);
    fd = open(str, O_WRONLY);
    write(fd, "low", 3);
    close(fd);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", REL1);
    fd = open(str, O_WRONLY);
    write(fd, "low", 3);
    close(fd);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", S_R);
    fd = open(str, O_WRONLY);
    write(fd, "low", 3);
    close(fd);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/direction", PULSE);
    fd = open(str, O_WRONLY);
    write(fd, "high", 4);
    close(fd);

	/* Relays descriptors */
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", REL0);
    fd_rel0 = open(str, O_WRONLY);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", REL1);
    fd_rel1 = open(str, O_WRONLY);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", S_R);
    fd_s_r = open(str, O_WRONLY);
    snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", PULSE);
    fd_pulse = open(str, O_WRONLY);    


    /* Initialize the file descriptors in an array to ease indexing */
    IOs[0]=fd_fb0; IOs[1]=fd_fb1; IOs[2]=fd_fb2; IOs[3]=fd_fb3;
    IOs[4]=fd_in0; IOs[5]=fd_in1; IOs[6]=fd_in2; IOs[7]=fd_in3;



	/* Set the output information in GPIOS 
     * GPIOS = [IN3 IN2 IN1 IN0 OUT3 OUT2 OUT1 OUT0]
   	 *          MSB                            LSB   
	 */
	/*GPIOs |= !(fb0);
	GPIOs |= (!(fb1) << 1);
	GPIOs |= (!(fb2) << 2);
	GPIOs |= (!(fb3) << 3);
	*/
	uciget("siod.@output[0].value", str);
	setgpio("0", str);
	uciget("siod.@output[1].value", str);
	setgpio("1", str);
    uciget("siod.@output[2].value", str);
	setgpio("2", str);
    uciget("siod.@output[3].value", str);
	setgpio("3", str);
    
	if(verbose) printf("GPIOs = 0x%x\n", GPIOs);

	return 0;
}


/*
 * set local gpio
 *
 *	X:(optional)	index of the output [0, 1, .. 3] Current version of SIOD supports 4 outputs. 
 *					X can be empty string. If empty it is assumed that YYYY specifies the state of all outputs. 
 *	Y:  			Active/not active "0" or "1"
 *	YYYY:			represent 4 digit binary number.(We have 4 outputs per SIOD) LSB specifies the state of the first IO, 			
 *					MSB of the 4th output. 
 *
 * The function updates outputs states in GST if successful
 */
int setgpio(char *X, char *Y){

	int x, n, xlen, ylen;
	char str[STR_MAX], value[2] ;
	
	xlen=strlen(X);
	ylen=strlen(Y);

	if(xlen == 1 && ylen == 1){
		
		x=atoi(X);
		if (x < 0 || x > OUTPUTS_NUM) {
			if(verbose) printf("Output index out of range, ignoring\n");
			return -1;
		} else if (Y[0] !='0' && Y[0] !='1') {
			if(verbose) printf("Output value should be 0 or 1\n");
			return -1;
		}
    
    	write(fd_rel0, (x>1)?"1":"0", 1); write(fd_rel1, (x%2)?"1":"0", 1);
    	write(fd_s_r, Y, 1);
    	write(fd_pulse, "0", 1); usleep(100000L); write(fd_pulse, "1", 1);
		      
		GPIOs = (Y[0]-'0')?(GPIOs|(1<<x)):(GPIOs&~(1<<x));

		if(verbose == 2) printf("Set: OUT%d = %s\n", x, Y);

		//Update GST
		GST[0].gpios=GPIOs;

		//Update the configs
		sprintf(str, "siod.@output[%d].value", x);
		uciset(str, Y);
		ucicommit();

	} else if (xlen == 0 && ylen == OUTPUTS_NUM){
		int i;
		
		for(i=0;i<ylen;i++){
			if (Y[i] == '0' || Y[i] == '1'){
				
					
        		write(fd_rel0, (i>1)?"1":"0", 1); write(fd_rel1, (i%2)?"1":"0", 1);
        		write(fd_s_r, (Y[OUTPUTS_NUM-1-i]-'0')?"1":"0", 1);
        		write(fd_pulse, "0", 1); usleep(100000L); write(fd_pulse, "1", 1);

				GPIOs = (Y[OUTPUTS_NUM-1-i]-'0')?(GPIOs|(1<<i)):(GPIOs&~(1<<i));

				if(verbose == 2) printf("Set: OUT%d = %d\n", i, Y[OUTPUTS_NUM-1-i]-'0');

				//Update the configs
        		sprintf(str, "siod.@output[%d].value", i);
				sprintf(value, "%c", Y[OUTPUTS_NUM-1-i]);
        		uciset(str, value);	
			}
		}

		ucicommit();

        //Update GST
        GST[0].gpios=GPIOs;

	} else {
		printf("setgpio: Invalid X and Y\n");
		return -1;
	} 

	return 0;

}

/*
 * Get local gpio
 * 
 *   X:(optional)    index of the output [0, 1, .. 8], 
 *					 optional argument, if empty the state of all IOs are returned
 *                   results are returned in a strin Y. It must be alocated by the caller
 *
 *   If successful the function updates GST 
 */
int getgpio(char *X, char *Y){

    int x, xlen;
    char str[STR_MAX];

    xlen=strlen(X);
    if(xlen == 1){

        x=atoi(X);
        if (x < 0 || x > 8) {
            if(verbose) printf("IO index out of range, ignoring\n");
            return -1;
		}

        snprintf(str, STR_MAX, "/sys/class/gpio/gpio%d/value", IOs[x]);
		lseek(IOs[x], 0, SEEK_SET);
        read(IOs[x], Y, 2); Y[2]='\0';
		Y[0]=(Y[0]=='0')?'1':'0'; Y[1]='\0';	//Invererse logic for the inputs and feedbacks

		if(verbose==3) printf("getgpio: IO%d = %s\n", x, Y);
		
		//Update GST
		GPIOs = (Y[0]-'0')?(GPIOs&~(1<<x)):(GPIOs|(1<<x));
		GST[0].gpios=GPIOs;

    } else if (xlen == 0){
        int i;
        for(i=0;i<INPUTS_NUM+OUTPUTS_NUM;i++){
			lseek(IOs[i], 0, SEEK_SET);
			read(IOs[i], str, 2); str[2]='\0';
			Y[INPUTS_NUM+OUTPUTS_NUM-1-i]=(str[0]=='0')?'1':'0'; //Invererse logic for the inputs and feedbacks

			if(verbose==3) printf("getgpio: IO%d = %s\n", i, (str[0]=='0')?"1":"0");

			GPIOs = (str[0]-'0')?(GPIOs&~(1<<i)):(GPIOs|(1<<i));
        }
		Y[i]='\0';

		//Update GST
		GST[0].gpios=GPIOs;

    } else {
        printf("getgpio: X must be empty or represent a number \n");
        return -1;
    }

    return 0;
}


/*
 * Calculates checksum of GST data. The data are terminated by zero siod_id
 * 
 * Note that no special efforts have been made to sort the data records in the GST, so they may be not bit exact in all SIODs. 
 * They should represent however the same SIOD states. The algebraic checksum used is invariant to swapping of the records. 
 * If the checksum of the two GST is the same it is assumed that they are the same. 
*/
unsigned char GSTchecksum(struct GST_nod *gst){

	unsigned char sum;
	unsigned short siod_id;
	int i;
	sum=0; i=0;
	while(siod_id = (gst+i)->siod_id) {
		sum += (gst+i)->gpios + (siod_id&0xff) + ((siod_id>>8)&0xff);
		i++;	
	}
	
	return(sum);
}

/*
 * Add siod_id, gpios data pair in the GST
 * If siod_id already available only the gpios value is updated
 * 
 */
void GSTadd(struct GST_nod *gst, unsigned short siod_id, unsigned char gpios){

	int i;
	i=0;
    while((gst+i)->siod_id) {
		if ((gst+i)->siod_id == siod_id) { //The siod_id found
			(gst+i)->gpios = gpios;
			return;
		}

		i++;

		if(i>=SIODS_MAX) {
			printf("Can not add, too much GST items already!\n");
			return;
		}
	}

	(gst+i)->siod_id = siod_id;
	(gst+i)->gpios = gpios;
}


/*
 * Remove item from GST. If siod_id is not found the GST is unchanged
 * 
 */
void GSTdel(struct GST_nod *gst, unsigned short siod_id){

    int i, j;

    i=0; j=-1;
	while((gst+i)->siod_id){
		if ((gst+i)->siod_id == siod_id) {
			j=i;
		}	
		i++;
	} 

	if(j != -1) { //If siod_id is found
		if(j==(i-1)){ //siod_id is last item in GST
			(gst+j)->siod_id = 0;	//make it zero
			(gst+j)->gpios = 0;		
		} else {
			(gst+j)->siod_id =  (gst+i-1)->siod_id;   //Copy last item on top of the one we delete
			(gst+j)->gpios = (gst+i-1)->gpios; 		
			
			(gst+i-1)->siod_id = 0;					  // and delete the last item 	
			(gst+i-1)->gpios = 0; 		
		}
	}
	
}

/*
 * Retreive a gpios for a given siod_id from the GST
 * 0 if siod_id found in the GST, -1 otherwise 
 */
int GSTget(struct GST_nod *gst, unsigned short siod_id, unsigned char *gpios){

    int i;
    i=0;

    while((gst+i)->siod_id){
        if ((gst+i)->siod_id == siod_id) {
            *gpios = (gst+i)->gpios;
            return 0;
        }
        i++;
    }

    return -1;
}

/*
 * Set gpios for a given siod_id to the GST
 * 0 if siod_id found in the GST, -1 otherwise 
 */
int GSTset(struct GST_nod *gst, unsigned short siod_id, unsigned char gpios){

    int i;
    i=0;

    while((gst+i)->siod_id){
        if ((gst+i)->siod_id == siod_id) {
            (gst+i)->gpios = gpios;
            return 0;
        }
        i++;
    }

    return -1;
}


/*
 * Print GST in a string. 
 * Note that caller should allocate the str memory
 */
void GSTprint(struct GST_nod *gst, char *str){

    int i;
	char gpios[9], item[STR_MAX];	

	str[0]='\0';i=0;
    while((gst+i)->siod_id){
        byte2binarystr((gst+i)->gpios, gpios);
		sprintf(item, "%d,%s;", (gst+i)->siod_id, gpios);
		strcat(str, item);
		i++;
    }
}

/*
 * Convert byte to str representing 8 digit binary equivalent
 * str should be allocated by the caller
 */
void byte2binarystr(int n, char *str){

   int c, d, i;
 
   i = 0;
   for (c = 7; c >= 0; c--){
		d = n>>c;
		*(str+i) = (d & 1)?'1':'0';
		i++;
   }

   *(str+i) = '\0';
}

/*
 * Convert str representing 8 digit binary equivalent into a byte
 */
unsigned char binarystr2byte(char *str){
	int i;	
	unsigned char byte;	

	if(strlen(str) != 8 ){
		printf("binarystr2byte: str shoould be 8 chars long\n");
		return -1;
	}

	byte=0;
	for(i=0;i<8;i++){
		if(str[7-i]-'0') byte |= (1<<i);
	}
	
	return byte;

}

/*
 * Retreive an IP address for a given siod_id from the IPT
 * 0 if siod_id found in the IPT, -1 otherwise 
 */
int IPTget(struct IPT_nod *ipt, unsigned short siod_id, unsigned long *IPaddress){

    int i;
    i=0;

    while((ipt+i)->siod_id){
        if ((ipt+i)->siod_id == siod_id) {
            *IPaddress = (ipt+i)->IPaddress;
            return 0;
        }
        i++;
    }

    return -1;
}

/*
 * Set IP address for a given siod_id to the IPT
 * If siod_id item not available in the IPT we add it at the end 
 */
void IPTset(struct IPT_nod *ipt, unsigned short siod_id, unsigned long IPaddress){

    int i;
    i=0;

    while((ipt+i)->siod_id){
        if ((ipt+i)->siod_id == siod_id) {
            (ipt+i)->IPaddress = IPaddress;
            return;
        }
        i++;
    }

	(ipt+i)->siod_id == siod_id;
	(ipt+i)->IPaddress = IPaddress;	

}

/*
This function parse the TimeRnage as defined by the message

	Examples:
	     Daylight     			
	     /7:00:00-23:59:59

	     Night 	
	     /0:0:0-6:59:59

	     Saturday	
	     6/

	     Sunday	
	     0/0:0:0-23:59:59

	     Arbitrary single range 	
	     20July2015/14:00:00-14:59:59

	     To clear the time range
	     /			
Arguments:
	Date:(optional)		Exact date 20July2015 or day in the week index (Monday 1, Sunday 0). Date ranges
						like 20July2015-25July2015 or 3-5 is also supported. Optional argument. If omitted
						any date assumed.
	Time:(optional)		Time range specification in a format. hh1:mm1:ss1- hh2:mm2:ss2. If the second time
						point is smaller then the first one (and the Date doesn't specify a range) it is assumed
						that the second time is a sample from the next day. Optional argument, if omitted any
						range withing specified Date assumed.     
Function returns 0 if manage to parse the data or -1 otherwise
*/
int ParseTimeRange(char *TimeRangeStr){

	char date[STR_MAX], time[STR_MAX], date1[STR_MAX], date2[STR_MAX],  time1[STR_MAX], time2[STR_MAX], start[STR_MAX], end[STR_MAX];
	char TimeRangeStr_[STR_MAX];
	char *args[2];
	int n_args, repetitive_date,  repetitive_time; 


	repetitive_date=0;
	repetitive_time=0;	
	strcpy(TimeRangeStr_, TimeRangeStr);
	extract_args(TimeRangeStr_, args,  &n_args);
	if(n_args != 2){
        printf("Wrong TimeRange format\n");

		return -1;
	}

	strcpy(date, args[0]);
	strcpy(time, args[1]);

	strcpy(TIMERANGE.Date, date);
	strcpy(TIMERANGE.Time, time);

	
	if(date[0]=='\0') {strcpy(date, "0-0"); repetitive_date=1;}//So strptime parsing works
	if(time[0]=='\0') {strcpy(time, "00:00:00-00:00:00"); repetitive_time=1;} //So strptime parsing works

	if(strfind(date, "-")){
		strcpy(date1, date);
		strcpy(date2, date);
    } else if(sscanf(date,"%[^-]-%[^-]", date1, date2) != 2){
        printf("Wrong TimeRange format\n");
        return -1;
    }

    if(strfind(time, "-")){
        strcpy(time1, time);
        strcpy(time2, time);
    } else if(sscanf(time,"%[^-]-%[^-]", time1, time2) != 2){
        printf("Wrong TimeRange format\n");
        return -1;
    }

	sprintf(start, "%s %s", date1, time1);
	sprintf(end, "%s %s", date2, time2);

	if (strptime(start, "%d%b%Y %H:%M:%S", &TIMERANGE.start) == 0){
		if (strptime(start, "%w %H:%M:%S", &TIMERANGE.start) == 0)
			return -1;
	}
		

    if (strptime(end, "%d%b%Y %H:%M:%S", &TIMERANGE.end) == 0){
        if (strptime(end, "%w %H:%M:%S", &TIMERANGE.end) == 0)
            return -1;
    }

	if(repetitive_date){
		TIMERANGE.start.tm_mday=-1;
		TIMERANGE.start.tm_mon=-1;
		TIMERANGE.start.tm_year=-1;

        TIMERANGE.end.tm_mday=-1;
        TIMERANGE.end.tm_mon=-1;
        TIMERANGE.end.tm_year=-1;
	}

    if(repetitive_time){
        TIMERANGE.start.tm_sec=-1;
        TIMERANGE.start.tm_min=-1;
        TIMERANGE.start.tm_hour=-1;

        TIMERANGE.end.tm_sec=-1;
        TIMERANGE.end.tm_min=-1;
        TIMERANGE.end.tm_hour=-1;
    }

	return 0;
}

/*
 *	Check if current system time is in the TimeRange
 *	Returns 1 if in time range and 0 otherwise  
 */
int CheckTimeRange(void){

	time_t start, end, now;
	struct tm start_, end_, now_;

	start_=TIMERANGE.start;
	end_=TIMERANGE.end;

	/* now */
	time(&now);
	localtime_r(&now, &now_);

	if(TIMERANGE.start.tm_mday == -1 ){

		if(verbose==2) printf("CheckTimeRange: Date repetition\n");

		start_.tm_mday =  now_.tm_mday;
		start_.tm_year = now_.tm_year;
		start_.tm_mon = now_.tm_mon;

		end_.tm_mday =  now_.tm_mday;
		end_.tm_year = now_.tm_year;
		end_.tm_mon = now_.tm_mon;

		if(TIMERANGE.start.tm_min != -1 ){
			start=mktime(&start_);
			end=mktime(&end_);
		}

	}
    
	if(TIMERANGE.start.tm_min == -1 ){

		if(verbose==2) printf("CheckTimeRange: Time repetition\n");

		start_.tm_sec =  now_.tm_sec;
		start_.tm_min = now_.tm_min;
		start_.tm_hour = now_.tm_hour;

		end_.tm_sec =  now_.tm_sec;
		end_.tm_min = now_.tm_min;
		end_.tm_hour = now_.tm_hour;

		start=mktime(&start_);
		end=mktime(&end_);
	}

	if (TIMERANGE.start.tm_year == 0){ //Date defined as week day. 

		if(verbose==2) printf("CheckTimeRange: Week repetition\n");

		/* into time_t converting to moments around now */
		start_.tm_mday =  now_.tm_mday;
		start_.tm_year = now_.tm_year;
		start_.tm_mon = now_.tm_mon;
		start=mktime(&start_);

		end_.tm_mday =  now_.tm_mday;
		end_.tm_year = now_.tm_year;
		end_.tm_mon = now_.tm_mon;
		end=mktime(&end_);

		start = start+(TIMERANGE.start.tm_wday-now_.tm_wday)*SECSINDAY;

		if(TIMERANGE.end.tm_wday >= TIMERANGE.start.tm_wday){

			if(verbose==2) printf("CheckTimeRange: Interval span a single week (sun, mon .. sat)\n");

			end = end+(TIMERANGE.end.tm_wday-now_.tm_wday)*SECSINDAY;

		} else {
			
			if(verbose==2) printf("CheckTimeRange: 'end' is from the next week\n");

			end = end+(TIMERANGE.end.tm_wday-now_.tm_wday+DAYSINWEEK)*SECSINDAY;
		}

		return (now>=start && now<=end)?1:0;


	} else { //Concret start-stop


		if(verbose==2) printf("CheckTimeRange: concrete range\n");

		if(verbose==3) printf("start=%d, end=%d, now=%d\n", start, end, now);

		return (now>=start && now<=end)?1:0;

	}

}

/*
 * The tripel (AAAA1, X1, Y1) defines a rule in PLC table
 * If that rule found it will be deleted from PLC table
 *
 * Function return 0 if a rule has been added, -1 otherwise
 */
int PLCadd(char *rule){

	char rule_[STR_MAX],*AAAA1,*X1,*Y1,*AAAA2,*X2,*Y2,*and_or,*AAAA3,*X3,*Y3;
	char *PLC_args[10];
    int PLC_n_args;

	if(PLCT.n>=RULES_MAX){

		printf("Can not add PLC rule, We already have %d rules\n", RULES_MAX);
		return -1;
	}

	strcpy(rule_, rule);
	extract_args(rule_, PLC_args, &PLC_n_args);	
	AAAA1=PLC_args[0],X1=PLC_args[1],Y1=PLC_args[2],AAAA2=PLC_args[3],X2=PLC_args[4],Y2=PLC_args[5],and_or=PLC_args[6],AAAA3=PLC_args[7],X3=PLC_args[8],Y3=PLC_args[9];


	/* Check if rule is valid */
	if(atoi(AAAA1)<1000 || atoi(AAAA1)>9999){
		printf("PLCadd: AAAA1 must represent 4 digit number\n");
        return -1;
	} 
    if(atoi(AAAA2)<1000 || atoi(AAAA2)>9999){
        printf("PLCadd: AAAA2 must represent 4 digit number\n");
        return -1;
    }
    if(atoi(AAAA3)<1000 || atoi(AAAA3)>9999){
        printf("PLCadd: AAAA3 must represent 4 digit number\n");
        return -1;
    }
    if(atoi(X1)<0 || atoi(X1)>3){
        printf("PLCadd: X1 should be 0,1,2 or 3\n");
        return -1;
    }
    if(atoi(X2)<0 || atoi(X2)>7){
        printf("PLCadd: X2 should be in the range [0,1, ... 7]\n");
        return -1;
    }
    if(atoi(X3)<0 || atoi(X3)>7){
        printf("PLCadd: X3 should be in the range [0,1, ... 7]\n");
        return -1;
    }
    if(strlen(Y1)!=1 || (Y1[0]!='0' && Y1[0]!='1')){
        printf("PLCadd: Y1 should be 0 or 1\n");
        return -1;
    }
    if(strlen(Y2)!=1 || (Y2[0]!='0' && Y2[0]!='1')){
        printf("PLCadd: Y2 should be 0 or 1\n");
        return -1;
    }
    if(strlen(Y3)!=1 || (Y3[0]!='0' && Y3[0]!='1')){
        printf("PLCadd: Y3 should be 0 or 1\n");
        return -1;
    }
    if(strcmp(and_or, "and") && strcmp(and_or, "or")){
        printf("PLCadd: and_or parameter should be 'and' or 'or' \n");
        return -1;
    }


	/* Check for rule duplications */
	if(PLC_n_args>=3)
		PLCdel(AAAA1, X1, Y1); //So we are sure no rules duplications 

	PLCT.rules[PLCT.n]=malloc(strlen(rule));
	if(PLCT.rules[PLCT.n] == NULL){
		printf("PLCadd: Can not allocate memory\n");
		return -1;
	}

	strcpy(PLCT.rules[PLCT.n], rule);

	PLCT.triggered[PLCT.n]=0;

	PLCT.n++;

	return 0;
}


/*
 * The tripel (AAAA1, X1, Y1) defines a rule in PLC table
 * If that rule	found it will be deleted from PLC table
 *
 * Function return 0 if a rule has been deleted, -1 otherwise
 */
int PLCdel(char *AAAA1, char *X1, char *Y1){
	int i, j, PLC_n_nargs;
	char *PLC_args[10];
	int PLC_n_args;
	char rule_[STR_MAX];
                   
	for(i=0;i<PLCT.n;i++){

		strcpy(rule_, PLCT.rules[i]);
		extract_args(rule_, PLC_args, &PLC_n_args);

		if(PLC_n_args >=3 && !strcmp(PLC_args[0], AAAA1) && !strcmp(PLC_args[1], X1) && !strcmp(PLC_args[2], Y1)){

			free(PLCT.rules[i]);     	//delete the rule memory;
		
			for(j=i;j<PLCT.n-1;j++){	//Move the pointers so no holes in PLCT.rules array
			                       
				PLCT.rules[j] = PLCT.rules[j+1];

				PLCT.triggered[j] = PLCT.triggered[j+1];
			}
				
			PLCT.n--;           	//Reduce rules count

			return 0;
		}
	}

	return -1;	
}

/*
 * Prints the rules in our PLC table
 * PLC should be allocated by the caller with a memory big enought
 */
void PLCprint(char *PLCstr){

	int i;
	PLCstr[0]='\0';
	for(i=0;i<PLCT.n;i++) {
		strcat(PLCstr, PLCT.rules[i]);
		strcat(PLCstr, "/");
	}
	PLCstr[strlen(PLCstr)-1]='\0';//Remove the final '/'

}

/*
 * Check the PLC rules and triggers those whos conditions met
 * Ment to be executed periodically (for example once each 100ms) 
 * The function reads the local inputs and update the GST
 */
void PLCexec(void){

    int i;
    char rule_[STR_MAX], Y[STR_MAX];
    char *args[10];
    int n_args;
	char *AAAA1, *X1, *Y1, *AAAA2, *X2, *Y2, *and_or, *AAAA3, *X3, *Y3;
	unsigned char gpios1, gpios2;
	char msg[MSG_MAX];
	unsigned long ipaddress;

	getgpio("", Y);//Use to update GST, result in GPIOs

    for(i=0;i<PLCT.n;i++) {

        strcpy(rule_, PLCT.rules[i]);
        extract_args(rule_, args, &n_args);

		AAAA1=args[0]; X1=args[1]; Y1=args[2]; AAAA2=args[3], X2=args[4]; Y2=args[5]; and_or=args[6]; AAAA3=args[7]; X3=args[8]; Y3=args[9];	

		if(verbose==3) printf("rule[%d]: %s/%s/%s/%s/%s/%s/%s/%s/%s/%s\n", i, AAAA1,X1,Y1,AAAA2,X2,Y2,and_or,AAAA3,X3,Y3);

		if(GSTget(GST, atoi(AAAA2), &gpios1)) continue;
		if(GSTget(GST, atoi(AAAA3), &gpios2)) continue;

		if(verbose==3) printf("gpios1=0x%x, gpios2=0x%x\n", gpios1, gpios2);

		if((!strcmp(and_or, "or"))?((((gpios1>>(X2[0]-'0'))&1) == (Y2[0]-'0')) || (((gpios2>>(X3[0]-'0'))&1) == (Y3[0]-'0'))): \
								   ((((gpios1>>(X2[0]-'0'))&1) == (Y2[0]-'0')) && (((gpios2>>(X3[0]-'0'))&1) == (Y3[0]-'0')))){
			//Trigger rule i if it is not already trigered
			if(PLCT.triggered[i]==0){
				if(!strcmp(AAAA1, SIOD_ID)){//Have to set a local output
  
					setgpio(X1, Y1);

					//broadcast Put message
					sprintf(msg, "JNTCIT/Put/%s/%s/%s", SIOD_ID, X1, Y1);
					if(verbose==2) printf("Sent: %s\n", msg);
					broadcast(msg);				
	
				} else {					//remote output need to be set				

					/* AAAA1 -> IPaddress from IPT */
					if(!IPTget(IPT, atoi(AAAA1), &ipaddress)){
                    						//Unicast Set to AAAA1

						sprintf(msg, "JNTCIT/Set/%s/%s", X1, Y1);

                    	if(verbose==2) printf("Sent: %s\n", msg);

						cliaddr.sin_addr.s_addr=ipaddress;
						unicast(msg);					
					}
					
				}
			
				PLCT.triggered[i]=1;		
			}

		} else{
			//rearm rule i
			PLCT.triggered[i]=0;
    	}

	}
}

/*
    Initalize the broadcasting socket
*/
void bcast_init(void){
	int enabled;

    bcast_sockfd=socket(AF_INET,SOCK_DGRAM,0);
    enabled = 1;
    setsockopt(bcast_sockfd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled));
    setsockopt(bcast_sockfd, SOL_SOCKET, SO_BINDTODEVICE, "br-bat", IFNAMSIZ-1);
    bzero(&bcast_servaddr,sizeof(bcast_servaddr));
    bcast_servaddr.sin_family = AF_INET;
    bcast_servaddr.sin_addr.s_addr=inet_addr("255.255.255.255");
    bcast_servaddr.sin_port=htons(PORT);
}


/*
 * Restart asterisk server
 */
void restart_asterisk(void){

    FILE *fp;

    fp=popen("asterisk -rx 'core restart now'","r");
    pclose(fp);
}

/*
 * Updates asterisk configurations
 */
void asterisk_config_write(char *SIPRegistrar1, char *AuthenticationName1, char *Password1, char *SIPRegistrar2, char *AuthenticationName2, char *Password2){

	char regstr[STR_MAX];

	/* Remove register lines */
	ini_puts("general", "register", NULL, "/etc/asterisk/sip.conf");
	ini_puts("general", "register", NULL, "/etc/asterisk/sip.conf");

	/* Update register lines */
	sprintf(regstr, "%s:%s@%s", AuthenticationName1, Password1, SIPRegistrar1);
	ini_adds("general", "register", regstr, "/etc/asterisk/sip.conf");

    sprintf(regstr, "%s:%s@%s", AuthenticationName2, Password2, SIPRegistrar2);
	ini_adds("general", "register", regstr, "/etc/asterisk/sip.conf");
	
	/* Update trunk1 and trunk2 sections */
	ini_puts("trunk1", "username", AuthenticationName1, "/etc/asterisk/sip.conf");
	ini_puts("trunk1", "host", SIPRegistrar1, "/etc/asterisk/sip.conf");
	ini_puts("trunk1", "secret", Password1, "/etc/asterisk/sip.conf");

    ini_puts("trunk2", "username", AuthenticationName2, "/etc/asterisk/sip.conf");
    ini_puts("trunk2", "host", SIPRegistrar2, "/etc/asterisk/sip.conf");
    ini_puts("trunk2", "secret", Password2, "/etc/asterisk/sip.conf");


}

/*
 * Read asterisk uptime. 
 * If Asterisk is not started asterisk_uptime is made null string
 * asterisk_uptime needs to be allocated by the caller
 */
void asterisk_uptime(char *uptime){


    FILE *fp;
    char *ret;
	int i, len;

	uptime[0]='\0';
    fp=popen("asterisk -rx 'core show uptime' 2>&1 | sed -n -e 's/^.*Last reload: //p'","r");
    ret=fgets(uptime, STR_MAX, fp);
    pclose(fp);

	//Remove trailing space and LF
	len=strlen(uptime);
	if(uptime[len-1] == 0xa) uptime[len-1] = '\0';
	if(uptime[len-2] == ' ') uptime[len-2] = '\0';
}
/*
 * Timer for the IVR command.
 * Called 10 times per second 
 */
void IVRSetTimer(void){
	
	if (!IVRSet_counter) return;

	if (IVRSet_counter++> IVRSETTIMEOUT){
		IVRSet_counter=0;
	} 
}

/*
 * Get the IP address of the br-bat interface
 * IP should have at least STR_MAX bytes alocated.
 */
void getIP(char *IP){
    FILE *fp;
    char *ret;
	int len;

    fp=popen("ifconfig br-bat | grep 'inet addr:' | cut -d: -f2 | awk '{ print $1}'","r");
    ret=fgets(IP, STR_MAX, fp);
    pclose(fp);

    len=strlen(IP);
    if(IP[len-1]=='\r' || IP[len-1]=='\n') IP[len-1]='\0';

}

/*
 * Get the IP netmask of the br-bat interface
 * mask should have at least STR_MAX bytes alocated.
 */
void getIPMask(char *mask){
    FILE *fp;
    char *ret;
	int len;

    fp=popen("ifconfig br-bat | grep 'inet addr:' | cut -d: -f4","r");
    ret=fgets(mask, STR_MAX, fp);
    pclose(fp);

    len=strlen(mask);
    if(mask[len-1]=='\r' || mask[len-1]=='\n') mask[len-1]='\0';

}
