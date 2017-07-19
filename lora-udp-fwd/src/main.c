#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
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


typedef bool boolean;

char b64[256];

struct sockaddr_in si_other;
int s, slen=sizeof(si_other);
struct ifreq ifr;

/* Set center frequency */
uint32_t  freq = 868100000; /* in Mhz! (868.1) */

/* Set location */
float lat=0.0;
float lon=0.0;
int   alt=0;

#define CHARLEN 64  /* Length of uci option value */  
/* Informal status fields */
static char platform[CHARLEN] = "LG01/OLG01";  /* platform definition */
char description[CHARLEN] = "";                        /* used for free form description */
char server[CHARLEN] = "server";
char email[CHARLEN]  = "mail";                        /* used for contact email */
char LAT[CHARLEN] = "lati";
char LON[CHARLEN] = "long";
char gatewayid[CHARLEN] = "gateway_id";
char port[CHARLEN] = "port";
char sf[CHARLEN] = "SF";
char bw[CHARLEN] = "BW";
char coderate[CHARLEN] = "coderate";

#define BUFLEN 2048  //Max length of buffer

#define TX_BUFF_SIZE  2048
#define STATUS_SIZE	  1024

#define TIMEOUT 10

#define DATAFILE "/var/iot/data"

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
        struct uci_section *s = uci_to_section(e);

        if(!strcmp(section, s->e.name))  /* compare section name */ {
            if (NULL != (value = uci_lookup_option_string(ctx, s, option))) {
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


/* signal for hangup receive message */
void sigalrm(int signo)
{
}

void die(const char *s)
{
    perror(s);
    closelog();
    exit(1);
}

boolean receivePkt(char *payload)
{
    return true;
}

/* convert dex to dec */
int todec(char ch)
{
    if(ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    if(ch >= 'A' && ch <='F') 
    {
        return ch - 'A' + 10;
    }
    if(ch >= 'a' && ch <= 'f')
    {
        return ch - 'a' + 10;
    }
    return -1;
}


/* sendto udp server */
void sendudp(char *msg, int length) {

    char buf[BUFLEN];
    /*send the update*/
    int n;

    inet_aton(server , &si_other.sin_addr);

    if (sendto(s, (char *)msg, length, 0 , (struct sockaddr *) &si_other, slen)==-1)
    {
        syslog(LOG_ERR, "lora: sendudp error");
        die("sendto()");
    }

    alarm(TIMEOUT);

    if ((n = recvfrom(s, buf, BUFLEN, 0, NULL, NULL)) < 0) {
        if (errno != EINTR)
            alarm(0);
    } else
        syslog(LOG_INFO, "receive from udp server %s", buf);

    alarm(0);

}

void sendstat(int mac) {

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

    if (!mac) {
        for (i = 0, j = 0; i < 16; i = i + 2, j++) {
            dec = 0;
            dec = todec(gatewayid[i]) * 16 + todec(gatewayid[i + 1]);
            /*
            printf("id[%d][%d] = %c%c TO dec: %d\n",  i, i+1, (char)gatewayid[i], (char)gatewayid[i+1], dec); 
            */
            status_report[4 + j] = (unsigned char)dec;
        }
    } else {
        status_report[4] = (unsigned char)ifr.ifr_hwaddr.sa_data[0];
        status_report[5] = (unsigned char)ifr.ifr_hwaddr.sa_data[1];
        status_report[6] = (unsigned char)ifr.ifr_hwaddr.sa_data[2];
        status_report[7] = 0xFF;
        status_report[8] = 0xFF;
        status_report[9] = (unsigned char)ifr.ifr_hwaddr.sa_data[3];
        status_report[10] = (unsigned char)ifr.ifr_hwaddr.sa_data[4];
        status_report[11] = (unsigned char)ifr.ifr_hwaddr.sa_data[5];
    }

    stat_index = 12; /* 12-byte header */

    /* get timestamp for statistics */
    t = time(NULL);
    strftime(stat_timestamp, sizeof stat_timestamp, "%F %T %Z", gmtime(&t));

    j = snprintf((char *)(status_report + stat_index), STATUS_SIZE-stat_index, "{\"stat\":{\"time\":\"%s\",\"lati\":%.5f,\"long\":%.5f,\"alti\":%i,\"rxnb\":%u,\"rxok\":%u,\"rxfw\":%u,\"ackr\":%.1f,\"dwnb\":%u,\"txnb\":%u,\"pfrm\":\"%s\",\"mail\":\"%s\",\"desc\":\"%s\"}}", stat_timestamp, lat, lon, (int)alt, 0, 0, 0, (float)0, 0, 0, platform, email, description);
    stat_index += j;
    status_report[stat_index] = 0; /* add string terminator, for safety */

    syslog(LOG_INFO, "stat update: %s\n", (char *)(status_report + 12)); /* DEBUG: display JSON stat */

    //send the update
    sendudp(status_report, stat_index);

}

void sendrxpk(int mac) {
    static char rxpk[TX_BUFF_SIZE]; /* status report as a JSON object */
    char stat_timestamp[24];
    char data[TX_BUFF_SIZE - 64];
    time_t t;

    int rxpk_index=0;
    int fd;
    int rssi, size;
    int dec, i, j;

    /* pre-fill the data buffer with fixed fields */
    rxpk[0] = PROTOCOL_VERSION;
    /* start composing datagram with the header */
    uint8_t token_h = (uint8_t)rand(); /* random token */
    uint8_t token_l = (uint8_t)rand(); /* random token */
    rxpk[1] = token_h;
    rxpk[2] = token_l;

    rxpk[3] = PKT_PUSH_DATA;

    if (!mac) {
        for (i = 0, j = 0; i < 16; i = i + 2, j++) {
            dec = 0;
            dec = todec(gatewayid[i]) * 16 + todec(gatewayid[i + 1]);
            rxpk[4 + j] = (unsigned char)dec;
        }
    } else {
        rxpk[4] = (unsigned char)ifr.ifr_hwaddr.sa_data[0];
        rxpk[5] = (unsigned char)ifr.ifr_hwaddr.sa_data[1];
        rxpk[6] = (unsigned char)ifr.ifr_hwaddr.sa_data[2];
        rxpk[7] = 0xFF;
        rxpk[8] = 0xFF;
        rxpk[9] = (unsigned char)ifr.ifr_hwaddr.sa_data[3];
        rxpk[10] = (unsigned char)ifr.ifr_hwaddr.sa_data[4];
        rxpk[11] = (unsigned char)ifr.ifr_hwaddr.sa_data[5];
    }

    rxpk_index = 12; /* 12-byte header */

    /* get timestamp for statistics */
    t = time(NULL);
    strftime(stat_timestamp, sizeof stat_timestamp, "%F %T %Z", gmtime(&t));

    if ((fd = open(DATAFILE, O_RDONLY)) < 0 ){
        syslog(LOG_ERR, "can't open data file!");
        die("open data file");
    } else {
        if (read(fd, data, TX_BUFF_SIZE - 64) != 0){
            syslog(LOG_ERR, "can't read data file!");
            die("read data file");
        }
        size = bin_to_b64((uint8_t *)data, strlen(data), (char *)(b64), 341);
    }

    if (close(fd) != 0)
        syslog(LOG_ERR, "can't close data file!");

    j = snprintf((char *)(rxpk + rxpk_index), TX_BUFF_SIZE-rxpk_index, "{\"rxpk\":[{\"tmst\":\"%s\",\"chan\": 0,\"rfch\": 0,\"freq\":%.6lf,\"stat\":1,\"modu\":\"LORA\",\"datr\":\"%s %s\",\"codr\":\"%s\",\"lsnr\":9,\"rssi\":%d,\"size\":%d,\"data\":\"%s\"}]}", stat_timestamp, freq, sf, bw, coderate, rssi, size, b64);
    rxpk_index += j;
    rxpk[rxpk_index] = 0; /* add string terminator, for safety */

    sendudp(rxpk, rxpk_index);
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
}

int main (int argc, char *argv[]) {

    struct timeval nowtime;
    uint32_t lasttime;

    struct sigaction sa;

    int PORT = 1700;

    int mac = 0; /*default gatewayid form uci option value */

    openlog("lora_udp_fwd", LOG_CONS | LOG_PID, 0);

    if (argc < 2){
        syslog(LOG_ERR, "%s: need argument", argv[0]);
        closelog();
        exit(1);
    }

    sa.sa_handler = sigalrm;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) < 0)
        syslog(LOG_ERR, "%s: install SIGALRM error", argv[0]);

    if (!get_option_value("general", server)){
        syslog(LOG_ERR, "%s: get option server=%s", argv[0], server);
        die(server);
    }

    if (!get_option_value("general", port)){
        syslog(LOG_ERR, "%s: get option port=%s", argv[0], port);
        die(port);
    }

    if (!get_option_value("general", email)){
        syslog(LOG_NOTICE, "%s: get option email=%s", argv[0], email);
    }

    if (!get_option_value("general", gatewayid)){
        syslog(LOG_NOTICE, "%s: get option gatewayid=%s", argv[0], gatewayid);
    } 

    if (!get_option_value("general", LAT)){
        syslog(LOG_NOTICE, "%s: get option lat=%s", argv[0], LAT);
    }

    if (!get_option_value("general", LON)){
        syslog(LOG_NOTICE, "%s: get option lon=%s", argv[0], LON);
    }

    if (!get_option_value("radio", sf)){
        syslog(LOG_NOTICE, "%s: get option sf=%s", argv[0], sf);
    }

    if (!get_option_value("radio", coderate)){
        syslog(LOG_NOTICE, "%s: get option coderate=%s", argv[0], coderate);
    }

    if (!get_option_value("radio", bw)){
        syslog(LOG_ERR, "%s: get option bw=%s", argv[0], bw);
        die(bw);
    }

    lat = atof(LAT);
    lon = atof(LON);
    PORT = atoi(port);

    /* uci value print 
    debug_uci_value();*/

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        syslog(LOG_ERR, "%s: socket error", argv[0]);
        die("socket");
    }

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "eth1", IFNAMSIZ-1);  /* can we rely on eth0? */
    ioctl(s, SIOCGIFHWADDR, &ifr);

    if (strlen(gatewayid) != 16) {  /*use mac for gatewayid,  gatewayid len is 16 bytes */

        syslog(LOG_NOTICE, "%s-Gateway ID: %.2x:%.2x:%.2x:ff:ff:%.2x:%.2x:%.2x\n",
               argv[0],
               (unsigned char)ifr.ifr_hwaddr.sa_data[0],
               (unsigned char)ifr.ifr_hwaddr.sa_data[1],
               (unsigned char)ifr.ifr_hwaddr.sa_data[2],
               (unsigned char)ifr.ifr_hwaddr.sa_data[3],
               (unsigned char)ifr.ifr_hwaddr.sa_data[4],
               (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
         mac = 1;
    } else {
        syslog(LOG_NOTICE, "%s-Gateway ID: %s", argv[0], gatewayid);
    }

    syslog(LOG_NOTICE, "%s: Listening at %s on %.6lf Mhz.\n", argv[0], sf, (double)freq/1000000);

    //sendstat(mac);
    
    if(!strcmp("stat", argv[1]))  /* send gateway status to UDP server */ 
        sendstat(mac);
    else
        sendrxpk(mac);

   /*
    while(1) {

        gettimeofday(&nowtime, NULL);
        uint32_t nowseconds = (uint32_t)(nowtime.tv_sec);
        if (nowseconds - lasttime >= 30) {
            lasttime = nowseconds;
            sendstat(mac);
        }
        sleep(1);
    }
    */

    closelog();

    return (0);

}

