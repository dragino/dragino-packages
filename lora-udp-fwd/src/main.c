#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <uci.h>
#include "base64.h"

static char b64[256];

/* socket for connect to server */
static struct addrinfo *res;
static struct ifreq ifr;
static int sock_up, sock_down;

/* Set center frequency */
static uint32_t  freq = 868100000; /* in Mhz! (868.1) */

/* Set location */
static float lat=0.0;
static float lon=0.0;
static int   alt=0;

#define CHARLEN 64  /* Length of uci option value */  
/* Informal status fields */
static char platform[CHARLEN] = "LG01/OLG01";  /* platform definition */
static char description[CHARLEN] = "";                        /* used for free form description */
static char server[CHARLEN] = "server";
static char email[CHARLEN]  = "mail";                        /* used for contact email */
static char LAT[CHARLEN] = "lati";
static char LON[CHARLEN] = "long";
static char gatewayid[CHARLEN] = "gateway_id";
static char port[CHARLEN] = "port";
static char sf[CHARLEN] = "SF";
static char bw[CHARLEN] = "BW";
static char coderate[CHARLEN] = "coderate";
static char frequency[CHARLEN] = "rx_frequency";
static char pfwd_debug[CHARLEN] = "pfwd_debug";

static uint32_t net_mac_h; /* Most Significant Nibble, network order */
static uint32_t net_mac_l; /* Least Significant Nibble, network order */

#define BUFLEN 2048  //Max length of buffer

#define TX_BUFF_SIZE  2048
#define STATUS_SIZE	  1024

#define PUSH_TIMEOUT_MS     100
#define PULL_TIMEOUT_MS     200

static struct timeval push_timeout_half = {0, (PUSH_TIMEOUT_MS * 500)}; /* cut in half, critical for throughput */
static struct timeval pull_timeout = {0, (PULL_TIMEOUT_MS * 1000)}; /* non critical for throughput */

#define DATAFILE "/var/iot/data"

#define DLPATH "/var/iot/dldata"

#define PROTOCOL_VERSION  1
#define PKT_PUSH_DATA 0
#define PKT_PUSH_ACK  1
#define PKT_PULL_DATA 2
#define PKT_PULL_RESP 3
#define PKT_PULL_ACK  4


/*******************************************************************************
 *
 * Load UCI Configure and get values!
 *
 *******************************************************************************/
#define UCI_CONFIG_FILE "/etc/config/lorawan"
static struct uci_context * ctx = NULL; 

bool get_option_value(const char *section, char *option)
{
    struct uci_package * pkg = NULL;
    struct uci_element *e;
    const char *value;
    bool ret = false;

    ctx = uci_alloc_context(); 
    if (UCI_OK != uci_load(ctx, UCI_CONFIG_FILE, &pkg))  
        goto cleanup;   /* load uci conifg failed*/

    uci_foreach_element(&pkg->sections, e)
    {
        struct uci_section *st = uci_to_section(e);

        if(!strcmp(section, st->e.name))  /* compare section name */ {
            if (NULL != (value = uci_lookup_option_string(ctx, st, option))) {
                     bzero(option, CHARLEN);
                     strncpy(option, value, CHARLEN); 
                     ret = true;
                     break;
            }
        }
    }
    uci_unload(ctx, pkg); /* free pkg which is UCI package */
cleanup:
    uci_free_context(ctx);
    ctx = NULL;
    return ret;
}


/* signal for hangup receive message 
void sigalrm(int signo)
{

}
*/

void Syslog(int type, char *fmt, ...)
{
    int debug = 0;

    debug = atoi(pfwd_debug);

    if (debug) {
        va_list ap;
        int ind = 0;
        char *st;
        char buf[BUFLEN] = {'\0'};

        va_start(ap, fmt);

        while (*fmt) {
            if (*fmt == '%') {
                st = va_arg(ap, char *);
                strcpy((buf + ind), st);
                ind = ind + strlen(st);
                *fmt++;
            } else {
                buf[ind] = *fmt;
                ind++;
            }

            *fmt++;
        }

        va_end(ap);

        syslog(type, "%s", buf);

        bzero(buf, BUFLEN);
    }
}

void die(const char *st)
{
    closelog();
    freeaddrinfo(res);
    exit(1);
}

