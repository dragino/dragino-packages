/*
License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Jac Kersing
*/

#include "config.h"
int debug_aux = DEBUG_AUX;
int debug_spi = DEBUG_SPI;
int debug_reg = DEBUG_REG;
int debug_hal = DEBUG_HAL;
int debug_gps = DEBUG_GPS;
int debug_gpio = DEBUG_GPIO;
int debug_lbt = DEBUG_LBT;

void lgw_debug_set(int aux, int spi, int reg, int hal, int gps, int gpio, int lbt) {
	debug_aux = aux;
	debug_spi = spi;
	debug_reg = reg;
	debug_hal = hal;
	debug_gps = gps;
	debug_gpio = gpio;
	debug_lbt = lbt;
}
