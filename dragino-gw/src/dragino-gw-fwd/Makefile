### Application-specific constants

APP_NAME := dragino_gw_fwd

### Environment constants 

LGW_PATH ?= ../libloragw
MQTT_PATH ?= ../libmqtt
TTN_PATH ?= ../libttn

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
TTN_INC := -I$(TTN_PATH) 

### Linking options

LIBS:=

ifeq ($(PLATFORM),imst_rpi)
  CFLAGS_C += -DRPI
endif

ifeq ($(CFG_SPI),native)
  LIBS += -lloragw -lrt -lpthread -lm
else ifeq ($(CFG_SPI),ftdi)
  LIBS += -lloragw -lrt -lpthread -lmpsse -lm -lusb-1.0
else ifeq ($(CFG_SPI),mac)
    LIBS += -lloragw -lpthread -lmpsse -lm
else
  # keep compatibility with SX1301 HAL version 1.2.x and bellow
  ifeq ($(LGW_PHY),native)
    LIBS += -lloragw -lrt -lpthread -lm
  else ifeq ($(LGW_PHY),ftdi)
    LIBS += -lloragw -lrt -lpthread -lmpsse -lm
  else ifeq ($(LGW_PHY),mac)
    LIBS += -lloragw -lpthread -lmpsse -lm
  else
    $(error [error] Can't find configuration for SPI phy)
  endif
endif

LIBS += -L$(MQTT_PATH) -lpahomqtt3c -L$(TTN_PATH)/obj -lttn-gateway-connector -lprotobuf-c 

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
	$(CC) -c $(CFLAGS_C) $(CFLAGS) -I$(LGW_PATH)/inc $(TTN_INC) $< -o $@

### Main program compilation and assembly

$(OBJDIR)/$(APP_NAME).o: src/$(APP_NAME).c $(LGW_INC) $(INCLUDES) | $(OBJDIR)
	$(CC) -c $(CFLAGS_C) $(VFLAG) $(CFLAGS) -I$(LGW_PATH)/inc $(TTN_INC) $< -o $@

$(APP_NAME): $(OBJDIR)/$(APP_NAME).o $(LGW_PATH)/libloragw.a $(OBJDIR)/parson.o $(OBJDIR)/base64.o $(OBJDIR)/jitqueue.o $(OBJDIR)/timersync.o $(OBJDIR)/ghost.o $(OBJDIR)/endianext.o $(OBJDIR)/semtech_transport.o $(OBJDIR)/ttn_transport.o $(OBJDIR)/stats.o $(OBJDIR)/gwtraf_transport.o $(OBJDIR)/transport.o 
	$(CC) -L$(LGW_PATH) $< $(OBJDIR)/parson.o $(OBJDIR)/base64.o $(OBJDIR)/jitqueue.o $(OBJDIR)/timersync.o $(OBJDIR)/ghost.o $(OBJDIR)/endianext.o $(OBJDIR)/semtech_transport.o $(OBJDIR)/ttn_transport.o $(OBJDIR)/gwtraf_transport.o $(OBJDIR)/stats.o $(OBJDIR)/transport.o -o $@ $(LIBS)


### EOF
