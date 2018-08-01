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
    [RXMODE_SINGLE] = IRQ_LORA_RXDONE_MASK|IRQ_LORA_CRCERR_MASK|IRQ_LORA_RXTOUT_MASK,
    [RXMODE_SCAN]   = IRQ_LORA_RXDONE_MASK|IRQ_LORA_CRCERR_MASK,
    [RXMODE_RSSI]   = 0x00,
};


/* GPIO configuration, true if GPIO is exposed */
const bool gpio_config[28] = {
    false, 	/* GPIO 0 */
    false, 	/* GPIO 1 */
    false, 	/* GPIO 2 */
    false, 	/* GPIO 3 */
    false, 	/* GPIO 4 */
    true, 	/* GPIO 5 */
    true, 	/* GPIO 6 */
    true, 	/* GPIO 7 */
    true, 	/* GPIO 8 */
    true, 	/* GPIO 9 */
    true, 	/* GPIO 10 */
    true, 	/* GPIO 11 */
    true, 	/* GPIO 12 */
    true, 	/* GPIO 13 */
    true, 	/* GPIO 14 */
    true, 	/* GPIO 15 */
    true, 	/* GPIO 16 */
    true, 	/* GPIO 17 */
    true, 	/* GPIO 18 */
    true, 	/* GPIO 19 */
    true, 	/* GPIO 20 */
    true, 	/* GPIO 21 */
    true, 	/* GPIO 22 */
    true, 	/* GPIO 23 */
    true, 	/* GPIO 24 */
    true, 	/* GPIO 25 */
    true, 	/* GPIO 26 */
    false, 	/* GPIO 27 */
};

/*
 * Reserve a GPIO for this program's use.
 * @gpio the GPIO pin to reserve.
 * @return true if the reservation was successful.
 */
