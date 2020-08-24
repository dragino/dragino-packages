/*
License: Revised BSD License, see LICENSE.TXT file include in the project
*/

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdio.h>		/* printf fprintf */
#include <stdlib.h>		/* malloc free */
#include <string.h>		/* memcpy */
#include <pthread.h>

#include <mpsse.h>

#include "loragw_spi.h"
#include "loragw_hal.h"
#include "loragw_aux.h"

#define  VID          0x0403
#define  PID          0x6010

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_SPI == 1
    #define DEBUG_MSG(str)                fprintf(stderr, str)
    #define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
    #define CHECK_NULL(a)                if(a==NULL){fprintf(stderr,"%s:%d: ERROR~ NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_SPI_ERROR;}
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
    #define CHECK_NULL(a)                if(a==NULL){return LGW_SPI_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */
static pthread_mutex_t mx_spi = PTHREAD_MUTEX_INITIALIZER;

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

/* SPI initialization and configuration */
int lgw_ft_spi_open(void **spi_target_ptr) {
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
	
	/*it resets the SX1276 */
	b = PinLow(mpsse, GPIOL0);
    wait_ms(10);
	a = PinHigh(mpsse, GPIOL0);

	if ((a != MPSSE_OK) || (b != MPSSE_OK)) {
		DEBUG_MSG("ERROR: IMPOSSIBLE TO TOGGLE GPIOL1/ADBUS5\n");
		return LGW_SPI_ERROR;
	}
	

    DEBUG_PRINTF("SPI port opened and configured ok!\ndesc:%s, PID:0x%04X, VID:0x%04X, clock:%d, Libmpsse version: 0x%02X\n", GetDescription(mpsse), GetPid(mpsse), GetVid(mpsse), GetClock(mpsse), Version());

	*spi_target_ptr = (void *)mpsse;

	return LGW_SPI_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* SPI release */
int lgw_ft_spi_close(void *spi_target) {
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
int lgw_ft_spi_w(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t data) {
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
    /*
	a = Start(mpsse);
	b = FastWrite(mpsse, (char *)out_buf, command_size);
	c = Stop(mpsse);
    */
	a = Start(mpsse);
	Transfer(mpsse, (char *)out_buf, command_size);
	b = Stop(mpsse);

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
int lgw_ft_spi_r(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data) {
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

int ftdi_sx127x_reset(void* spi_target, bool invert) {
	struct mpsse_context *mpsse = spi_target;
    int a, b;

    if (!invert) {
        b = PinLow(mpsse, GPIOL0);
        wait_ms(10);
        a = PinHigh(mpsse, GPIOL0);
    } else {
        a = PinHigh(mpsse, GPIOL0);
        wait_ms(10);
        b = PinLow(mpsse, GPIOL0);
    }
	if ((a != MPSSE_OK) || (b != MPSSE_OK)) {
		DEBUG_MSG("ERROR: reset sx127x error!\n");
		return LGW_SPI_ERROR;
	}
    return LGW_SPI_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
