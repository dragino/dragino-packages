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

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>
#include <stdio.h>

#include "loragw_spi.h"

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

#define LGW_DETECT

char SPI_DEV_PATH[64] = {'\0'};
int spi_mux_target = 0;

int main(int argc, char *argv[])
{
    int i;
    void *spi_target = NULL;
    uint8_t data = 0;
    uint8_t spi_mux_mode = LGW_SPI_MUX_MODE0;

    /* argv[1] = spi_target; argv[2] = spi_mux_target 
     * spi_target : (1) /dev/spidev1.0  (2) /dev/spidev2.0
     * spi_mux_target : SX1301 = 0x0,  SX127X = 0x3 
     * */ 
    if (argc < 3) 
        return -1;

    strncpy(SPI_DEV_PATH, argv[1], sizeof(SPI_DEV_PATH));
    spi_mux_target = atoi(argv[2]);
    
    lgw_spi_open(&spi_target);

    lgw_spi_r(spi_target, spi_mux_mode, spi_mux_target, 0x1, &data);
    printf("data received (simple read): %d\n", data);

    lgw_spi_close(spi_target);
    printf("End of test for loragw_spi.c\n");

    return 0;
}

/* --- EOF ------------------------------------------------------------------ */