/* convert hex to dec */
int todec(char ch)
{
    if(ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if(ch >= 'A' && ch <='F') {
        return ch - 'A' + 10;
    }

    if(ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }

    return -1;
}

void parser_txpk(char *buff, char *name, char *opt)
{
    int i = 0;

    char tmp[BUFLEN]; 

    char * pt_name;

    strncpy(tmp, buff, sizeof(tmp));

    printf("parser %s\n", opt);

    if (NULL != (pt_name = strstr(tmp, opt))) { /* txpk is json obj such as: "txpk": {  "size": 17,  "data": "IPqAKXQmYVCDy8K3k4"}*/
        while (*pt_name != ':') 
            pt_name++;
        while (*pt_name != ',') {
            if (*pt_name == '}') break;
            if (*pt_name == ':' || *pt_name == ' ' || *pt_name == '"') {
                pt_name++;
                continue;
            } else {
                name[i] = *pt_name;
                pt_name++;
                i++;
            }
        }
        name[i] = '\0';
    }

    printf("parser %s: %s\n", opt, name);
}

static double difftimespec(struct timespec end, struct timespec beginning) {
	double x;
	
	x = 1E-9 * (double)(end.tv_nsec - beginning.tv_nsec);
	x += (double)(end.tv_sec - beginning.tv_sec);
	
	return x;
}


/* sendto udp server */
void sendudp(char *msg, int length, int h, int l) 
{
    int i, j;

    char buf[BUFLEN];
    uint8_t buff_ack[32]; /* buffer to receive acknowledges */

    struct addrinfo *q; 

    /* ping measurement variables */
    struct timespec send_time;
    struct timespec recv_time;

    for (q = res; q != NULL; q = q->ai_next) {
        sock_up = socket(q->ai_family, q->ai_socktype, q->ai_protocol);
        if (sock_down == -1) continue; /* try next field */
        else break; /* success, get out of loop */
    }   

    if (connect(sock_up, q->ai_addr, q->ai_addrlen) != 0) { 
        Syslog(LOG_ERR, "ERROR: [down] connect returned %s", strerror(errno));
        die("[up]connect");
    }

    if (setsockopt(sock_up, SOL_SOCKET, SO_RCVTIMEO, \
                (void *)&push_timeout_half, sizeof(push_timeout_half)) != 0) {
        Syslog(LOG_ERR, "ERROR: [up] setsockopt returned %s", strerror(errno));
        die("[up]setsockopt");
    }

    /*if (sendto(sock_up, (char *)msg, length, 0 , res->ai_addr, res->ai_addrlen) == -1) */
    if (send(sock_up, (void *)msg, length, 0) == -1) {
        Syslog(LOG_ERR, "[up] sendudp error");
        die("[up]sendto()");
    }

    clock_gettime(CLOCK_MONOTONIC, &send_time);

	/* wait for acknowledge (in 2 times, to catch extra packets) */
	for (i=0; i<2; ++i) {
		j = recv(sock_up, (void *)buff_ack, sizeof(buff_ack), 0);
		clock_gettime(CLOCK_MONOTONIC, &recv_time);
		if (j == -1) {
			if (errno == EAGAIN) { /* timeout */
			    //Syslog(LOG_INFO, "[up]: server connection timeout");
				continue;
			} else { /* server connection error */
			    Syslog(LOG_ERR, "ERROR[up]: server connection error");
				break;
			}
		} else if ((j < 4) || (buff_ack[0] != PROTOCOL_VERSION) || (buff_ack[3] != PKT_PUSH_ACK)) {
			Syslog(LOG_WARNING, "WARNING: [up] ignored invalid non-ACL packet");
			continue;
		} else if ((buff_ack[1] != (uint8_t)h) || (buff_ack[2] != (uint8_t)l)) {
			Syslog(LOG_WARNING, "WARNING: [up] ignored out-of sync ACK packet");
			continue;
		} else {
			Syslog(LOG_INFO, "INFO: [up] PUSH_ACK received in %i ms", (int)(1000 * difftimespec(recv_time, send_time)));
			break;
		}
	}
}

void pull_data()
{
    int i, j, dec; 

    int keepalive_time = 5;

    struct addrinfo *q; 

	/* local timekeeping variables */
	struct timespec send_time; /* time of the pull request */
	struct timespec recv_time; /* time of return from recv socket call */

	/* data buffers */
	uint8_t buff_down[BUFLEN]; /* buffer to receive downstream packets */
	uint8_t buff_req[16]; /* buffer to compose pull requests */
	int msg_len;

	/* protocol variables */
	uint8_t token_h; /* random token for acknowledgement matching */
	uint8_t token_l; /* random token for acknowledgement matching */
	bool req_ack = false; /* keep track of whether PULL_DATA was acknowledged or not */

    /* for parser b64 to bin */
    char down_size[8] = "0";
    char down_data[STATUS_SIZE] = {'\0'};
	uint8_t payload[STATUS_SIZE] = {'\0'}; 

    for (q = res; q != NULL; q = q->ai_next) {
        sock_down = socket(q->ai_family, q->ai_socktype, q->ai_protocol);
        if (sock_down == -1) continue; /* try next field */
        else break; /* success, get out of loop */
    }   

    if (connect(sock_down, q->ai_addr, q->ai_addrlen) != 0) 
        Syslog(LOG_ERR, "ERROR: [down] connect returned %s", strerror(errno));

   if (setsockopt(sock_down, SOL_SOCKET, SO_RCVTIMEO, (void *)&pull_timeout, sizeof pull_timeout) != 0)
       Syslog(LOG_ERR, "ERROR: [down] setsockopt returned %s", strerror(errno));

	/* pre-fill the pull request buffer with fixed fields */
	buff_req[0] = PROTOCOL_VERSION;
	buff_req[3] = PKT_PULL_DATA;

    *(uint32_t *)(buff_req + 4) = net_mac_h; 
    *(uint32_t *)(buff_req + 8) = net_mac_l; 

	/* generate random token for request */
    token_h = (uint8_t)rand(); /* random token */
	token_l = (uint8_t)rand(); /* random token */
	buff_req[1] = token_h;
	buff_req[2] = token_l;

    /* for debug */
    FILE *fp;
    if ((fp = fopen("/var/iot/pull_data", "w+")) > 0) {
        fwrite(buff_req, 1, 12, fp);
        fclose(fp);
    }

	/* send PULL request and record time */
	if (send(sock_down, (void *)buff_req, sizeof(buff_req), 0) == -1) {
        Syslog(LOG_ERR, "[down]send Pull request error");
        die("ERRORS");
    }

	clock_gettime(CLOCK_MONOTONIC, &send_time);

	/* listen to packets and process them until a new PULL request must be sent */
	recv_time = send_time;
	while ((int)difftimespec(recv_time, send_time) < keepalive_time) {
		/* try to receive a datagram */
		msg_len = recv(sock_down, (void *)buff_down, (sizeof buff_down) - 1, 0);
		clock_gettime(CLOCK_MONOTONIC, &recv_time);

		/* if no network message was received, got back to listening sock_down socket */
		if (msg_len == -1) {
			//Syslog(LOG_WARNING, "[down]recv returned %s", strerror(errno)); /* too verbose */
			continue;
		}

		/* if the datagram is an ACK, check token */
		if (buff_down[3] == PKT_PULL_ACK) {
			if ((buff_down[1] == token_h) && (buff_down[2] == token_l)) {
				if (req_ack) {
					Syslog(LOG_INFO, "INFO: [down] duplicate ACK received :)");
				} else { /* if that packet was not already acknowledged */
					Syslog(LOG_INFO, "INFO: [down] PULL_ACK received in %i ms", (int)(1000 * difftimespec(recv_time, send_time)));
				}
			} else { /* out-of-sync token */
				Syslog(LOG_INFO, "INFO: [down] received out-of-sync ACK");
			}
			continue;
		}

		/* the datagram is a PULL_RESP */
		buff_down[msg_len] = 0; /* add string terminator, just to be safe */
		Syslog(LOG_INFO, "INFO: [down] PULL_RESP received :)"); /* very verbose */
		// printf("\nJSON down: %s\n", (char *)(buff_down + 4)); /* DEBUG: display JSON payload */
        parser_txpk((char *)(buff_down + 4), down_size, "size");
        parser_txpk((char *)(buff_down + 4), down_data, "data");

		Syslog(LOG_INFO, "INFO: [down] PULL_RESP parser data: %s", down_data); 

		i = b64_to_bin(down_data, strlen(down_data), payload, sizeof(payload));

        if (i != atoi(down_size))
            Syslog(LOG_INFO, "WARNING: [down] mismatch between .size and .data size once converter to binary");
        else {
            FILE *fp;
            if ((fp = fopen(DLPATH, "w+")) > 0) {
                if (fwrite(payload, 1, i, fp) < i) 
                    Syslog(LOG_INFO, "downlink message fail to write");
                fclose(fp);
            } else
                Syslog(LOG_INFO, "fail to open %s", DLPATH);
        }
    } /* end while */

}

void sendstat()
{
    static char status_report[STATUS_SIZE]; /* status report as a JSON object */
    char stat_timestamp[24];
    time_t t;

    int stat_index=0;

    int dec, i, j;

    /* pre-fill the data buffer with fixed fields */
    status_report[0] = PROTOCOL_VERSION;

    /* start composing datagram with the header */
    uint8_t token_h = (uint8_t)rand(); /* random token */
    uint8_t token_l = (uint8_t)rand(); /* random token */
    status_report[1] = token_h;
    status_report[2] = token_l;

    status_report[3] = PKT_PUSH_DATA;

    /* fill GEUI  8bytes */
    *(uint32_t *)(status_report + 4) = net_mac_h; 
    *(uint32_t *)(status_report + 8) = net_mac_l; 

    /*
    for (i = 0, j = 0; i < 16; i = i + 2, j++) {
        dec = 0;
        dec = todec(gatewayid[i]) * 16 + todec(gatewayid[i + 1]);
        status_report[4 + j] = (unsigned char)dec;
    }
    */

    stat_index = 12; /* 12-byte header */

    /* get timestamp for statistics */
    t = time(NULL);
    strftime(stat_timestamp, sizeof stat_timestamp, "%F %T %Z", gmtime(&t));

    j = snprintf((char *)(status_report + stat_index), STATUS_SIZE-stat_index, "{\"stat\":{\"time\":\"%s\",\"lati\":%.5f,\"long\":%.5f,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}", stat_timestamp, lat, lon, (int)alt, 0, 0, 0, (float)0, 0, 0, platform, email, description);
    stat_index += j;
    status_report[stat_index] = 0; /* add string terminator, for safety */

    Syslog(LOG_INFO, "stat update: %s", (char *)(status_report + 12)); /* DEBUG: display JSON stat */

    //send the update
    sendudp(status_report, stat_index, token_h, token_l);

}

void sendrxpk(int rssi, int size) {
    static char rxpk[TX_BUFF_SIZE]; /* status report as a JSON object */
    char data[TX_BUFF_SIZE - 64];

    int rxpk_index=0;
    int fd;
    int dec, i, j;

    /* pre-fill the data buffer with fixed fields */
    rxpk[0] = PROTOCOL_VERSION;

    /* start composing datagram with the header */
    uint8_t token_h = (uint8_t)rand(); /* random token */
    uint8_t token_l = (uint8_t)rand(); /* random token */
    rxpk[1] = token_h;
    rxpk[2] = token_l;

    rxpk[3] = PKT_PUSH_DATA;

    *(uint32_t *)(rxpk + 4) = net_mac_h; 
    *(uint32_t *)(rxpk + 8) = net_mac_l; 

    rxpk_index = 12; /* 12-byte header */

    /* get timestamp for statistics */
    struct timeval now;
    gettimeofday(&now, NULL);
    uint32_t tmst = (uint32_t)(now.tv_sec*1000000 + now.tv_usec);

    if ((fd = open(DATAFILE, O_CREAT|O_RDWR)) < 0 ){
        Syslog(LOG_ERR, "can't open data file!");
        die("open data file");
    } else {
        if ((i = read(fd, data, TX_BUFF_SIZE - 64)) < 0){
            Syslog(LOG_ERR, "can't read data file!");
            die("read data file");
        }

        /*
        size = bin_to_b64((uint8_t *)data, i, (char *)(b64), 341);
        */
    }

    if (close(fd) != 0)
        Syslog(LOG_ERR, "can't close data file!");

    j = snprintf((char *)(rxpk + rxpk_index), TX_BUFF_SIZE - rxpk_index, "{\"rxpk\":[{\"tmst\":%u,\"chan\": 0,\"rfch\": 0,\"freq\":%u,\"stat\":1,\"modu\":\"LORA\",\"datr\":\"SF%sBW125\",\"codr\":\"4/%s\",\"lsnr\":9", tmst, freq, sf, coderate);

    rxpk_index += j;

    j = snprintf((char *)(rxpk + rxpk_index), TX_BUFF_SIZE - rxpk_index, ",\"rssi\":%d,\"size\":%u", rssi, size);

    rxpk_index += j;

    memcpy((void *)(rxpk + rxpk_index), (void *)",\"data\":\"", 9);
    rxpk_index += 9;

    j = bin_to_b64((uint8_t *)data, i, rxpk + rxpk_index, 341);

    rxpk_index += j;
    rxpk[rxpk_index] = '"';
    ++rxpk_index;
    rxpk[rxpk_index] = '}';
    ++rxpk_index;
    rxpk[rxpk_index] = ']';
    ++rxpk_index;
    rxpk[rxpk_index] = '}';
    ++rxpk_index;
    rxpk[rxpk_index] = '\0'; /* add string terminator, for safety */

    Syslog(LOG_NOTICE, "rxpk: %s", (char *)(rxpk + 12));
    sendudp(rxpk, rxpk_index, token_h, token_l);

    uint8_t otaa;
    otaa = data[0] & 0X07;
    if (otaa == 0x00)   /* MHDR for otaa requrie is 000 */
        pull_data();
}

void debug_uci_value() {
    printf("server: %s\n", server);
    printf("email: %s\n", email);
    printf("port: %s\n", port);
    printf("gatewayid: %s\n", gatewayid);
    printf("lat: %f\n", lat);
    printf("lon: %f\n", lon);
    printf("sf: %s\n", sf);
    printf("bw: %s\n", bw);
    printf("code: %s\n", coderate);
}

int main (int argc, char *argv[]) {

    /*
    struct sigaction sa;
    */

    struct addrinfo hints; 

    int n;

    uint64_t lgwm = 0; /* Lora gateway MAC address */
    unsigned long long ull = 0;

    openlog("lora_udp_fwd", LOG_CONS | LOG_PID, 0);

    if (argc < 2){
        Syslog(LOG_ERR, "%s: need argument", argv[0]);
        closelog();
        exit(1);
    }

    /*
    sa.sa_handler = sigalrm;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) < 0)
        Syslog(LOG_INFO, "%s: install SIGALRM error", argv[0]);
    */

    if (!get_option_value("general", server)){
        Syslog(LOG_INFO, "%s: get option server=%s", argv[0], server);
        strcpy(server, "52.169.76.203");  /*set default:router.eu.thethings.network*/
    }

    if (!get_option_value("general", port)){
        Syslog(LOG_ERR, "%s: get option port=%s", argv[0], port);
        strcpy(port, "1700");
    }

    if (!get_option_value("general", email)){
        Syslog(LOG_NOTICE, "%s: get option email=%s", argv[0], email);
    }

    if (!get_option_value("general", gatewayid)){
        Syslog(LOG_NOTICE, "%s: get option gatewayid=%s", argv[0], gatewayid);
    } 

    if (!get_option_value("general", LAT)){
        Syslog(LOG_NOTICE, "%s: get option lat=%s", argv[0], LAT);
    }

    if (!get_option_value("general", LON)){
        Syslog(LOG_NOTICE, "%s: get option lon=%s", argv[0], LON);
    }

    if (!get_option_value("general", pfwd_debug)){
        Syslog(LOG_NOTICE, "get option pfwd_debug=%s", pfwd_debug);
    }

    if (!get_option_value("radio", sf)){
        Syslog(LOG_NOTICE, "%s: get option sf=%s", argv[0], sf);
    }

    if (!get_option_value("radio", coderate)){
        Syslog(LOG_NOTICE, "%s: get option coderate=%s", argv[0], coderate);
    }

    if (!get_option_value("radio", bw)){
        Syslog(LOG_NOTICE, "%s: get option bw=%s", argv[0], bw);
    }

    if (!get_option_value("radio", frequency)){
        Syslog(LOG_NOTICE, "%s: get option frequency=%s", argv[0], frequency);
        strcpy(frequency, "868100000"); /* default frequency*/
    }

    lat = atof(LAT);
    lon = atof(LON);
    freq = atof(frequency);

    /* uci value print 
    debug_uci_value();
    */

    /*
    if ((sock_up = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        Syslog(LOG_ERR, "%s: socket error", argv[0]);
        die("socket");
    }
    */

    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    /*
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "eth1", IFNAMSIZ-1);  
    ioctl(sock_up, SIOCGIFHWADDR, &ifr);
    */

    if ((n = getaddrinfo(server, port, &hints, &res)) != 0){
        Syslog(LOG_ERR, "failed to open socket to any of %s:%s, %s", server, port, gai_strerror(n));
        die("getaddrinfo");
    }

    
    if (strlen(gatewayid) != 16) {  /*make sure gatewayid len equal 16 bytes */
        Syslog(LOG_ERR, "[GEUI] GatewayID: %s ERR", gatewayid);
        die("GEUI error");
    }

    sscanf(gatewayid, "%llx", &ull);
    lgwm = ull;

    net_mac_h = htonl((uint32_t)(0xFFFFFFFF & (lgwm>>32)));
    net_mac_l = htonl((uint32_t)(0xFFFFFFFF &  lgwm  ));

    syslog(LOG_NOTICE, "Listening at SF%sBW125 on %.6lf Mhz.\n", sf, (double)freq/1000000);

    if(!strcmp("stat", argv[1]))  /* send gateway status to UDP server */ 
        sendstat();
    else
        if (argc < 4) 
            printf("Usage: %s rxpk rssi size\n", argv[0]);
        else
            sendrxpk(atoi(argv[2]), atoi(argv[3]));

    closelog();
    
    freeaddrinfo(res);

    return (0);

}

