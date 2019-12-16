/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
	Host specific functions to address the LoRa concentrator registers through
	a SPI interface.
	Single-byte read/write and burst read/write.
	Does not handle pagination.
	Could be used with multiple SPI ports in parallel (explicit file descriptor)

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont [support for ftdi was dropped from version 4.0.0 and onwards]
Maintainer: Ruud Vlaming
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>		/* C99 types */
#include <stdio.h>		/* printf fprintf */
#include <stdlib.h>		/* malloc free */
#include <string.h>		/* memcpy */
#include <pthread.h>

#include <mpsse.h>

#include "loragw_spi.h"
#include "loragw_debug.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define DEBUG_MSG(str)				if(debug_spi)fprintf(stderr, str)
#define DEBUG_PRINTF(fmt, args...)	if(debug_spi)fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#define CHECK_NULL(a)				if(debug_spi){if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_SPI_ERROR;}}else{if(a==NULL){return LGW_SPI_ERROR;}}

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define READ_ACCESS		0x00
#define WRITE_ACCESS	0x80

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */
static pthread_mutex_t mx_spi = PTHREAD_MUTEX_INITIALIZER;

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

/* SPI initialization and configuration */
int lgw_spi_open(void **spi_target_ptr) {
	struct mpsse_context *mpsse = NULL;
	int a, b;
	
	/* check input variables */
	CHECK_NULL(spi_target_ptr); /* cannot be null, must point on a void pointer (*spi_target_ptr can be null) */
	
	/* try to open the first available FTDI device matching VID/PID parameters */
	mpsse = OpenIndex(VID,PID,SPI0, SIX_MHZ, MSB, IFACE_A, NULL, NULL, 0);
	if (mpsse == NULL) {
		DEBUG_MSG("ERROR: MPSSE OPEN FUNCTION RETURNED NULL\n");
		return LGW_SPI_ERROR;
	}
	if (mpsse->open != 1) {
		DEBUG_MSG("ERROR: MPSSE OPEN FUNCTION FAILED\n");
		return LGW_SPI_ERROR;
	}
	
#ifdef _MULTITECH_H_
	/* toggle pin ADBUS5 of the FT232H */
	/* On the MTAC LORA, it resets the SX1301 */
	a = PinLow(mpsse, GPIOL1);
	b = PinHigh(mpsse, GPIOL1);
#else
	/* toggle pin ADBUS5 of the FT2232H */
	/* On the Semtech reference board, it resets the SX1301 */
	a = PinHigh(mpsse, GPIOL1);
	b = PinLow(mpsse, GPIOL1);
#endif
	if ((a != MPSSE_OK) || (b != MPSSE_OK)) {
		DEBUG_MSG("ERROR: IMPOSSIBLE TO TOGGLE GPIOL1/ADBUS5\n");
		return LGW_SPI_ERROR;
	}
	
	DEBUG_PRINTF("SPI port opened and configured ok\ndesc: %s\nPID: 0x%04X\nVID: 0x%04X\nclock: %d\nLibmpsse version: 0x%02X\n", GetDescription(mpsse), GetPid(mpsse), GetVid(mpsse), GetClock(mpsse), Version());
	*spi_target_ptr = (void *)mpsse;
	return LGW_SPI_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* SPI release */
int lgw_spi_close(void *spi_target) {
	struct mpsse_context *mpsse = spi_target;
	
	/* check input variables */
	CHECK_NULL(spi_target);
	
	Close(mpsse);
	
	/* close return no status, assume success (0_o) */
	return LGW_SPI_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple write */
/* transaction time: .6 to 1 ms typically */
int lgw_spi_w(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t data) {
	struct mpsse_context *mpsse = spi_target;
	uint8_t out_buf[3];
    uint8_t command_size;
	int a, b, c;
	
	/* check input variables */
	CHECK_NULL(spi_target);
	if ((address & 0x80) != 0) {
		DEBUG_MSG("WARNING: SPI address > 127\n");
	}
	
    /* prepare frame to be sent */
    if (spi_mux_mode == LGW_SPI_MUX_MODE1) {
        out_buf[0] = spi_mux_target;
        out_buf[1] = WRITE_ACCESS | (address & 0x7F);
        out_buf[2] = data;
        command_size = 3;
    } else {
        out_buf[0] = WRITE_ACCESS | (address & 0x7F);
        out_buf[1] = data;
        command_size = 2;
    }
	
	/* lock USB bus */
	pthread_mutex_lock(&mx_spi);

	/* MPSSE transaction */
	a = Start(mpsse);
	b = FastWrite(mpsse, (char *)out_buf, command_size);
	c = Stop(mpsse);

	/* unlock USB bus */
	pthread_mutex_unlock(&mx_spi);
	
	/* determine return code */
	if ((a != MPSSE_OK) || (b != MPSSE_OK) || (c != MPSSE_OK)) {
		DEBUG_MSG("ERROR: SPI WRITE FAILURE\n");
		exit(255);
		//return LGW_SPI_ERROR;
	} else {
		DEBUG_MSG("Note: SPI write success\n");
		return LGW_SPI_SUCCESS;
	}
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple read (using Transfer function) */
/* transaction time: 1.1 to 2 ms typically */
int lgw_spi_r(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data) {
	struct mpsse_context *mpsse = spi_target;
	uint8_t out_buf[3];
    uint8_t command_size;
	uint8_t *in_buf = NULL;
	int a, b;
	
	/* check input variables */
	CHECK_NULL(spi_target);
	if ((address & 0x80) != 0) {
		DEBUG_MSG("WARNING: SPI address > 127\n");
	}
	CHECK_NULL(data);
	
    /* prepare frame to be sent */
    if (spi_mux_mode == LGW_SPI_MUX_MODE1) {
        out_buf[0] = spi_mux_target;
        out_buf[1] = READ_ACCESS | (address & 0x7F);
        out_buf[2] = 0x00;
        command_size = 3;
    } else {
        out_buf[0] = READ_ACCESS | (address & 0x7F);
        out_buf[1] = 0x00;
        command_size = 2;
    }
	
	/* lock USB bus */
	pthread_mutex_lock(&mx_spi);

	/* MPSSE transaction */
	a = Start(mpsse);
	in_buf = (uint8_t *)Transfer(mpsse, (char *)out_buf, command_size);
	b = Stop(mpsse);
	
	/* unlock USB bus */
	pthread_mutex_unlock(&mx_spi);

	/* determine return code */
	if ((in_buf == NULL) || (a != MPSSE_OK) || (b != MPSSE_OK)) {
		DEBUG_MSG("ERROR: SPI READ FAILURE\n");
		exit(255);
#if 0
		if (in_buf != NULL) {
			free(in_buf);
		}
		return LGW_SPI_ERROR;
#endif
	} else {
		DEBUG_MSG("Note: SPI read success\n");
		*data = in_buf[1];
		free(in_buf);
		return LGW_SPI_SUCCESS;
	}
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Burst (multiple-byte) write */
/* transaction time: 3.7ms for 2500 data bytes @6MHz, 1kB chunks */
/* transaction time: 0.5ms for 16 data bytes @6MHz, 1kB chunks */
int lgw_spi_wb(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
	struct mpsse_context *mpsse = spi_target;
	uint8_t command[2];
    uint8_t command_size;
	uint8_t *out_buf = NULL;
	int size_to_do, buf_size, chunk_size, offset;
	int a=0, b=0, c=0;
	int i;
	
	/* check input parameters */
	CHECK_NULL(spi_target);
	if ((address & 0x80) != 0) {
		DEBUG_MSG("WARNING: SPI address > 127\n");
	}
	CHECK_NULL(data);
	if (size == 0) {
		DEBUG_MSG("ERROR: BURST OF NULL LENGTH\n");
		return LGW_SPI_ERROR;
	}
	
    /* prepare command bytes */
    if (spi_mux_mode == LGW_SPI_MUX_MODE1) {
        command[0] = spi_mux_target;
        command[1] = WRITE_ACCESS | (address & 0x7F);
        command_size = 2;
    } else {
        command[0] = WRITE_ACCESS | (address & 0x7F);
        command_size = 1;
    }
	size_to_do = size + command_size; /* add a byte for the address */
	
	/* allocate data buffer */
	buf_size = (size_to_do < LGW_BURST_CHUNK) ? size_to_do : LGW_BURST_CHUNK;
	out_buf = malloc(buf_size);
	if (out_buf == NULL) {
		DEBUG_MSG("ERROR: MALLOC FAIL\n");
		return LGW_SPI_ERROR;
	}
	
	/* lock USB bus */
	pthread_mutex_lock(&mx_spi);

	/* start MPSSE transaction */
	a = Start(mpsse);
	for (i=0; size_to_do > 0; ++i) {
		chunk_size = (size_to_do < LGW_BURST_CHUNK) ? size_to_do : LGW_BURST_CHUNK;
		if (i == 0) {
			/* first chunk, need to prepend the address */
			memcpy(out_buf, command, command_size);
			memcpy(out_buf+command_size, data, chunk_size-command_size);
		} else {
			/* following chunks, just copy the data */
			offset = (i * LGW_BURST_CHUNK) - command_size;
			memcpy(out_buf, data + offset, chunk_size);
		}
		b = FastWrite(mpsse, (char *)out_buf, chunk_size);
		size_to_do -= chunk_size; /* subtract the quantity of data already transferred */
	}
	c = Stop(mpsse);
	
	/* unlock USB bus */
	pthread_mutex_unlock(&mx_spi);

	/* deallocate data buffer */
	free(out_buf);
	
	/* determine return code (only the last FastWrite is checked) */
	if ((a != MPSSE_OK) || (b != MPSSE_OK) || (c != MPSSE_OK)) {
		DEBUG_MSG("ERROR: SPI BURST WRITE FAILURE\n");
		exit(255);
		//return LGW_SPI_ERROR;
	} else {
		DEBUG_MSG("Note: SPI burst write success\n");
		return LGW_SPI_SUCCESS;
	}
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Burst (multiple-byte) read (using FastWrite & FastRead functions) */
/* transaction time: 7-12ms for 2500 data bytes @6MHz, 1kB chunks */
/* transaction time: 2ms for 16 data bytes @6MHz, 1kB chunks */
int lgw_spi_rb(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data, uint16_t size) {
	struct mpsse_context *mpsse = spi_target;
	uint8_t command[2];
    uint8_t command_size;
	int size_to_do, chunk_size, offset;
	int a=0, b=0, c=0, d=0;
	int i;
	
	/* check input parameters */
	CHECK_NULL(spi_target);
	if ((address & 0x80) != 0) {
		DEBUG_MSG("WARNING: SPI address > 127\n");
	}
	CHECK_NULL(data);
	if (size == 0) {
		DEBUG_MSG("ERROR: BURST OF NULL LENGTH\n");
		return LGW_SPI_ERROR;
	}
	
	/* prepare command bytes */
    if (spi_mux_mode == LGW_SPI_MUX_MODE1) {
        command[0] = spi_mux_target;
        command[1] = READ_ACCESS | (address & 0x7F);
        command_size = 2;
    } else {
        command[0] = READ_ACCESS | (address & 0x7F);
        command_size = 1;
    }
	size_to_do = size;
	
	/* lock USB bus */
	pthread_mutex_lock(&mx_spi);

	/* start MPSSE transaction */
	a = Start(mpsse);
	b = FastWrite(mpsse, (char *)&command, command_size);
	for (i=0; size_to_do > 0; ++i) {
		chunk_size = (size_to_do < LGW_BURST_CHUNK) ? size_to_do : LGW_BURST_CHUNK;
		offset = i * LGW_BURST_CHUNK;
		c = FastRead(mpsse, (char *)(data + offset), chunk_size);
		size_to_do -= chunk_size; /* subtract the quantity of data already transferred */
	}
	d = Stop(mpsse);
	
	/* unlock USB bus */
	pthread_mutex_unlock(&mx_spi);

	/* determine return code (only the last FastRead is checked) */
	if ((a != MPSSE_OK) || (b != MPSSE_OK) || (c != MPSSE_OK) || (d != MPSSE_OK)) {
		DEBUG_MSG("ERROR: SPI BURST READ FAILURE\n");
		exit(255);
		//return LGW_SPI_ERROR;
	} else {
		DEBUG_MSG("Note: SPI burst read success\n");
		return LGW_SPI_SUCCESS;
	}
}

/* --- EOF ------------------------------------------------------------------ */
