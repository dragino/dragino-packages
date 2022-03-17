### Application-specific constants

APP_NAME := util_ftdi_test

### Environment constants 

LGW_PATH ?= ../libloragw
ARCH ?=
CROSS_COMPILE ?=

### External constant definitions

include $(LGW_PATH)/library.cfg

### Constant symbols

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

CFLAGS=-O2 -Wall -Wextra -std=c99 -Iinc -I.

OBJDIR = obj

### Constants for LoRa concentrator HAL library
# List the library sub-modules that are used by the application

LGW_INC = $(LGW_PATH)/inc/config.h
LGW_INC += $(LGW_PATH)/inc/loragw_reg.h
LGW_INC += $(LGW_PATH)/inc/loragw_fpga.h

### Linking options

LIBS := -lpthread -lusb-1.0 -lftdi1 -lmpsse

### General build targets

all: $(APP_NAME)

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(APP_NAME)

### HAL library (do no force multiple library rebuild even with 'make -B')

$(LGW_PATH)/inc/config.h:
	@if test ! -f $@; then \
	$(MAKE) all -C $(LGW_PATH); \
	fi

$(LGW_PATH)/libloragw.a: $(LGW_INC)
	@if test ! -f $@; then \
	$(MAKE) all -C $(LGW_PATH); \
	fi

### Main program compilation and assembly
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/$(APP_NAME).o: src/$(APP_NAME).c| $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(APP_NAME): $(OBJDIR)/$(APP_NAME).o 
	$(CC) $< -o $@ $(LIBS) 

### EOF
