### Application-specific constants

APP_NAME := fwd

### Environment constants 

LGW_PATH ?= ../sx1302hal

ARCH ?=
CROSS_COMPILE ?=

OBJDIR = obj
INCLUDES = $(wildcard inc/*.h)

### External constant definitions
# must get library build option to know if mpsse must be linked or not

include $(LGW_PATH)/library.cfg
RELEASE_VERSION := $(shell cat VERSION)

### Constant symbols

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

CFLAGS_C := -fPIC -O2 -Wall -Wextra -Iinc -I.
VFLAG := -D VERSION_STRING="\"$(RELEASE_VERSION)\""

### Constants for Lora concentrator HAL library
# List the library sub-modules that are used by the application

LGW_INC =
ifneq ($(wildcard $(LGW_PATH)/inc/config.h),)
  # only for HAL version 1.3 and beyond
  LGW_INC += $(LGW_PATH)/inc/config.h
endif
LGW_INC += $(LGW_PATH)/inc/loragw_hal.h
LGW_INC += $(LGW_PATH)/inc/loragw_gps.h

### Linking options

LIBS += -lloragw -lrt -lsqlite3 -lpthread -lm

#LIBS += -L$(MQTT_PATH) -lpahomqtt3c -L$(TTN_PATH)/obj -lttn-gateway-connector -lprotobuf-c 

CFLAGS ?=
LDFLAGS ?=

### General build targets

all: $(APP_NAME)

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(APP_NAME)

### Sub-modules compilation

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c $(INCLUDES) | $(OBJDIR)
	$(CC) -c $(CFLAGS_C) -I$(LGW_PATH)/inc $< -o $@

### Main program compilation and assembly

$(OBJDIR)/$(APP_NAME).o: src/$(APP_NAME).c $(LGW_INC) $(INCLUDES) | $(OBJDIR)
	$(CC) -c $(CFLAGS_C) $(VFLAG) -I$(LGW_PATH)/inc $< -o $@

$(APP_NAME): $(OBJDIR)/$(APP_NAME).o $(LGW_PATH)/libloragw.a $(OBJDIR)/parson.o $(OBJDIR)/base64.o $(OBJDIR)/jitqueue.o  $(OBJDIR)/ghost.o $(OBJDIR)/endianext.o $(OBJDIR)/semtech_serv.o $(OBJDIR)/service.o $(OBJDIR)/stats.o $(OBJDIR)/gwtraf_serv.o $(OBJDIR)/pkt_serv.o $(OBJDIR)/db.o $(OBJDIR)/utilities.o $(OBJDIR)/lgwmm.o $(OBJDIR)/aes.o $(OBJDIR)/cmac.o $(OBJDIR)/mac-header-decode.o $(OBJDIR)/loramac-crypto.o $(OBJDIR)/timersync.o $(OBJDIR)/gwcfg.o
	$(CC) -L$(LGW_PATH) $< $(OBJDIR)/parson.o $(OBJDIR)/base64.o $(OBJDIR)/jitqueue.o $(OBJDIR)/timersync.o $(OBJDIR)/ghost.o $(OBJDIR)/endianext.o $(OBJDIR)/semtech_serv.o $(OBJDIR)/service.o $(OBJDIR)/stats.o $(OBJDIR)/gwtraf_serv.o $(OBJDIR)/pkt_serv.o $(OBJDIR)/db.o $(OBJDIR)/utilities.o $(OBJDIR)/lgwmm.o $(OBJDIR)/aes.o $(OBJDIR)/cmac.o $(OBJDIR)/mac-header-decode.o $(OBJDIR)/loramac-crypto.o $(OBJDIR)/gwcfg.o -o $@ $(LIBS)

### EOF
