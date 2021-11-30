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

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "gpio.h"

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

void digital_write(int gpio, int state)
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
int digital_read(int gpio) {
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

