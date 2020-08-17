/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino_gw_fwd -- An opensource lora gateway forward 
 *
 * See http://www.dragino.com for more information about
 * the lora gateway project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Maintainer: skerlan
 *
 */

/*! \file
 *
 * \brief LG02 radio interface
 *
 */

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf */
#include <string.h>     /* memcpy */
#include <stdlib.h>     
#include <time.h>      

#include <sys/time.h>

#include "gpio.h"
#include "loragw_aux.h"
#include "loragw_spi.h"
#include "loragw_sx1276_regs.h"
#include "loragw_sx1272_regs.h"
#include "loragw_sx127x_radio.h"
#include "loragw_hal_lg02.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_HAL == 1
    #define DEBUG_MSG(str)                fprintf(stderr, str)
    #define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
    #define DEBUG_ARRAY(a,b,c)            for(a=0;a<b;++a) fprintf(stderr,"%x.",c[a]);fprintf(stderr,"end\n")
    #define CHECK_NULL(a)                 if(a==NULL){fprintf(stderr,"%s:%d: ERROR~ NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_HAL_ERROR;}
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
    #define DEBUG_ARRAY(a,b,c)            for(a=0;a!=0;){}
    #define CHECK_NULL(a)                 if(a==NULL){return LGW_HAL_ERROR;}
#endif


