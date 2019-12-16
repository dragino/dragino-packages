/*
 * linklabs_blowfish_rpi.h
 *
 *  Created on: Feb 23, 2016
 *      Author: ticapix
 */

#ifndef _LINKLABS_BLOWFISH_H_
#define _LINKLABS_BLOWFISH_H_

/* Human readable platform definition */
#define DISPLAY_PLATFORM "LinkLabs Blowfish + Rpi"

/* parameters for native spi */
#define SPI_SPEED		8000000
#define SPI_DEV_PATH	"/dev/spidev0.0"
#define SPI_CS_CHANGE   0

/* parameters for a FT2232H */
#define VID		        0x0403
#define PID		        0x6014

#endif /* _LINKLABS_BLOWFISH_H_ */
