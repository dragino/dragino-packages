/**
 * Author: Dragino 
 * Date: 16/01/2018
 * 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.dragino.com
 *
 */

#include "radio.h"

static const uint8_t rxlorairqmask[] = {
    [RXMODE_SINGLE] = IRQ_LORA_RXDONE_MASK|IRQ_LORA_CRCERR_MASK,
    [RXMODE_SCAN]   = IRQ_LORA_RXDONE_MASK|IRQ_LORA_CRCERR_MASK,
    [RXMODE_RSSI]   = 0x00,
};


/*
 * Reserve a GPIO for this program's use.
 * @gpio the GPIO pin to reserve.
 * @return true if the reservation was successful.
 */
static bool gpio_reserve(int gpio) {
    int fd; /* File descriptor for GPIO controller class */
    char buf[3]; /* Write buffer */

    /* Try to open GPIO controller class */
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        /* The file could not be opened */
        fprintf(stderr, "gpio_reserve: could not open /sys/class/gpio/export\r\n");
        return false;
    }

    /* Prepare buffer */
    snprintf(buf, 3, "%d", gpio);

    /* Try to reserve GPIO */
    if (write(fd, buf, strlen(buf)) < 0) {
        close(fd);
        fprintf(stderr, "gpio_reserve: could not write '%s' to /sys/class/gpio/export\r\n", buf);
        return false;
    }

    /* Close the GPIO controller class */
    if (close(fd) < 0) {
        fprintf(stderr, "gpio_reserve: could not close /sys/class/gpio/export\r\n");
        return false;
    }

    /* Success */
    return true;
}

/*
 * Release a GPIO after use.
 * @gpio the GPIO pin to release.
 * @return true if the release was successful.
 */
static bool gpio_release(int gpio) {
    int fd; /* File descriptor for GPIO controller class */
    char buf[3]; /* Write buffer */

    /* Try to open GPIO controller class */
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) {
        /* The file could not be opened */
       fprintf(stderr, "gpio_release: could not open /sys/class/gpio/unexport\r\n");
       return false;
    }

    /* Prepare buffer */
    snprintf(buf, 3, "%d", gpio);

    /* Try to release GPIO */
    if (write(fd, buf, strlen(buf)) < 0) {
        fprintf(stderr, "gpio_release: could not write /sys/class/gpio/unexport\r\n");
        return false;
    }

    /* Close the GPIO controller class */
    if (close(fd) < 0) {
        fprintf(stderr, "gpio_release: could not close /sys/class/gpio/unexport\r\n");
        return false;
    }

    /* Success */
    return true;
}

/*
 * Set the direction of the GPIO port.
 * @gpio the GPIO pin to release.
 * @direction the direction of the GPIO port.
 * @return true if the direction could be successfully set.
 */
static bool gpio_set_direction(int gpio, int direction) {
    int fd; /* File descriptor for GPIO port */
    char buf[33]; /* Write buffer */

    /* Make the GPIO port path */
    snprintf(buf, 33, "/sys/class/gpio/gpio%d/direction", gpio);

    /* Try to open GPIO port for writing only */
    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        /* The file could not be opened */
        return false;
    }

    /* Set the port direction */
    if (direction == GPIO_OUT) {
        if (write(fd, "out", 3) < 0) {
            return false;
        }
    } else {
        if (write(fd, "in", 2) < 0) {
            return false;
        }
    }

    /* Close the GPIO port */
    if (close(fd) < 0) {
        return false;
    }

    /* Success */
    return true;
}

/*
 * Set the state of the GPIO port.
 * @gpio the GPIO pin to set the state for.
 * @state 1 or 0
 * @return true if the state change was successful.
 */
static bool gpio_set_state(int gpio, int state) {
    int fd; /* File descriptor for GPIO port */
    char buf[29]; /* Write buffer */

    /* Make the GPIO port path */
    snprintf(buf, 29, "/sys/class/gpio/gpio%d/value", gpio);

    /* Try to open GPIO port */
    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        /* The file could not be opened */
        return false;
    }

    /* Set the port state */
    if (write(fd, (state == 1 ? "1" : "0"), 1) < 0) {
        return false;
    }

    /* Close the GPIO port */
    if (close(fd) < 0) {
        return false;
    }

    /* Success */
    return true;
}

