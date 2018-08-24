/**
 * Author: Dragino 
 * Date: 16/01/2018
 * 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.dragino.com
 *
 * 
*/

#include <signal.h>		/* sigaction */
#include <errno.h>		/* error messages */

#include "radio.h"

#define TX_MODE 0
#define RX_MODE 1

#define RADIO1    "/dev/spidev1.0"
#define RADIO2    "/dev/spidev2.0"

static char ver[8] = "0.1";

/* lora configuration variables */
static char sf[8] = "7";
static char bw[8] = "125000";
static char cr[8] = "5";
static char prlen[8] = "8";
static char freq[16] = "868500000";            /* frequency of radio */
static char radio[16] = RADIO1;
static char filepath[32] = {'\0'};
static int mode = TX_MODE;
static int device = 49;
static bool getversion = false;

/* signal handling variables */
volatile bool exit_sig = false; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
volatile bool quit_sig = false; /* 1 -> application terminates without shutting down the hardware */

/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
	    quit_sig = true;;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
	    exit_sig = true;
    }
}

static void wait_ms(unsigned long a) {
    struct timespec dly;
    struct timespec rem;

    dly.tv_sec = a / 1000;
    dly.tv_nsec = ((long)a % 1000) * 1000000;

    //MSG("NOTE dly: %ld sec %ld ns\n", dly.tv_sec, dly.tv_nsec);

    if((dly.tv_sec > 0) || ((dly.tv_sec == 0) && (dly.tv_nsec > 100000))) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &dly, &rem);
        //MSG("NOTE remain: %ld sec %ld ns\n", rem.tv_sec, rem.tv_nsec);
    }
    return;
}

void print_help(void) {
    printf("Usage: lg02_single_rx_tx   [-d radio_dev] select radio 1 or 2 (default:1) \n");
    printf("                           [-t] set as tx\n");
    printf("                           [-r] set as rx\n");
    printf("                           [-f frequence] (default:868500000)\n");
    printf("                           [-s spreadingFactor] (default: 7)\n");
    printf("                           [-b bandwidth] default: 125k \n");
    printf("                           [-p payload ]  \n");
    printf("                           [-o filepath ]  \n");
    printf("                           [-v] show version \n");
    printf("                           [-h] show this help and exit \n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char *argv[])
{

    struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
	
    int c, i;

    char input[128] = {'\0'};

    FILE *fp = NULL;

    // Make sure only one copy of the daemon is running.
    //if (already_running()) {
      //  MSG_DEBUG(DEBUG_ERROR, "%s: already running!\n", argv[0]);
      //  exit(1);
    //}

    while ((c = getopt(argc, argv, "trd:m:f:s:b:p:o:vh")) != -1) {
        switch (c) {
            case 'v':
                getversion = true;
                break;
            case 'd':
		if (optarg)
			device = optarg[0];
		else {
			print_help();
			exit(1);
		}
                break;
            case 't':
                mode = TX_MODE;
                break;
            case 'r':
                mode = RX_MODE;
                break;
            case 'f':
                if (optarg)
                    strncpy(freq, optarg, sizeof(freq));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 's':
                if (optarg)
                    strncpy(sf, optarg, sizeof(sf));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'b':
                if (optarg)
                    strncpy(bw, optarg, sizeof(bw));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'p':
                if (optarg)
                    strncpy(input, optarg, sizeof(input));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'o':
                if (optarg)
                    strncpy(filepath, optarg, sizeof(filepath));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'h':
            case '?':
            default:
                print_help();
                exit(0);
        }
    }

	
    if (getversion) {
		printf("lg02_single_rx_tx ver: %s\n",ver);
        exit(0);
    }	

	
	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* radio device SPI_DEV init */
    radiodev *loradev;
    loradev = (radiodev *) malloc(sizeof(radiodev));

    if (device == 49){
	loradev->nss = 15;
	loradev->rst = 8;
	loradev->dio[0] = 7;
	loradev->dio[1] = 6;
	loradev->dio[2] = 0;	
	strncpy(radio, RADIO1, sizeof(radio));
    }
    else if ( device == 50){
	loradev->nss = 24;
	loradev->rst = 23;
	loradev->dio[0] = 22;
	loradev->dio[1] = 20;
	loradev->dio[2] = 0;
	strncpy(radio, RADIO2, sizeof(radio));	
    }

    loradev->spiport = lgw_spi_open(radio);

    if (loradev->spiport < 0) { 
        printf("opening %s error!\n",radio);
        goto clean;
    }

    loradev->freq = atol(freq);
    loradev->sf = atoi(sf);
    loradev->bw = atol(bw);
    loradev->cr = atoi(cr);
    loradev->nocrc = 1;  /* crc check */
    loradev->prlen = atoi(prlen);
    loradev->invertio = 0;
    strcpy(loradev->desc, "RFDEV");	

    MSG("Radio struct: spi_dev=%s, spiport=%d, freq=%ld, sf=%d, bw=%ld, cr=%d\n", radio, loradev->spiport, loradev->freq, loradev->sf, loradev->bw, loradev->cr );

    if(!get_radio_version(loradev))  
        goto clean;

    /* configure signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL); /* Ctrl-\ */
    sigaction(SIGINT, &sigact, NULL); /* Ctrl-C */
    sigaction(SIGTERM, &sigact, NULL); /* default "kill" command */

    setup_channel(loradev);

    if ( mode == TX_MODE ){
	uint8_t payload[256] = {'\0'};

	if (strlen(input) < 1)
	    strcpy(input, "HELLO DRAGINO");	

	snprintf(payload, sizeof(payload), "%s", input);
	single_tx(loradev, payload, strlen((char *)payload));
    } else if ( mode == RX_MODE){

        struct pkt_rx_s rxpkt;

        rxlora(loradev->spiport, RXMODE_SCAN);

        if (strlen(filepath) > 0) 
            fp = fopen(filepath, "w+");

        MSG("\nListening at SF%i on %.6lf Mhz. port%i\n", loradev->sf, (double)(loradev->freq)/1000000, loradev->spiport);
        while (!exit_sig && !quit_sig) {
            if(digitalRead(loradev->dio[0]) == 1) {
                memset(rxpkt.payload, 0, sizeof(rxpkt.payload));
                received(loradev->spiport, &rxpkt);
                if (fp) {
                    //fwrite(rxpkt.payload, 1, strlen(rxpkt.payload), fp);
                    fprintf(fp, "%s\n", rxpkt.payload);
                    fflush(fp);
                }
                fprintf(stderr, "echo received: %s\n", rxpkt.payload);
            }
        }

    }

clean:
    if(fp)
        fclose(fp);

    free(loradev);
	
    MSG("INFO: Exiting %s\n", argv[0]);
    exit(EXIT_SUCCESS);
}

