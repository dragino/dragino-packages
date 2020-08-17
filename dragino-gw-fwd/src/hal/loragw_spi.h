/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Host specific functions to address the LoRa concentrator registers through a
    SPI interface.
    Single-byte read/write and burst read/write.
    Does not handle pagination.
    Could be used with multiple SPI ports in parallel (explicit file descriptor)

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


#ifndef _LORAGW_SPI_H
#define _LORAGW_SPI_H

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>        /* C99 types*/

#include "config.h"    /* library configuration options (dynamically generated) */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC CONSTANTS ----------------------------------------------------- */

#define SPI_DEV_PATH                "/dev/spidev1.0"

#define READ_ACCESS		            0x00
#define WRITE_ACCESS	            0x80

#define LGW_SPI_SUCCESS             0
#define LGW_SPI_ERROR               -1
#define LGW_BURST_CHUNK             1024

#define SPI_SPEED                   8000000

#define LGW_SPI_MUX_TARGET_SX1301   0x08
#define LGW_SPI_MUX_TARGET_SX127X   0x08

#define LGW_SPI_MUX_TARGET_SX1302   0x00
#define LGW_SPI_MUX_TARGET_RADIOA   0x01
#define LGW_SPI_MUX_TARGET_RADIOB   0x02

/* -------------------------------------------------------------------------- */
/* --- INTERNAL SHARED VARIABLES -------------------------------------------- */

void *lgw_spi_target = NULL; /*! generic pointer to the SPI device */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS PROTOTYPES ------------------------------------------ */

/**
@brief LoRa concentrator SPI setup (configure I/O and peripherals)
@param spi_target_ptr pointer on a generic pointer to SPI target (implementation dependant)
@return status of register operation (LGW_SPI_SUCCESS/LGW_SPI_ERROR)
*/

int lgw_spi_open(const char* spidev_path, void **spi_target_ptr);

int lgw_ft_spi_open(void **spi_target_ptr);

/**
@brief LoRa concentrator SPI close
@param spi_target generic pointer to SPI target (implementation dependant)
@return status of register operation (LGW_SPI_SUCCESS/LGW_SPI_ERROR)
*/

int lgw_spi_close(void *spi_target);

int lgw_ft_spi_close(void *spi_target);

/**
@brief LoRa concentrator SPI single-byte write
@param spi_target generic pointer to SPI target (implementation dependant)
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_SPI_SUCCESS/LGW_SPI_ERROR)
*/
int lgw_spi_w(void *spi_target, uint8_t spi_mux_target, uint16_t address, uint8_t data);

int lgw_ft_spi_w(void *spi_target, uint8_t spi_mux_target, uint8_t address, uint8_t data);

/**
@brief LoRa concentrator SPI single-byte read
@param spi_target generic pointer to SPI target (implementation dependant)
@param address 7-bit register address
@param data data byte to write
@return status of register operation (LGW_SPI_SUCCESS/LGW_SPI_ERROR)
*/
int lgw_spi_r(void *spi_target, uint8_t spi_mux_target, uint16_t address, uint8_t *data);

int lgw_ft_spi_r(void *spi_target, uint8_t spi_mux_target, uint8_t address, uint8_t *data);

/**
@brief LoRa concentrator SPI burst (multiple-byte) write
@param spi_target generic pointer to SPI target (implementation dependant)
@param address 7-bit register address
@param data pointer to byte array that will be sent to the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_SPI_SUCCESS/LGW_SPI_ERROR)
*/
int lgw_spi_wb(void *spi_target, uint8_t spi_mux_target, uint16_t address, const uint8_t *data, uint16_t size);

/**
@brief LoRa concentrator SPI burst (multiple-byte) read
@param spi_target generic pointer to SPI target (implementation dependant)
@param address 7-bit register address
@param data pointer to byte array that will be written from the LoRa concentrator
@param size size of the transfer, in byte(s)
@return status of register operation (LGW_SPI_SUCCESS/LGW_SPI_ERROR)
*/
int lgw_spi_rb(void *spi_target, uint8_t spi_mux_target, uint16_t address, uint8_t *data, uint16_t size);

/**
@brief sx127x ftdi device reset
@return status of register operation (LGW_SPI_SUCCESS/LGW_SPI_ERROR)
*/
int ftdi_sx127x_reset(void* spi_target, int invert);

#endif

/* --- EOF ------------------------------------------------------------------ */
