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

#include <errno.h>		/* error messages */

#include "loragw_hal.h"
#include "radio.h"

extern char *optarg;
extern int optind, opterr, optopt;

/* lora configuration variables */
static char sf[8] = "7";
static char bw[8] = "125000";
static char cr[8] = "1";
static char wd[8] = "52";
static char prlen[8] = "8";
static char freq[16] = "868500000";            /* frequency of radio */

static int invertiq = 0;

/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

void print_help(void) {
    printf("Usage: single_tx           [-f frequence] <uint> (default:868500000) target frequency in Hz\n");
    printf("                           [-s spreadingFactor] <uint> (default: 7)\n");
    printf("                           [-b bandwidth] <uint> default: 125k \n");
    printf("                           [-c coderate] <uint> LoRa Coding Rate [1-4] \n");
    printf("                           [-w syncword] <uint> default: 52, 0x34\n");
    printf("                           [-i] send packet using inverted modulation polarity\n");
    printf("                           [-p int ]  RF power (dBm) [ 0dBm 10dBm 14dBm 20dBm 27dBm ]\n");
    printf("                           [-m message] <text> send this message from radio\n");
    printf("                           [-h] show this help and exit \n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char *argv[])
{

    int c;

    char message[128] = {'\0'};

    // Make sure only one copy of the daemon is running.
    //if (already_running()) {
      //  printf_DEBUG(DEBUG_ERROR, "%s: already running!\n", argv[0]);
      //  exit(1);
    //}

    while ((c = getopt(argc, argv, "f:s:b:c:w:ip:m:h")) != -1) {
        switch (c) {
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
            case 'c':
                if (optarg)
                    strncpy(cr, optarg, sizeof(cr));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'i':
                invertiq = 1;
                break;
            case 'w':
                if (optarg)
                    strncpy(wd, optarg, sizeof(wd));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'm':
                if (optarg)
                    strncpy(message, optarg, sizeof(message));
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

	
	
	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* radio device SPI_DEV init */
    radiodev *loradev;
    loradev = (radiodev *) malloc(sizeof(radiodev));

    loradev->nss = 21;
    loradev->rst = 12;
    loradev->dio[0] = 7;
    loradev->dio[1] = 6;
    loradev->dio[2] = 8;	

    loradev->spiport = spi_open(SPI_DEV_RADIO);

    loradev->freq = atol(freq);
    loradev->sf = atoi(sf);
    loradev->bw = atol(bw);
    loradev->cr = atoi(cr);
    loradev->syncword = atoi(wd);
    loradev->nocrc = 1;  /* crc check */
    loradev->prlen = atoi(prlen);
    loradev->invertio = invertiq;
    strcpy(loradev->desc, "RFDEV");	

    printf("Radio struct: spiport=%d, freq=%d, sf=%d, bw=%d, cr=%d, wd=0x%2x\n", loradev->spiport, loradev->freq, loradev->sf, loradev->bw, loradev->cr, loradev->syncword);

    if(!get_radio_version(loradev))  
        goto clean;

    setup_channel(loradev);

    char payload[256] = {'\0'};

    if (strlen(message) < 1)
        strcpy(message, "HELLO DRAGINO");	

    snprintf(payload, sizeof(payload), "%s", message);

    single_tx(loradev, (uint8_t *)payload, strlen(payload));

clean:

    free(loradev);
	
    printf("INFO: Exiting %s\n", argv[0]);
    exit(EXIT_SUCCESS);
}