/*
 * Get the state of the GPIO port.
 * @gpio the gpio pin to get the state from.
 * @return GPIO_HIGH if the pin is HIGH, GPIO_LOW if the pin is low. GPIO_ERR
 * when an error occured. 
 */
static int gpio_get_state(int gpio) {
    int fd;             /* File descriptor for GPIO port */
    char buf[29];       /* Write buffer */
    char port_state; /* Character indicating the port state */
    int state; /* API integer indicating the port state */

    /* Make the GPIO port path */
    snprintf(buf, 29, "/sys/class/gpio/gpio%d/value", gpio);

    /* Try to open GPIO port */
    fd = open(buf, O_RDONLY);
    if (fd < 0) {
        /* The file could not be opened */
        fprintf(stderr, "gpio_get_state: could not open /sys/class/gpio/gpio%d/value\r\n", gpio);
        return LOW;
    }

    /* Read the port state */
    if (read(fd, &port_state, 1) < 0) {
        close(fd);
        fprintf(stderr, "gpio_get_state: could not read /sys/class/gpio/gpio%d/value\r\n", gpio);
        return LOW;
    }

    /* Translate the port state into API state */
    state = port_state == '1' ? HIGH : LOW;

    /* Close the GPIO port */
    if (close(fd) < 0) {
        fprintf(stderr, "gpio_get_state: could not close /sys/class/gpio/gpio%d/value\r\n", gpio);
        return LOW;
    }

    /* Return the state */
    return state;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void digitalWrite(int gpio, int state)
{
    gpio_reserve(gpio);
    gpio_set_direction(gpio, GPIO_OUT);
    gpio_set_state(gpio, state);
    gpio_release(gpio);
} 

/*
 * Read the state of the port. The port can be input
 * or output.
 * @gpio the GPIO pin to read from.
 * @return GPIO_HIGH if the pin is HIGH, GPIO_LOW if the pin is low. Output
 * is -1 when an error occurred.
 */
int digitalRead(int gpio) {
    int state; /* The port state */

    /* Reserve the port */
    if (!gpio_reserve(gpio)) {
        return -1;
    }

    /* Read the port */
    state = gpio_get_state(gpio);

    if (!gpio_release(gpio)) {
        return -1;
    }

    /* Return the port state */
    return state;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void selectreceiver(int pin)
{
    digitalWrite(pin, LOW);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void unselectreceiver(int pin)
{
    digitalWrite(pin, HIGH);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple write */
static int lgw_spi_w(uint8_t spidev, uint8_t address, uint8_t data) {
    uint8_t out_buf[3];
    uint8_t command_size;
    struct spi_ioc_transfer k;
    int a;

    /* check input variables */
    if ((address & 0x80) != 0) {
        fprintf(stderr, "WARNING: SPI address > 127\n");
    }

    /* prepare frame to be sent */
    out_buf[0] = WRITE_ACCESS | (address & 0x7F);
    out_buf[1] = data;
    command_size = 2;

    /* I/O transaction */
    memset(&k, 0, sizeof(k)); /* clear k */
    k.tx_buf = (unsigned long) out_buf;
    k.len = command_size;
    k.speed_hz = SPI_SPEED;
    k.cs_change = 0;
    k.bits_per_word = 8;
    a = ioctl(spidev, SPI_IOC_MESSAGE(1), &k);

    /* determine return code */
    if (a != (int)k.len) {
        fprintf(stderr, "ERROR: SPI WRITE FAILURE\n");
        return -1;
    } else {
        //printf("Note: SPI write success\n");
        return 0;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple read */
static int lgw_spi_r(uint8_t spidev, uint8_t address, uint8_t *data) {
    uint8_t out_buf[3];
    uint8_t command_size;
    uint8_t in_buf[10];
    struct spi_ioc_transfer k;
    int a;

    /* check input variables */
    if ((address & 0x80) != 0) {
        fprintf(stderr, "WARNING: SPI address > 127\n");
    }

    /* prepare frame to be sent */
    out_buf[0] = READ_ACCESS | (address & 0x7F);
    out_buf[1] = 0x00;
    command_size = 2;

    /* I/O transaction */
    memset(&k, 0, sizeof(k)); /* clear k */
    k.tx_buf = (unsigned long) out_buf;
    k.rx_buf = (unsigned long) in_buf;
    k.len = command_size;
    k.cs_change = 0;
    a = ioctl(spidev, SPI_IOC_MESSAGE(1), &k);

    /* determine return code */
    if (a != (int)k.len) {
        fprintf(stderr, "ERROR: SPI READ FAILURE\n");
        return -1;
    } else {
        //printf("Note: SPI read success\n");
        *data = in_buf[command_size - 1];
        return 0;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static uint8_t readReg(uint8_t spidev, uint8_t addr)
{
    uint8_t data = 0x00;

    lgw_spi_r(spidev, addr, &data);

    return data;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void writeReg(uint8_t spidev, uint8_t addr, uint8_t value)
{
    lgw_spi_w(spidev, addr, value);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void opmode (uint8_t spidev, uint8_t mode) {
    writeReg(spidev, REG_OPMODE, (readReg(spidev, REG_OPMODE) & ~OPMODE_MASK) | mode);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void opmodeLora(uint8_t spidev) {
    uint8_t u = OPMODE_LORA | 0x8; // TBD: sx1276 high freq
    writeReg(spidev, REG_OPMODE, u);
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setfreq(uint8_t spidev, long frequency)
{
    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;

    writeReg(spidev, REG_FRF_MSB, (uint8_t)(frf >> 16));
    writeReg(spidev, REG_FRF_MID, (uint8_t)(frf >> 8));
    writeReg(spidev, REG_FRF_LSB, (uint8_t)(frf >> 0));
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setsf(uint8_t spidev, int sf)
{
    if (sf < 6) {
        sf = 6;
    } else if (sf > 12) {
        sf = 12;
    }

    if (sf == 6) {
        writeReg(spidev, REG_DETECTION_OPTIMIZE, 0xc5);
        writeReg(spidev, REG_DETECTION_THRESHOLD, 0x0c);
    } else {
        writeReg(spidev, REG_DETECTION_OPTIMIZE, 0xc3);
        writeReg(spidev, REG_DETECTION_THRESHOLD, 0x0a);
    }

    writeReg(spidev, REG_MODEM_CONFIG2, (readReg(spidev, REG_MODEM_CONFIG2) & 0x0f) | ((sf << 4) & 0xf0));
    MSG_LOG(DEBUG_SPI, "SF=0x%02x\n", sf, readReg(spidev, REG_MODEM_CONFIG2));
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setsbw(uint8_t spidev, long sbw)
{
    int bw;

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

    writeReg(spidev, REG_MODEM_CONFIG, (readReg(spidev, REG_MODEM_CONFIG) & 0x0f) | (bw << 4));
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setcr(uint8_t spidev, int denominator)
{
    if (denominator < 5) {
        denominator = 5;
    } else if (denominator > 8) {
        denominator = 8;
    }

    int cr = denominator - 4;

    writeReg(spidev, REG_MODEM_CONFIG, (readReg(spidev, REG_MODEM_CONFIG) & 0xf1) | (cr << 1));
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setprlen(uint8_t spidev, long length)
{
    writeReg(spidev, REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
    writeReg(spidev, REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void setsyncword(uint8_t spidev, int sw)
{
    writeReg(spidev, REG_SYNC_WORD, sw);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void crccheck(uint8_t spidev, uint8_t nocrc)
{
    if (nocrc)
        writeReg(spidev, REG_MODEM_CONFIG2, readReg(spidev, REG_MODEM_CONFIG2) & 0xfb);
    else
        writeReg(spidev, REG_MODEM_CONFIG2, readReg(spidev, REG_MODEM_CONFIG2) | 0x04);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/* SPI initialization and configuration */
int lgw_spi_open(char *spi_path) {
    int a = 0, b = 0;
    int i, spidev = 0;

    /* open SPI device */
    spidev = open(spi_path, O_RDWR);
    if (spidev < 0) {
        fprintf(stderr, "ERROR: failed to open SPI device %s\n", spi_path);
        return -1;
    }

    /* setting SPI mode to 'mode 0' */
    i = SPI_MODE_0;
    a = ioctl(spidev, SPI_IOC_WR_MODE, &i);
    b = ioctl(spidev, SPI_IOC_RD_MODE, &i);
    if ((a < 0) || (b < 0)) {
        fprintf(stderr, "ERROR(%s): SPI PORT FAIL TO SET IN MODE 0\n", spi_path);
        close(spidev);
        return -1;
    }

    /* setting SPI max clk (in Hz) */
    i = SPI_SPEED;
    a = ioctl(spidev, SPI_IOC_WR_MAX_SPEED_HZ, &i);
    b = ioctl(spidev, SPI_IOC_RD_MAX_SPEED_HZ, &i);
    if ((a < 0) || (b < 0)) {
        fprintf(stderr, "ERROR(%s): SPI PORT FAIL TO SET MAX SPEED\n", spi_path);
        close(spidev);
        return -1;
    }

    /* setting SPI to MSB first */
    i = 0;
    a = ioctl(spidev, SPI_IOC_WR_LSB_FIRST, &i);
    b = ioctl(spidev, SPI_IOC_RD_LSB_FIRST, &i);
    if ((a < 0) || (b < 0)) {
        fprintf(stderr, "ERROR(%s): SPI PORT FAIL TO SET MSB FIRST\n", spi_path);
        close(spidev);
        return -1;
    }

    /* setting SPI to 8 bits per word */
    i = 0;
    a = ioctl(spidev, SPI_IOC_WR_BITS_PER_WORD, &i);
    b = ioctl(spidev, SPI_IOC_RD_BITS_PER_WORD, &i);
    if ((a < 0) || (b < 0)) {
        fprintf(stderr, "ERROR(%s): SPI PORT FAIL TO SET 8 BITS-PER-WORD\n", spi_path);
        close(spidev);
        return -1;
    }

    //fprintf(stderr, "Note(%s): SPI port opened and configured ok\n", spi_path);
    return spidev;
}



/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
// Lora configure : Freq, SF, BW
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool get_radio_version(radiodev *radiodev)
{
    digitalWrite(radiodev->rst, LOW);
    sleep(1);
    digitalWrite(radiodev->rst, HIGH);
    sleep(1);

    opmode(radiodev->spiport, OPMODE_SLEEP);
    uint8_t version = readReg(radiodev->spiport, REG_VERSION);

    if (version == 0x12) {
        fprintf(stderr, "%s: SX1276 detected, starting.\n", radiodev->desc);
        return true;
    } else {
        fprintf(stderr, "%s: Unrecognized transceiver.\n", radiodev->desc);
        return false;
    }

}

void setup_channel(radiodev *radiodev)
{
    opmode(radiodev->spiport, OPMODE_SLEEP);
    opmodeLora(radiodev->spiport);
    ASSERT((readReg(radiodev->spiport, REG_OPMODE) & OPMODE_LORA) != 0);
    // setup lora
    printf("Setup %s Channel: freq = %d, sf = %d, spi = %d\n", radiodev->desc, radiodev->freq, radiodev->sf, radiodev->spiport);
    setfreq(radiodev->spiport, radiodev->freq);
    setsf(radiodev->spiport, radiodev->sf);
    setsbw(radiodev->spiport, radiodev->bw);
    setcr(radiodev->spiport, radiodev->cr);
    setprlen(radiodev->spiport, radiodev->prlen);
    setsyncword(radiodev->spiport, LORA_MAC_PREAMBLE);

    /* use inverted I/Q signal (prevent mote-to-mote communication) */
    
    if (!radiodev->invertio)
        writeReg(radiodev->spiport, REG_INVERTIQ, readReg(radiodev->spiport, REG_INVERTIQ) & ~(1<<6));
    else
        writeReg(radiodev->spiport, REG_INVERTIQ, readReg(radiodev->spiport, REG_INVERTIQ) | (1<<6));
    

    /* CRC check */
    crccheck(radiodev->spiport, radiodev->nocrc);

    // Boost on , 150% LNA current
    writeReg(radiodev->spiport, REG_LNA, LNA_MAX_GAIN);

    // Auto AGC
    writeReg(radiodev->spiport, REG_MODEM_CONFIG3, SX1276_MC3_AGCAUTO);

    // configure output power,RFO pin Output power is limited to +14db
    writeReg(radiodev->spiport, REG_PACONFIG, (uint8_t)(0x80|(15&0xf)));
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void rxlora(int spidev, uint8_t rxmode)
{

    ASSERT((readReg(spidev, REG_OPMODE) & OPMODE_LORA) != 0);

    // enter standby mode (required for FIFO loading))
    opmode(spidev, OPMODE_STANDBY);

    if(rxmode == RXMODE_RSSI) { // use fixed settings for rssi scan
        writeReg(spidev, REG_MODEM_CONFIG, RXLORA_RXMODE_RSSI_REG_MODEM_CONFIG1);
        writeReg(spidev, REG_MODEM_CONFIG2, RXLORA_RXMODE_RSSI_REG_MODEM_CONFIG2);
    }

    writeReg(spidev, REG_MAX_PAYLOAD_LENGTH, 0x80);
    //writeReg(spidev, REG_PAYLOAD_LENGTH, PAYLOAD_LENGTH);
    writeReg(spidev, REG_HOP_PERIOD, 0xFF);
    writeReg(spidev, REG_FIFO_RX_BASE_AD, 0x00);
    writeReg(spidev, REG_FIFO_ADDR_PTR, 0x00);

    // configure DIO mapping DIO0=RxDone DIO1=RxTout DIO2=NOP
    writeReg(spidev, REG_DIO_MAPPING_1, MAP_DIO0_LORA_RXDONE|MAP_DIO1_LORA_RXTOUT|MAP_DIO2_LORA_NOP);

    // clear all radio IRQ flags
    writeReg(spidev, REG_IRQ_FLAGS, 0xFF);
    // enable required radio IRQs
    writeReg(spidev, REG_IRQ_FLAGS_MASK, ~rxlorairqmask[rxmode]);

    setsyncword(spidev, LORA_MAC_PREAMBLE);  //LoraWan syncword

    // now instruct the radio to receive
    if (rxmode == RXMODE_SINGLE) { // single rx
        //printf("start rx_single\n");
        opmode(spidev, OPMODE_RX_SINGLE);
        //writeReg(spidev, REG_OPMODE, OPMODE_RX_SINGLE);
    } else { // continous rx (scan or rssi)
        opmode(spidev, OPMODE_RX);
    }

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool received(uint8_t spidev, struct pkt_rx_s *pkt_rx) {

    int i, rssicorr;

    int irqflags = readReg(spidev, REG_IRQ_FLAGS);

    // clean all IRQ
    writeReg(spidev, REG_IRQ_FLAGS, 0xFF);

    //printf("Start receive, flags=%d\n", irqflags);

    if ((irqflags & IRQ_LORA_RXDONE_MASK) && (irqflags & IRQ_LORA_CRCERR_MASK) == 0) {

        uint8_t currentAddr = readReg(spidev, REG_FIFO_RX_CURRENT_ADDR);
        uint8_t receivedCount = readReg(spidev, REG_RX_NB_BYTES);

        pkt_rx->size = receivedCount;

        writeReg(spidev, REG_FIFO_ADDR_PTR, currentAddr);

        printf("\nReceive(HEX):");
        for(i = 0; i < receivedCount; i++) {
            pkt_rx->payload[i] = (char)readReg(spidev, REG_FIFO);
            printf("%02x", pkt_rx->payload[i]);
        }
        printf("\n");

        uint8_t value = readReg(spidev, REG_PKT_SNR_VALUE);

        if( value & 0x80 ) // The SNR sign bit is 1
        {
            // Invert and divide by 4
            value = ( ( ~value + 1 ) & 0xFF ) >> 2;
            pkt_rx->snr = -value;
        } else {
            // Divide by 4
            pkt_rx->snr = ( value & 0xFF ) >> 2;
        }
        
        rssicorr = 157;

        pkt_rx->rssi = readReg(spidev, REG_PKTRSSI) - rssicorr;

        pkt_rx->empty = 0;  /* make sure save the received messaeg */

        return true;
    } /* else if (readReg(spidev, REG_OPMODE) != (OPMODE_LORA | OPMODE_RX_SINGLE)) {  //single mode
        writeReg(spidev, REG_FIFO_ADDR_PTR, 0x00);
        rxlora(spidev, RXMODE_SINGLE);
    }*/
    return false;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void txlora(radiodev *radiodev, struct pkt_tx_s *pkt) {

    opmode(radiodev->spiport, OPMODE_SLEEP);
    // select LoRa modem (from sleep mode)
    opmodeLora(radiodev->spiport);

    ASSERT((readReg(radiodev->spiport, REG_OPMODE) & OPMODE_LORA) != 0);

    setfreq(radiodev->spiport, pkt->freq_hz);
    setsf(radiodev->spiport, sf_getval(pkt->datarate));
    setsbw(radiodev->spiport, bw_getval(pkt->bandwidth));
    setcr(radiodev->spiport, pkt->coderate);
    setprlen(radiodev->spiport, pkt->preamble);
    setsyncword(radiodev->spiport, LORA_MAC_PREAMBLE);

    /* CRC check */
    crccheck(radiodev->spiport, pkt->no_crc);

    // Boost on , 150% LNA current
    writeReg(radiodev->spiport, REG_LNA, LNA_MAX_GAIN);

    // Auto AGC
    writeReg(radiodev->spiport, REG_MODEM_CONFIG3, SX1276_MC3_AGCAUTO);

    // configure output power,RFO pin Output power is limited to +14db
    writeReg(radiodev->spiport, REG_PACONFIG, (uint8_t)(0x80|(15&0xf)));

    if (pkt->invert_pol)
        writeReg(radiodev->spiport, REG_INVERTIQ, readReg(radiodev->spiport, REG_INVERTIQ) | (1<<6));
    else
        writeReg(radiodev->spiport, REG_INVERTIQ, readReg(radiodev->spiport, REG_INVERTIQ) & ~(1<<6));

    // enter standby mode (required for FIFO loading))
    opmode(radiodev->spiport, OPMODE_STANDBY);

    // set the IRQ mapping DIO0=TxDone DIO1=NOP DIO2=NOP
    writeReg(radiodev->spiport, REG_DIO_MAPPING_1, MAP_DIO0_LORA_TXDONE|MAP_DIO1_LORA_NOP|MAP_DIO2_LORA_NOP);
    // clear all radio IRQ flags
    writeReg(radiodev->spiport, REG_IRQ_FLAGS, 0xFF);
    // mask all IRQs but TxDone
    writeReg(radiodev->spiport, REG_IRQ_FLAGS_MASK, ~IRQ_LORA_TXDONE_MASK);

    // initialize the payload size and address pointers
    writeReg(radiodev->spiport, REG_FIFO_TX_BASE_AD, 0x00); writeReg(radiodev->spiport, REG_FIFO_ADDR_PTR, 0x00);

    // write buffer to the radio FIFO
    int i;

    for (i = 0; i < pkt->size; i++) { 
        writeReg(radiodev->spiport, REG_FIFO, pkt->payload[i]);
    }

    writeReg(radiodev->spiport, REG_PAYLOAD_LENGTH, pkt->size);

    // now we actually start the transmission
    opmode(radiodev->spiport, OPMODE_TX);

    // wait for TX done
    while(digitalRead(radiodev->dio[0]) == 0);

    MSG("\nTransmit at SF%iBW%ld on %.6lf.\n", sf_getval(pkt->datarate), bw_getval(pkt->bandwidth)/1000, (double)(pkt->freq_hz)/1000000);

    // mask all IRQs
    writeReg(radiodev->spiport, REG_IRQ_FLAGS_MASK, 0xFF);

    // clear all radio IRQ flags
    writeReg(radiodev->spiport, REG_IRQ_FLAGS, 0xFF);

    // go from stanby to sleep
    opmode(radiodev->spiport, OPMODE_SLEEP);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void single_tx(radiodev *radiodev, uint8_t *payload, int size) {

    ASSERT((readReg(radiodev->spiport, REG_OPMODE) & OPMODE_LORA) != 0);

    // enter standby mode (required for FIFO loading))
    opmode(radiodev->spiport, OPMODE_STANDBY);

    setsyncword(radiodev->spiport, LORA_MAC_PREAMBLE);

    // set the IRQ mapping DIO0=TxDone DIO1=NOP DIO2=NOP
    writeReg(radiodev->spiport, REG_DIO_MAPPING_1, MAP_DIO0_LORA_TXDONE|MAP_DIO1_LORA_NOP|MAP_DIO2_LORA_NOP);
    // clear all radio IRQ flags
    writeReg(radiodev->spiport, REG_IRQ_FLAGS, 0xFF);
    // mask all IRQs but TxDone
    writeReg(radiodev->spiport, REG_IRQ_FLAGS_MASK, ~IRQ_LORA_TXDONE_MASK);

    // initialize the payload size and address pointers
    writeReg(radiodev->spiport, REG_FIFO_TX_BASE_AD, 0x00); writeReg(radiodev->spiport, REG_FIFO_ADDR_PTR, 0x00);

    // write buffer to the radio FIFO
    int i;

    for (i = 0; i < size; i++) { 
        writeReg(radiodev->spiport, REG_FIFO, payload[i]);
    }

    writeReg(radiodev->spiport, REG_PAYLOAD_LENGTH, size);

    // now we actually start the transmission
    opmode(radiodev->spiport, OPMODE_TX);

    // wait for TX done
    while(digitalRead(radiodev->dio[0]) == 0);

    MSG("\nTransmit at SF%iBW%ld on %.6lf.\n", radiodev->sf, (radiodev->bw)/1000, (double)(radiodev->freq)/1000000);

    // mask all IRQs
    writeReg(radiodev->spiport, REG_IRQ_FLAGS_MASK, 0xFF);

    // clear all radio IRQ flags
    writeReg(radiodev->spiport, REG_IRQ_FLAGS, 0xFF);

    // go from stanby to sleep
    opmode(radiodev->spiport, OPMODE_SLEEP);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lockfile(int fd)
{
    struct flock fl;

    fl.l_type = F_WRLCK;
    fl.l_start = 0;
    fl.l_whence = SEEK_SET;
    fl.l_len = 0;
    return(fcntl(fd, F_SETLK, &fl));
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int already_running(void)
{
    int     fd;
    char    buf[16];

    fd = open(LOCKFILE, O_RDWR|O_CREAT, LOCKMODE);
    if (fd < 0) {
        fprintf(stderr, "can't open %s: %s", LOCKFILE, strerror(errno));
        exit(1);
    }
    if (lockfile(fd) < 0) {
        if (errno == EACCES || errno == EAGAIN) {
            close(fd);
            return(1);
        }
        fprintf(stderr, "can't lock %s: %s", LOCKFILE, strerror(errno));
        exit(1);
    }
    ftruncate(fd, 0);
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf)+1);
    return(0);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int32_t bw_getval(int x) {
    switch (x) {
        case BW_500KHZ: return 500000;
        case BW_250KHZ: return 250000;
        case BW_125KHZ: return 125000;
        case BW_62K5HZ: return 62500;
        case BW_31K2HZ: return 31200;
        case BW_15K6HZ: return 15600;
        case BW_7K8HZ : return 7800;
        default: return -1;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int32_t sf_getval(int x) {
    switch (x) {
        case DR_LORA_SF7: return 7;
        case DR_LORA_SF8: return 8;
        case DR_LORA_SF9: return 9;
        case DR_LORA_SF10: return 10;
        case DR_LORA_SF11: return 11;
        case DR_LORA_SF12: return 12;
        default: return -1;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int32_t sf_toval(int x) {
    switch (x) {
        case 7: return DR_LORA_SF7; 
        case 8: return DR_LORA_SF8; 
        case 9: return DR_LORA_SF9;
        case 10: return DR_LORA_SF10;
        case 11: return DR_LORA_SF11;
        case 12: return DR_LORA_SF12;
        default: return -1;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int32_t bw_toval(int x) {
    switch (x) {
        case 500000: return BW_500KHZ;
        case 250000: return BW_250KHZ;
        case 125000: return BW_125KHZ;
        case 62500: return BW_62K5HZ;
        case 31200: return BW_31K2HZ;
        case 15600: return BW_15K6HZ;
        case 7800: return BW_7K8HZ;
        default: return -1;
    }
}