static bool gpio_reserve(int gpio) {
    int fd; /* File descriptor for GPIO controller class */
    char buf[3]; /* Write buffer */

    /* Check if GPIO is valid */
    if (gpio > 27 || !gpio_config[gpio]) {
        return false;
    }

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

    /* Check if GPIO is valid */
    if (gpio > 27 || !gpio_config[gpio]) {
        return false;
    }

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

    /* Check if GPIO is valid */
    if (gpio > 27 || !gpio_config[gpio]) {
        return false;
    }

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

    /* Check if GPIO is valid */
    if (gpio > 27 || !gpio_config[gpio]) {
        return false;
    }

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

    /* Check if GPIO is valid */
    if (gpio > 27 || !gpio_config[gpio]) {
        return LOW;
    }

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
        syslog(LOG_DEBUG, "WARNING: SPI address > 127\n");
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
        syslog(LOG_ERR, "ERROR: SPI WRITE FAILURE\n");
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
        syslog(LOG_DEBUG, "WARNING: SPI address > 127\n");
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
        syslog(LOG_ERR, "ERROR: SPI READ FAILURE\n");
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

static void configPower (uint8_t spidev, int8_t pw) {
    // no boost used for now
    if(pw >= 17) {
        pw = 15;
    } else if(pw < 2) {
        pw = 2;
    }
    // check board type for BOOST pin
    writeReg(spidev, REG_PACONFIG, (uint8_t)(0x80|(pw&0xf)));
    writeReg(spidev, REG_PADAC, readReg(spidev, REG_PADAC)|0x4);
    writeReg(spidev, REG_PACONFIG, (uint8_t)(0x80|(pw-2)));
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

void crccheck(uint8_t spidev, uint8_t crc)
{
    if (crc)
        writeReg(spidev, REG_MODEM_CONFIG2, readReg(spidev, REG_MODEM_CONFIG2) | 0x04);
    else
        writeReg(spidev, REG_MODEM_CONFIG2, readReg(spidev, REG_MODEM_CONFIG2) & 0xfb);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/* SPI initialization and configuration */
int lgw_spi_open(char *spi_path) {
    int a = 0, b = 0;
    int i, spidev = 0;

    /* open SPI device */
    spidev = open(spi_path, O_RDWR);
    if (spidev < 0) {
        syslog(LOG_ERR, "ERROR: failed to open SPI device %s\n", spi_path);
        return -1;
    }

    /* setting SPI mode to 'mode 0' */
    i = SPI_MODE_0;
    a = ioctl(spidev, SPI_IOC_WR_MODE, &i);
    b = ioctl(spidev, SPI_IOC_RD_MODE, &i);
    if ((a < 0) || (b < 0)) {
        syslog(LOG_ERR, "ERROR(%s): SPI PORT FAIL TO SET IN MODE 0\n", spi_path);
        close(spidev);
        return -1;
    }

    /* setting SPI max clk (in Hz) */
    i = SPI_SPEED;
    a = ioctl(spidev, SPI_IOC_WR_MAX_SPEED_HZ, &i);
    b = ioctl(spidev, SPI_IOC_RD_MAX_SPEED_HZ, &i);
    if ((a < 0) || (b < 0)) {
        syslog(LOG_ERR, "ERROR(%s): SPI PORT FAIL TO SET MAX SPEED\n", spi_path);
        close(spidev);
        return -1;
    }

    /* setting SPI to MSB first */
    i = 0;
    a = ioctl(spidev, SPI_IOC_WR_LSB_FIRST, &i);
    b = ioctl(spidev, SPI_IOC_RD_LSB_FIRST, &i);
    if ((a < 0) || (b < 0)) {
        syslog(LOG_ERR, "ERROR(%s): SPI PORT FAIL TO SET MSB FIRST\n", spi_path);
        close(spidev);
        return -1;
    }

    /* setting SPI to 8 bits per word */
    i = 0;
    a = ioctl(spidev, SPI_IOC_WR_BITS_PER_WORD, &i);
    b = ioctl(spidev, SPI_IOC_RD_BITS_PER_WORD, &i);
    if ((a < 0) || (b < 0)) {
        syslog(LOG_ERR, "ERROR(%s): SPI PORT FAIL TO SET 8 BITS-PER-WORD\n", spi_path);
        close(spidev);
        return -1;
    }

    syslog(LOG_DEBUG, "Note(%s): SPI port opened and configured ok\n", spi_path);
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
        syslog(LOG_DEBUG, "%s: SX1276 detected, starting.\n", radiodev->desc);
        return true;
    } else {
        syslog(LOG_ERR, "%s: Unrecognized transceiver.\n", radiodev->desc);
        return false;
    }

}

void setup_channel(radiodev *radiodev)
{
    opmode(radiodev->spiport, OPMODE_SLEEP);
    // setup lora
    syslog(LOG_DEBUG, "Setup %s Channel: freq = %d, sf = %d, spi = %d\n", radiodev->desc, radiodev->freq, radiodev->sf, radiodev->spiport);
    setfreq(radiodev->spiport, radiodev->freq);
    writeReg(radiodev->spiport, REG_SYNC_WORD, LORA_MAC_PREAMBLE); // LoRaWAN public sync word
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
    crccheck(radiodev->spiport, radiodev->crc);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void rxlora(int spidev, uint8_t rxmode)
{

    opmodeLora(spidev);
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

    writeReg(spidev, REG_LNA, LNA_MAX_GAIN);  // max lna gain

    // configure DIO mapping DIO0=RxDone DIO1=RxTout DIO2=NOP
    writeReg(spidev, REG_DIO_MAPPING_1, MAP_DIO0_LORA_RXDONE|MAP_DIO1_LORA_RXTOUT|MAP_DIO2_LORA_NOP);

    // clear all radio IRQ flags
    writeReg(spidev, REG_IRQ_FLAGS, 0xFF);
    // enable required radio IRQs
    writeReg(spidev, REG_IRQ_FLAGS_MASK, ~rxlorairqmask[rxmode]);

    // now instruct the radio to receive
    if (rxmode == RXMODE_SINGLE) { // single rx
        opmode(spidev, OPMODE_RX_SINGLE);
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

    // payload crc check: 0x20
    if((irqflags & 0x20) == 0x20) {
        //printf("CRC error\n");
        writeReg(spidev, REG_FIFO_ADDR_PTR, 0x00);
        return false;
    } else {

        uint8_t currentAddr = readReg(spidev, REG_FIFO_RX_CURRENT_ADDR);
        uint8_t receivedCount = readReg(spidev, REG_RX_NB_BYTES);

        pkt_rx->size = receivedCount;

        writeReg(spidev, REG_FIFO_ADDR_PTR, currentAddr);

        for(i = 0; i < receivedCount; i++) {
            pkt_rx->payload[i] = (char)readReg(spidev, REG_FIFO);
            printf("%02x", pkt_rx->payload[i]);
        }

        printf("\n");

       // pkt_rx->payload[i] = '\0';

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

    }

    return true;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void txlora(uint8_t spidev, uint8_t *frame, uint8_t framelen) {

    int i;
    // select LoRa modem (from sleep mode)
    opmodeLora(spidev);

    ASSERT((readReg(spidev, REG_OPMODE) & OPMODE_LORA) != 0);

    // enter standby mode (required for FIFO loading))
    opmode(spidev, OPMODE_STANDBY);

    // configure output power
    writeReg(spidev, REG_PARAMP, (readReg(spidev, REG_PARAMP) & 0xF0) | 0x08); // set PA ramp-up time 50 uSec

    configPower(spidev, 17);

    // set the IRQ mapping DIO0=TxDone DIO1=NOP DIO2=NOP
    writeReg(spidev, REG_DIO_MAPPING_1, MAP_DIO0_LORA_TXDONE|MAP_DIO1_LORA_NOP|MAP_DIO2_LORA_NOP);
    // clear all radio IRQ flags
    writeReg(spidev, REG_IRQ_FLAGS, 0xFF);
    // mask all IRQs but TxDone
    writeReg(spidev, REG_IRQ_FLAGS_MASK, ~IRQ_LORA_TXDONE_MASK);

    // initialize the payload size and address pointers
    writeReg(spidev, REG_FIFO_TX_BASE_AD, 0x00);
    writeReg(spidev, REG_FIFO_ADDR_PTR, 0x00);

    /*  
    int curlen = readReg(spidev, REG_PAYLOAD_LENGTH);

    if ((curlen + framelen) > MAX_PAYLOADLEN) {
        framelen = MAX_PAYLOADLEN - curlen;
    }
    */

    // write buffer to the radio FIFO
    for (i = 0; i < framelen; i++) { 
        writeReg(spidev, REG_FIFO, frame[i]);
    }

    writeReg(spidev, REG_PAYLOAD_LENGTH, framelen);

    // now we actually start the transmission
    opmode(spidev, OPMODE_TX);

}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

