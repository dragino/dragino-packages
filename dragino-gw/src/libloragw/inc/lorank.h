/*
 * lorank.h
 *
 *  Created on: Sep 21, 2015
 *      Author: ruud
 */

#ifndef _LORANK_H_
#define _LORANK_H_

/* Human readable platform definition */
#define DISPLAY_PLATFORM "Lorank"

/* parameters for native spi */
#define SPI_SPEED		8000000
#define SPI_DEV_PATH	"/dev/spidev1.0"
#define SPI_CS_CHANGE   1

/* parameters for a FT2232H */
#define VID		        0x0403
#define PID		        0x6014

#endif /* _LORANK_H_ */