/* constant arrays defining hardware capability */
static const uint8_t rxlorairqmask[] = {
    [RXMODE_SINGLE] = IRQ_LORA_RXDONE_MASK|IRQ_LORA_RXTOUT_MASK|IRQ_LORA_CRCERR_MASK,
    [RXMODE_SCAN]   = IRQ_LORA_RXDONE_MASK|IRQ_LORA_CRCERR_MASK,
    [RXMODE_RSSI]   = 0x00,
};

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS & TYPES -------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static int opmode (void* spidev, bool isftdi, uint8_t mode) {
    int ret;
    uint8_t val;

    ret = lg02_reg_r(spidev, isftdi, SX1276_REG_LR_OPMODE, &val);
    ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_OPMODE, (val & ~OPMODE_MASK) | mode);

    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static int loramode(void* spidev, bool isftdi) {
    uint8_t u = OPMODE_LORA | 0x8; // TBD: sx1276 high freq
    return lg02_reg_w(spidev, isftdi, SX1276_REG_LR_OPMODE, u);
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
int lg02_setpower(void* spidev, bool isftdi, uint8_t pw) {
    int ret;
    ret = lg02_reg_w(spidev, isftdi, SX1276_REG_LR_PADAC, 0x87);
    if (pw < 5) pw = 5;
    if (pw > 20) pw = 20;
    ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_PACONFIG, (uint16_t)(0x80 | ((pw - 5) & 0x0f)));

    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_setfreq(void* spidev, bool isftdi, long frequency)
{
    int ret;
    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;

    ret = lg02_reg_w(spidev, isftdi, SX1276_REG_LR_FRFMSB, (uint8_t)(frf >> 16));
    ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_FRFMID, (uint8_t)(frf >> 8));
    ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_FRFLSB, (uint8_t)(frf >> 0));

    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_setsf(void* spidev, bool isftdi, int sf)
{
    int ret;
    uint8_t val;

    if (sf < 6) {
        sf = 6;
    } else if (sf > 12) {
        sf = 12;
    }

    if (sf == 6) {
        ret = lg02_reg_w(spidev, isftdi, SX1276_REG_LR_DETECTOPTIMIZE, 0xc5);
        ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_DETECTIONTHRESHOLD, 0x0c);
    } else {
        ret = lg02_reg_w(spidev, isftdi, SX1276_REG_LR_DETECTOPTIMIZE, 0xc3);
        ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_DETECTIONTHRESHOLD, 0x0a);
    }

    ret |= lg02_reg_r(spidev, isftdi, SX1276_REG_LR_MODEMCONFIG2, &val);
    ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_MODEMCONFIG2, (val & 0x0f) | ((sf << 4) & 0xf0));
    //MSG_LOG(DEBUG_SPI, "SPI", "SF=0x%02x\n", sf, lg02_reg_r(spidev, SX1276_REG_LR_MODEMCONFIG2));
    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_setsbw(void* spidev, bool isftdi, long sbw)
{
    int ret, bw;
    uint8_t val;

    if (sbw <= 7.8E3) {
        bw = 0;
    } else if (sbw <= 10.4E3) {
        bw = 1;
    } else if (sbw <= 15.6E3) {
        bw = 2;
    } else if (sbw <= 20.8E3) {
        bw = 3;
    } else if (sbw <= 31.25E3) {
        bw = 4;
    } else if (sbw <= 41.7E3) {
        bw = 5;
    } else if (sbw <= 62.5E3) {
        bw = 6;
    } else if (sbw <= 125E3) {
        bw = 7;
    } else if (sbw <= 250E3) {
        bw = 8;
    } else /*if (sbw <= 250E3)*/ {
        bw = 9;
    }

    ret = lg02_reg_r(spidev, isftdi, SX1276_REG_LR_MODEMCONFIG1, &val);
    ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_MODEMCONFIG1, (val & 0x0f) | (bw << 4));

    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_setcr(void* spidev, bool isftdi, int denominator)
{
    int ret, cr;
    uint8_t val;

    if (denominator < 5) {
        denominator = 5;
    } else if (denominator > 8) {
        denominator = 8;
    }

    cr = denominator - 4;

    ret = lg02_reg_r(spidev, isftdi, SX1276_REG_LR_MODEMCONFIG1, &val);
    ret |=lg02_reg_w(spidev, isftdi, SX1276_REG_LR_MODEMCONFIG1, (val & 0xf1) | (cr << 1));

    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_setprlen(void* spidev, bool isftdi, long length)
{
    int ret;
    ret = lg02_reg_w(spidev, isftdi, SX1276_REG_LR_PREAMBLEMSB, (uint8_t)(length >> 8));
    ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_PREAMBLELSB, (uint8_t)(length >> 0));
    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_setsyncword(void* spidev, bool isftdi, int sw)
{
    return lg02_reg_w(spidev, isftdi, SX1276_REG_LR_SYNCWORD, sw);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_crccheck(void* spidev, bool isftdi, uint8_t nocrc)
{
    int ret;
    uint8_t val;
    
    ret = lg02_reg_r(spidev, isftdi, SX1276_REG_LR_MODEMCONFIG2, &val);

    if (nocrc)
        ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_MODEMCONFIG2, val & 0xfb);
    else
        ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_MODEMCONFIG2, val | 0x04);

    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
// Lora configure : Freq, SF, BW
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool lg02_detect(sx127x_dev *dev)
{
    uint8_t version;

    digital_write(dev->rst, LOW);
    wait_ms(10);
    digital_write(dev->rst, HIGH);
    wait_ms(10);

    opmode(dev->spiport, dev->isftdi, OPMODE_SLEEP);

    lg02_reg_r(dev->spiport, dev->isftdi, SX1276_REG_LR_VERSION, &version);

    if (version == 0x12) {
        fprintf(stderr, "INFO~ %s: SX1276 detected, starting.\n", dev->desc);
        return true;
    } else if (version == 0x22) {
        fprintf(stderr, "INFO~ %s: SX1272 detected, starting.\n", dev->desc);
    } else {
        fprintf(stderr, "ERROR~ %s: SX127x radio hsa not been found.\n", dev->desc);
        return false;
    }

    return false;

}

int lg02_setup(sx127x_dev *dev, uint8_t modulation)
{
    int ret = 0;
    uint8_t val;

    if (dev->isftdi) {
        if (lgw_ft_spi_open(&dev->spiport) == LGW_SPI_ERROR) {
            DEBUG_MSG("LG02_setup Error: can't open ftdi device.\n");
            return ret;
        }
    } else {
        if (lgw_spi_open(dev->spipath, &dev->spiport) == LGW_SPI_ERROR) {
            DEBUG_PRINTF("LG02_setup Error: can't open spi device %s.\n", dev->spipath);
            return ret;
        }
    }

    CHECK_NULL(dev->spiport);
        
    ret = opmode(dev->spiport, dev->isftdi, OPMODE_SLEEP);
    switch (modulation) {
        case MOD_LORA:
            ret |= loramode(dev->spiport, dev->isftdi);
            ret |= lg02_reg_r(dev->spiport, dev->isftdi, SX1276_REG_LR_OPMODE, &val);
            ASSERT(( val & OPMODE_LORA) != 0);

            /* setup lora */
            ret |= lg02_setfreq(dev->spiport, dev->isftdi, dev->freq);
            ret |= lg02_setsf(dev->spiport, dev->isftdi, dev->sf);
            ret |= lg02_setsbw(dev->spiport, dev->isftdi, dev->bw);
            ret |= lg02_setcr(dev->spiport, dev->isftdi, dev->cr);
            ret |= lg02_setprlen(dev->spiport, dev->isftdi, dev->prlen);
            ret |= lg02_setsyncword(dev->spiport, dev->isftdi, dev->syncword);

            /* CRC check */
            ret |= lg02_crccheck(dev->spiport, dev->isftdi, dev->nocrc);

            // Boost on , 150% LNA current
            ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_LNA, LNA_MAX_GAIN);

            // Auto AGC Low datarate optiomize
            if (dev->sf < 11)
                ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_MODEMCONFIG3, SX1276_MC3_AGCAUTO);
            else
                ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_MODEMCONFIG3, SX1276_MC3_LOW_DATA_RATE_OPTIMIZE);

            //500kHz RX optimization
            ret |= lg02_reg_w(dev->spiport, dev->isftdi, 0x36, 0x02);
            ret |= lg02_reg_w(dev->spiport, dev->isftdi, 0x3a, 0x64);

            // configure output power,RFO pin Output power is limited to +14db
            //lg02_reg_w(dev->spiport, SX1276_REG_LR_PACONFIG, 0x8F);
            //lg02_reg_w(dev->spiport, SX1276_REG_LR_PADAC, lg02_reg_r(dev->spiport, SX1276_REG_LR_PADAC)|0x07);
            
            return ret;
        case MOD_FSK:
            return LGW_HAL_ERROR;
        default:
            return LGW_HAL_ERROR;
    }
    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_start_rx(sx127x_dev *dev, uint8_t modulation, uint8_t rxmode)
{
    int ret = 0;
    uint8_t val;
    switch (modulation) {

        case MOD_LORA:

            ret = lg02_reg_r(dev->spiport, dev->isftdi, SX1276_REG_LR_OPMODE, &val);
            ASSERT((val & OPMODE_LORA) != 0);

            /* enter standby mode (required for FIFO loading)) */
            ret |= opmode(dev->spiport, dev->isftdi, OPMODE_STANDBY);

            /* use inverted I/Q signal (prevent mote-to-mote communication) */
            
            ret |= lg02_reg_r(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ, &val);

            if (dev->invertio) {
                ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ,  (val & INVERTIQ_RX_MASK & INVERTIQ_TX_MASK) | INVERTIQ_RX_ON | INVERTIQ_TX_OFF);
                ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ2, INVERTIQ2_ON);
            } else {
                ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ, (val & INVERTIQ_RX_MASK & INVERTIQ_TX_MASK) | INVERTIQ_RX_OFF | INVERTIQ_TX_OFF);
                ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ2, INVERTIQ2_OFF);
            }


            if (rxmode == RXMODE_RSSI) { // use fixed settings for rssi scan
                ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_MODEMCONFIG1, 0x0A);
                ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_MODEMCONFIG2, 0x70);
            }

            ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_PAYLOADMAXLENGTH, 0x80);
            //lg02_reg_w(dev->spiport, SX1276_REG_LR_PAYLOADLENGTH, PAYLOAD_LENGTH);
            ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_HOPPERIOD, 0xFF);
            ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_FIFORXBASEADDR, 0x00);
            ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_FIFOADDRPTR, 0x00);

            // configure DIO mapping DIO0=RxDone DIO1=RxTout DIO2=NOP
            ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_DIOMAPPING1, MAP_DIO0_LORA_RXDONE|MAP_DIO1_LORA_RXTOUT|MAP_DIO2_LORA_NOP);

            // clear all radio IRQ flags
            ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGS, 0xFF);
            // enable required radio IRQs
            ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGSMASK, ~rxlorairqmask[rxmode]);

            //setsyncword(dev->spiport, LORA_MAC_PREAMBLE);  //LoraWan syncword

            // now instruct the radio to receive
            if (rxmode == RXMODE_SINGLE) { // single rx
                //printf("start rx_single\n");
                ret |= opmode(dev->spiport, dev->isftdi, OPMODE_RX_SINGLE);
                //lg02_reg_w(dev->spiport, SX1276_REG_LR_OPMODE, OPMODE_RX_SINGLE);
            } else { // continous rx (scan or rssi)
                ret |= opmode(dev->spiport, dev->isftdi, OPMODE_RX);
            }

            return ret;
        case MOD_FSK:
            return ret;
        default:
            return ret;
    }
    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_receive(void* spidev, bool isftdi, struct lgw_pkt_rx_s* pkt_data) {

    int i, ret = 0;
    uint8_t rssicorr, irqflags, curaddr, reccount, value;

    ret = lg02_reg_r(spidev, isftdi, SX1276_REG_LR_IRQFLAGS, &irqflags);

    CHECK_NULL(pkt_data);

    // clean all IRQ
    ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_IRQFLAGS, 0xFF);

    //DEBUG_PRINTF("Start receive, flags=%d\n", irqflags);

    if ((irqflags & IRQ_LORA_RXDONE_MASK) && (irqflags & IRQ_LORA_CRCERR_MASK) == 0) {


        ret |= lg02_reg_r(spidev, isftdi, SX1276_REG_LR_FIFORXCURRENTADDR, &curaddr);
        ret |= lg02_reg_r(spidev, isftdi, SX1276_REG_LR_RXNBBYTES, &reccount);

        pkt_data->size = reccount;

        ret |= lg02_reg_w(spidev, isftdi, SX1276_REG_LR_FIFOADDRPTR, curaddr);

        DEBUG_MSG("\nRXTX~ Receive(HEX):");
        for(i = 0; i < reccount; i++) {
            ret |= lg02_reg_r(spidev, isftdi, SX1276_REG_LR_FIFO, &value);
            pkt_data->payload[i] = value;
            DEBUG_PRINTF("%02x", pkt_data->payload[i]);
        }
        DEBUG_MSG("\n");

        ret |= lg02_reg_r(spidev, isftdi, SX1276_REG_LR_PKTSNRVALUE, &value);

        if( value & 0x80 ) // The SNR sign bit is 1
        {
            // Invert and divide by 4
            value = ( ( ~value + 1 ) & 0xFF ) >> 2;
            pkt_data->snr = -value;
        } else {
            // Divide by 4
            pkt_data->snr = ( value & 0xFF ) >> 2;
        }
        
        rssicorr = 157;

        ret |= lg02_reg_r(spidev, isftdi, SX1276_REG_LR_PKTRSSIVALUE, &value);
        pkt_data->rssic =  value - rssicorr;

    } /* else if (lg02_reg_r(spidev, SX1276_REG_LR_OPMODE) != (OPMODE_LORA | OPMODE_RX_SINGLE)) {  //single mode
        lg02_reg_w(spidev, SX1276_REG_LR_FIFOADDRPTR, 0x00);
        rxlora(spidev, RXMODE_SINGLE);
    }*/
    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_send(sx127x_dev *dev, struct lgw_pkt_tx_s *pkt_data) {

    int i, ret = 0;
    uint8_t val;

    struct timeval current_unix_time;
    uint32_t time_us;

    CHECK_NULL(pkt_data);

    ret = opmode(dev->spiport, dev->isftdi, OPMODE_SLEEP);
    // select LoRa modem (from sleep mode)
    ret |= loramode(dev->spiport, dev->isftdi);

    ret |= lg02_reg_r(dev->spiport, dev->isftdi, SX1276_REG_LR_OPMODE, &val);

    ASSERT((val & OPMODE_LORA) != 0);

    ret |= lg02_setpower(dev->spiport, dev->isftdi, pkt_data->rf_power);
    ret |= lg02_setfreq(dev->spiport, dev->isftdi, pkt_data->freq_hz);
    ret |= lg02_setsf(dev->spiport, dev->isftdi, lgw_sf_getval(pkt_data->datarate));
    ret |= lg02_setsbw(dev->spiport, dev->isftdi, lgw_bw_getval(pkt_data->bandwidth));
    ret |= lg02_setcr(dev->spiport, dev->isftdi, pkt_data->coderate);
    ret |= lg02_setprlen(dev->spiport, dev->isftdi, pkt_data->preamble);
    ret |= lg02_setsyncword(dev->spiport, dev->isftdi, dev->syncword);

    /* CRC check */
    ret |= lg02_crccheck(dev->spiport, dev->isftdi, pkt_data->no_crc);

    // Boost on , 150% LNA current
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_LNA, LNA_MAX_GAIN);

    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_PARAMP, 0x08);

    // Auto AGC
    if (dev->sf < 11)
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_MODEMCONFIG3, SX1276_MC3_AGCAUTO);
    else
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_MODEMCONFIG3, SX1276_MC3_LOW_DATA_RATE_OPTIMIZE);

    // configure output power,RFO pin Output power is limited to +14db
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_PACONFIG, (uint8_t)(0x80|(15&0xf)));

    ret |= lg02_reg_r(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ, &val);

    if (pkt_data->invert_pol) {
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ, (val & INVERTIQ_RX_MASK & INVERTIQ_TX_MASK) | INVERTIQ_RX_OFF | INVERTIQ_TX_ON);
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ2, INVERTIQ2_ON);
    } else {
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ, (val & INVERTIQ_RX_MASK & INVERTIQ_TX_MASK) | INVERTIQ_RX_OFF | INVERTIQ_TX_OFF);
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ2, INVERTIQ2_OFF);
    }

    // enter standby mode (required for FIFO loading))
    ret |= opmode(dev->spiport, dev->isftdi, OPMODE_STANDBY);

    // set the IRQ mapping DIO0=TxDone DIO1=NOP DIO2=NOP
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_DIOMAPPING1, MAP_DIO0_LORA_TXDONE|MAP_DIO1_LORA_NOP|MAP_DIO2_LORA_NOP);
    // clear all radio IRQ flags
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGS, 0xFF);
    // mask all IRQs but TxDone
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGSMASK, ~IRQ_LORA_TXDONE_MASK);

    // initialize the payload size and address pointers
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_FIFOTXBASEADDR, 0x00); 
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_FIFOADDRPTR, 0x00);

    for (i = 0; i < pkt_data->size; i++) { 
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_FIFO, pkt_data->payload[i]);
    }

    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_PAYLOADLENGTH, pkt_data->size);

    if (pkt_data->tx_mode == TIMESTAMPED) {
        gettimeofday(&current_unix_time, NULL);
        time_us = current_unix_time.tv_sec * 1000000UL + current_unix_time.tv_usec;
        time_us = pkt_data->count_us - 1495/*TX_START_DELAY*/ - time_us;
        if (time_us > 0 && time_us < 30000/*TX_JIT DELAY*/)
            wait_us(time_us);
    }

    // now we actually start the transmission
    ret |= opmode(dev->spiport, dev->isftdi, OPMODE_TX);

    // wait for TX done
    while(digital_read(dev->dio[0]) == 0);

    DEBUG_PRINTF("\nDEBUG~ Transmit at SF%iBW%d on %.6lf.\n", lgw_sf_getval(pkt_data->datarate), lgw_bw_getval(pkt_data->bandwidth)/1000, (double)(pkt_data->freq_hz)/1000000);

    // mask all IRQs
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGSMASK, 0xFF);

    // clear all radio IRQ flags
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGS, 0xFF);

    // go from stanby to sleep
    ret |= opmode(dev->spiport, dev->isftdi, OPMODE_SLEEP);

    return ret;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lg02_single_tx(sx127x_dev *dev, uint8_t* payload, int size) {

    int i, ret;
    uint8_t val;

    ret = lg02_reg_r(dev->spiport, dev->isftdi, SX1276_REG_LR_OPMODE, &val);

    ASSERT((val & OPMODE_LORA) != 0);

    // enter standby mode (required for FIFO loading))
    ret |= opmode(dev->spiport, dev->isftdi, OPMODE_STANDBY);

    val |= lg02_reg_r(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ, &val);

    if (dev->invertio) {
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ, (val & INVERTIQ_RX_MASK & INVERTIQ_TX_MASK) | INVERTIQ_RX_OFF | INVERTIQ_TX_ON);
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ2, INVERTIQ2_ON);
    } else {
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ, (val & INVERTIQ_RX_MASK & INVERTIQ_TX_MASK) | INVERTIQ_RX_OFF | INVERTIQ_TX_OFF);
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_INVERTIQ2, INVERTIQ2_OFF);
    }

    ret |= lg02_setsyncword(dev->spiport, dev->isftdi, dev->syncword);

    ret |= lg02_setpower(dev->spiport, dev->isftdi, dev->power);

    // set the IRQ mapping DIO0=TxDone DIO1=NOP DIO2=NOP
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_DIOMAPPING1, MAP_DIO0_LORA_TXDONE|MAP_DIO1_LORA_NOP|MAP_DIO2_LORA_NOP);
    // clear all radio IRQ flags
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGS, 0xFF);
    // mask all IRQs but TxDone
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGSMASK, ~IRQ_LORA_TXDONE_MASK);

    // initialize the payload size and address pointers
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_FIFOTXBASEADDR, 0x00); 
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_FIFOADDRPTR, 0x00);

    // write buffer to the radio FIFO

    for (i = 0; i < size; i++) { 
        ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_FIFO, payload[i]);
    }

    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_PAYLOADLENGTH, size);

    // now we actually start the transmission
    ret |= opmode(dev->spiport, dev->isftdi, OPMODE_TX);

    // wait for TX done
    while(digital_read(dev->dio[0]) == 0);

    DEBUG_PRINTF("\nINFO~Transmit at SF%iBW%d on %.6lf.\n", dev->sf, (dev->bw)/1000, (double)(dev->freq)/1000000);

    // mask all IRQs
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGSMASK, 0xFF);

    // clear all radio IRQ flags
    ret |= lg02_reg_w(dev->spiport, dev->isftdi, SX1276_REG_LR_IRQFLAGS, 0xFF);

    // go from stanby to sleep
    ret |= opmode(dev->spiport, dev->isftdi, OPMODE_SLEEP);

    return ret;
}

