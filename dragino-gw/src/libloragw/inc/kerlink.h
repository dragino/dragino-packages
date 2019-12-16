/*
 * kerlink.h
 *
 *  Created on: Sep 21, 2015
 *      Author: ruud
 */

#ifndef _KERLINK_H_
#define _KERLINK_H_

/* Human readable platform definition */
#define DISPLAY_PLATFORM "Kerlink"

/* parameters for native spi */
#define SPI_SPEED		8000000
#define SPI_DEV_PATH	"/dev/spidev32766.0"
#define SPI_CS_CHANGE   1

/* parameters for a FT2232H */
#define VID		        0x0403
#define PID		        0x6010


#endif /* _KERLINK_H_ */
