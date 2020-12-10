### get external defined data

LIBLORAGW_VERSION := `cat ../VERSION`
include library.cfg

### constant symbols

ARCH ?=
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

VPATH := ./ lg02hal sx1301hal sx1302hal

LCFLAGS = $(CFLAGS) -fPIC -Ilg02hal -Isx1301hal -Isx1302hal -I.
LCFLAGS_SO = $(LCFLAGS) -shared -O2 -Wall -DLINUX_SO

OBJDIR = obj
INCLUDES = $(wildcard *.h lg02hal/*.h sx1301hal/*.h sx1302hal/*.h)

fobjs = $(OBJDIR)/loragw_hal.o $(OBJDIR)/loragw_gps.o $(OBJDIR)/loragw_reg.o $(OBJDIR)/loragw_spi.native.o $(OBJDIR)/loragw_spi.ftdi.o $(OBJDIR)/loragw_aux.o $(OBJDIR)/loragw_lbt.o $(OBJDIR)/gpio.o $(OBJDIR)/loragw_i2c.o $(OBJDIR)/loragw_debug.o $(OBJDIR)/tinymt32.o

sx1302objs = $(OBJDIR)/loragw_hal_sx1302.o $(OBJDIR)/loragw_reg_sx1302.o $(OBJDIR)/loragw_sx1250.o $(OBJDIR)/loragw_sx125x.o $(OBJDIR)/loragw_sx1302.o $(OBJDIR)/loragw_cal.o $(OBJDIR)/loragw_hal.o $(OBJDIR)/loragw_stts751.o $(OBJDIR)/loragw_sx1302_timestamp.o $(OBJDIR)/loragw_sx1302_rx.o

sx1301objs = $(OBJDIR)/loragw_hal_sx1301.o $(OBJDIR)/loragw_radio_sx1301.o $(OBJDIR)/loragw_reg_sx1301.o 

lg02objs = $(OBJDIR)/loragw_hal_lg02.o $(OBJDIR)/loragw_sx127x_radio.o 

objs = $(fobjs) $(sx1302objs) $(sx1301objs) $(lg02objs)

### linking options

### general build targets

all: libloragw.so tx_test rx_test

clean:
	rm -f libloragw.a
	rm -f test_loragw_*
	rm -f $(OBJDIR)/*.o
	rm -f inc/config.h

### transpose library.cfg into a C header file : config.h

config.h: ../VERSION library.cfg
	@echo "*** Checking libloragw library configuration ***"
	@rm -f $@
	#File initialization
	@echo "#ifndef _LORAGW_CONFIGURATION_H" >> $@
	@echo "#define _LORAGW_CONFIGURATION_H" >> $@
	# Release version
	@echo "Release version   : $(LIBLORAGW_VERSION)"
	@echo "	#define LIBLORAGW_VERSION	"\"$(LIBLORAGW_VERSION)\""" >> $@
	# Debug options
	@echo "	#define DEBUG_AUX	$(DEBUG_AUX)" >> $@
	@echo "	#define DEBUG_SPI	$(DEBUG_SPI)" >> $@
	@echo "	#define DEBUG_I2C	$(DEBUG_I2C)" >> $@
	@echo "	#define DEBUG_REG	$(DEBUG_REG)" >> $@
	@echo "	#define DEBUG_HAL	$(DEBUG_HAL)" >> $@
	@echo "	#define DEBUG_LBT	$(DEBUG_LBT)" >> $@
	@echo "	#define DEBUG_GPS	$(DEBUG_GPS)" >> $@
	@echo "	#define DEBUG_RAD	$(DEBUG_RAD)" >> $@
	@echo "	#define DEBUG_CAL	$(DEBUG_CAL)" >> $@
	@echo "	#define DEBUG_SX1302	$(DEBUG_SX1302)" >> $@
	# end of file
	@echo "#endif" >> $@
	@echo "*** Configuration seems ok ***"

### library module target

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.c $(INCLUDES) config.h | $(OBJDIR)
	$(CC) -g -c $(LCFLAGS) $< -o $@

$(OBJDIR)/loragw_sx1301_hal.o: sx1301hal/loragw_sx1301_hal.c $(INCLUDES) sx1301hal/arb_fw.var sx1301hal/agc_fw.var sx1301hal/cal_fw.var config.h | $(OBJDIR)
	$(CC) -g -c $(LCFLAGS) $< -o $@

$(OBJDIR)/loragw_sx1302_hal.o: sx1302hal/loragw_sx1302_hal.c $(INCLUDES) sx1302hal/arb_fw.var sx1302hal/cal_fw.var sx1302hal/agc_fw_sx1250.var sx1302hal/agc_fw_sx1250.val config.h | $(OBJDIR)
	$(CC) -g -c $(LCFLAGS) $< -o $@

$(OBJDIR)/loragw_lg02_hal.o: sx1302hal/loragw_lg02_hal.c $(INCLUDES) config.h | $(OBJDIR)
	$(CC) -g -c $(LCFLAGS) $< -o $@

$(OBJDIR)/loragw_hal.o: loragw_hal.c $(INCLUDES) config.h | $(OBJDIR)
	$(CC) -g -c $(LCFLAGS) $< -o $@

### static library
libloragw.so: $(objs)
	${CC} ${LCFLAGS_SO} -o $@ $^

### test programs
tx_test: test/test_loragw_hal_tx.c libloragw.so
	$(CC) -fPIE $(LCFLAGS) -L. $< -o $@ -lloragw -lmpsse

rx_test: test/test_loragw_hal_rx.c libloragw.so
	$(CC) -fPIE $(LCFLAGS) -L. $< -o $@ -lloragw -lmpsse

### EOF
