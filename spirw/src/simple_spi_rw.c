#define _POSIX_C_SOURCE 200112L

#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <linux/spi/spidev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int spidev = 0;

/*******************************************************************************
 *
 * Configure these values!
 *
 *******************************************************************************/

int RST   = 8;

#define LOW 		0
#define HIGH 		1

#define GPIO_OUT        0
#define GPIO_IN         1

#define READ_ACCESS     0x00
#define WRITE_ACCESS    0x80
#define SPI_SPEED       8000000
#define SPI_DEV_PATH    "/dev/spidev1.0"

// #############################################
// #############################################

#define REG_FIFO                    0x00
#define REG_OPMODE                  0x01
#define REG_VERSION                 0x42

/* GPIO configuration, true if GPIO is exposed */
const bool gpio_config[28] = {
    false, 	/* GPIO 0 */
    false, 	/* GPIO 1 */
    false, 	/* GPIO 2 */
    false, 	/* GPIO 3 */
    false, 	/* GPIO 4 */
    false, 	/* GPIO 5 */
    true, 	/* GPIO 6 */
    true, 	/* GPIO 7 */
    true, 	/* GPIO 8 */
    false, 	/* GPIO 9 */
    false, 	/* GPIO 10 */
    false, 	/* GPIO 11 */
    false, 	/* GPIO 12 */
    true, 	/* GPIO 13 */
    false, 	/* GPIO 14 */
    false, 	/* GPIO 15 */
    true, 	/* GPIO 16 */
    true, 	/* GPIO 17 */
    true, 	/* GPIO 18 */
    true, 	/* GPIO 19 */
    true, 	/* GPIO 20 */
    false, 	/* GPIO 21 */
    true, 	/* GPIO 22 */
    true, 	/* GPIO 23 */
    true, 	/* GPIO 24 */
    false, 	/* GPIO 25 */
    false, 	/* GPIO 26 */
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

static void digitalWrite(int gpio, int state)
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
static int digitalRead(int gpio) {
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

/* SPI initialization and configuration */
static int lgw_spi_open() {
    int a=0, b=0;
    int i;

    /* open SPI device */
    spidev = open(SPI_DEV_PATH, O_RDWR);
    if (spidev < 0) {
        printf("ERROR: failed to open SPI device %s\n", SPI_DEV_PATH);
        return -1;
    }

    /* setting SPI mode to 'mode 0' */
    i = SPI_MODE_0;
    a = ioctl(spidev, SPI_IOC_WR_MODE, &i);
    b = ioctl(spidev, SPI_IOC_RD_MODE, &i);
    if ((a < 0) || (b < 0)) {
        printf("ERROR: SPI PORT FAIL TO SET IN MODE 0\n");
        close(spidev);
        return -1;
    }

    /* setting SPI max clk (in Hz) */
    i = SPI_SPEED;
    a = ioctl(spidev, SPI_IOC_WR_MAX_SPEED_HZ, &i);
    b = ioctl(spidev, SPI_IOC_RD_MAX_SPEED_HZ, &i);
    if ((a < 0) || (b < 0)) {
        printf("ERROR: SPI PORT FAIL TO SET MAX SPEED\n");
        close(spidev);
        return -1;
    }

    /* setting SPI to MSB first */
    i = 0;
    a = ioctl(spidev, SPI_IOC_WR_LSB_FIRST, &i);
    b = ioctl(spidev, SPI_IOC_RD_LSB_FIRST, &i);
    if ((a < 0) || (b < 0)) {
        printf("ERROR: SPI PORT FAIL TO SET MSB FIRST\n");
        close(spidev);
        return -1;
    }

    /* setting SPI to 8 bits per word */
    i = 0;
    a = ioctl(spidev, SPI_IOC_WR_BITS_PER_WORD, &i);
    b = ioctl(spidev, SPI_IOC_RD_BITS_PER_WORD, &i);
    if ((a < 0) || (b < 0)) {
        printf("ERROR: SPI PORT FAIL TO SET 8 BITS-PER-WORD\n");
        close(spidev);
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple write */
static int lgw_spi_w( uint8_t address, uint8_t data) {
    uint8_t out_buf[3];
    uint8_t command_size;
    struct spi_ioc_transfer k;
    int a;

    /* check input variables */
    if ((address & 0x80) != 0) {
        printf("WARNING: SPI address > 127\n");
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
        printf("ERROR: SPI WRITE FAILURE\n");
        return -1;
    } else {
        return 0;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple read */
static int lgw_spi_r(uint8_t address, uint8_t *data) {
    uint8_t out_buf[3];
    uint8_t command_size;
    uint8_t in_buf[10];
    struct spi_ioc_transfer k;
    int a;

    /* check input variables */
    if ((address & 0x80) != 0) {
        printf("WARNING: SPI address > 127\n");
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
        printf("ERROR: SPI READ FAILURE\n");
        return -1;
    } else {
        *data = in_buf[command_size - 1];
        return 0;
    }
}


static uint8_t readRegister(uint8_t addr)
{
    uint8_t data = 0x00;

    lgw_spi_r(addr, &data);

    return data;
}

static void writeRegister(uint8_t addr, uint8_t value)
{
    lgw_spi_w(addr, value);
}

static bool get_version()
{
    // sx1276?
    digitalWrite(RST, LOW);
    sleep(1);
    digitalWrite(RST, HIGH);
    sleep(1);
    uint8_t version = readRegister(REG_VERSION);
    //printf("Version: 0x%x\n", version);
    if (version == 0x12) {
        // sx1276
        //printf("SX1276 detected, starting.\n");
        return true;
    } else {
        return false;
    }
}

int main () {
    FILE *fp;

    lgw_spi_open();

    if ((fp = fopen("/var/iot/board", "w")) < 0) {
        perror("open board");
        close(spidev);
        return 1;
    }

    if (get_version())
        fprintf(fp, "LG02\n");
    else
        fprintf(fp, "LG08\n");

    fclose(fp);

    close(spidev);

    return 0;

}
