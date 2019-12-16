/*
 * imst_rpi.h
 *
 *  Created on: Dec 30, 2015
 *      Author: gonzalocasas
 */

#ifndef _IMST_RPI_H_
#define _IMST_RPI_H_

/* Human readable platform definition */
#define DISPLAY_PLATFORM "IMST + Rpi"

/* parameters for native spi */
#define SPI_SPEED		8000000
#define SPI_DEV_PATH	"/dev/spidev0.0"
#define SPI_CS_CHANGE   0

/* parameters for a FT2232H */
#define VID		        0x0403
#define PID		        0x6014

#endif /* _IMST_RPI_H_ */
