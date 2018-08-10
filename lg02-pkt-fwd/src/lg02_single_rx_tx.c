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

/* lora configuration variables */
static char rxsf[8] = "7";
static char txsf[8] = "7";
static char rxbw[8] = "125000";
static char txbw[8] = "125000";
static char rxcr[8] = "5";
static char txcr[8] = "5";
static char rxprlen[8] = "8";
static char txprlen[8] = "8";
static char rx_freq[16] = "868500000";            /* rx frequency of radio */
static char tx_freq[16] = "868500000";            /* tx frequency of radio */
static bool radioB = false;
static bool getversion = false;
static bool lg02 = false;

/* signal handling variables */
volatile bool exit_sig = false; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
volatile bool quit_sig = false; /* 1 -> application terminates without shutting down the hardware */

/* radio devices */

radiodev *rxdev;
radiodev *txdev;

void thread_rec(void);

/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
	    quit_sig = true;;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
	    exit_sig = true;
    }
    return;
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
    printf("Usage: lg02_single_rx_tx   [-r radiodevice] \n");
    printf("                           [-f frequence] (default:868500000)\n");
    printf("                           [-s spreadingFactor] (default: 7)\n");
    printf("                           [-b bandwidth] default: 125k \n");
    printf("                           [-F frequence] (default:868500000)\n");
    printf("                           [-S spreadingFactor] (default: 7)\n");
    printf("                           [-B bandwidth] default: 125k \n");
    printf("                           [-p payload]   \n");
    printf("                           [-h] show this help and exit \n");
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main(int argc, char *argv[])
{
    /* threads */
    pthread_t thrid_rec;

    int c, i;

    char input[128] = {'\0'};

    // Make sure only one copy of the daemon is running.
    if (already_running()) {
        MSG_DEBUG(DEBUG_ERROR, "%s: already running!\n", argv[0]);
        exit(1);
    }

    while ((c = getopt(argc, argv, "trf:s:b:F:S:B:ph")) != -1) {
        switch (c) {
            case 't':
                getversion = true;
                break;
            case 'r':
                radioB = true;
                break;
            case 'f':
                if (optarg)
                    strncpy(rx_freq, optarg, sizeof(rx_freq));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 's':
                if (optarg)
                    strncpy(rxsf, optarg, sizeof(rxsf));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'b':
                if (optarg)
                    strncpy(rxbw, optarg, sizeof(rxbw));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'F':
                if (optarg)
                    strncpy(tx_freq, optarg, sizeof(tx_freq));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'S':
                if (optarg)
                    strncpy(txsf, optarg, sizeof(txsf));
                else {
                    print_help();
                    exit(1);
                }
                break;
            case 'B':
                if (optarg)
                    strncpy(txbw, optarg, sizeof(txbw));
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
            case 'h':
            case '?':
            default:
                print_help();
                exit(0);
        }
    }

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* radio device init */

    rxdev = (radiodev *) malloc(sizeof(radiodev));

    txdev = (radiodev *) malloc(sizeof(radiodev));
    
    rxdev->nss = 15;
    rxdev->rst = 8;
    rxdev->dio[0] = 7;
    rxdev->dio[1] = 6;
    rxdev->dio[2] = 0;
    rxdev->spiport = lgw_spi_open(SPI_DEV_RX);
    if (rxdev->spiport < 0) { 
        printf("open spi_dev_tx error!\n");
        goto clean;
    }
    rxdev->freq = atol(rx_freq);
    rxdev->sf = atoi(rxsf);
    rxdev->bw = atol(rxbw);
    rxdev->cr = atoi(rxcr);
    rxdev->nocrc = 1;  /* crc check */
    rxdev->prlen = atoi(rxprlen);
    rxdev->invertio = 0;
    strcpy(rxdev->desc, "RXRF");

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    txdev->nss = 24;
    txdev->rst = 23;
    txdev->dio[0] = 22;
    txdev->dio[1] = 20;
    txdev->dio[2] = 0;
    txdev->spiport = lgw_spi_open(SPI_DEV_TX);
    if (txdev->spiport < 0) {
        MSG("open spi_dev_tx error!\n");
        goto clean;
    }
    txdev->freq = atol(tx_freq);
    txdev->sf = atoi(txsf);
    txdev->bw = atol(txbw);
    txdev->cr = atoi(txcr);
    txdev->nocrc = 1;
    txdev->prlen = atoi(txprlen);
    txdev->invertio = 0;
    strcpy(txdev->desc, "TXRF");

    MSG("RadioA struct: spiport=%d, freq=%ld, sf=%d\n", rxdev->spiport, rxdev->freq, rxdev->sf);
    MSG("RadioB struct: spiport=%d, freq=%ld, sf=%d\n", txdev->spiport, txdev->freq, txdev->sf);

    if(!get_radio_version(rxdev))  
        goto clean;

    if (getversion) {
        printf("RadioA 1276 detected\n");
        if(get_radio_version(txdev))  
            printf("RadioB 1276 detected\n");
        goto clean;
    }

    if (strlen(input) < 1)
        strcpy(input, "HELLO");

    /* spawn threads to manage radio receive queue*/
    MSG("Spawn threads to manage fifo payload...\n");
    i = pthread_create( &thrid_rec, NULL, (void * (*)(void *))thread_rec, NULL);
    if (i != 0) {
	    MSG("ERROR: [main] impossible to create receive thread\n");
	    exit(EXIT_FAILURE);
    }

    int count = 0;
    radiodev *sdev;

    if (!radioB) {
        sdev = txdev;
    } else {
        sdev = rxdev;
    }

    uint8_t payload[256] = {'\0'};

    setup_channel(sdev);

    while (!exit_sig && !quit_sig) {
        snprintf(payload, sizeof(payload), "%s%d", input, count++);
        single_tx(sdev, payload, strlen((char *)payload));
	wait_ms(1000 * 10); // 10 seconds
    }

clean:
    free(rxdev);
    free(txdev);
	
    MSG("INFO: Exiting %s\n", argv[0]);
    exit(EXIT_SUCCESS);
}


void thread_rec(void) {

    struct pkt_rx_s rxpkt;

    radiodev *dev;

    if (!radioB) {
        dev = rxdev;
    } else {
        dev = txdev;
    }

    setup_channel(dev);

    rxlora(dev->spiport, RXMODE_SCAN);

    MSG("\nListening at SF%i on %.6lf Mhz. port%i\n", dev->sf, (double)(dev->freq)/1000000, dev->spiport);
    while (!exit_sig && !quit_sig) {
        if(digitalRead(dev->dio[0]) == 1) {
            memset(rxpkt.payload, 0, sizeof(rxpkt.payload));
            received(dev->spiport, &rxpkt);
            }
    }
}
