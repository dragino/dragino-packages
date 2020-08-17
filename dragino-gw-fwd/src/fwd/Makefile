### Application-specific constants

APP_NAME := fwd

### Environment constants 

LGW_PATH ?= ../hal

VPATH += ../hal ../hal/lg02hal ../hal/sx1301hal ../hal/sx1302hal src inc

OBJDIR = obj

INCLUDES = $(wildcard inc/*.h)

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

CFLAGS_C := $(CFLAGS) -fPIC -Iinc -I. -I../hal -I../hal/lg02hal -I../hal/sx1301hal -I../hal/sx1302hal 

### Linking options
LIBS += -L$(LGW_PATH) -lloragw -lrt -lsqlite3 -lpthread -lm -lmpsse -lftdi1 -lusb-1.0

#LIBS += -L$(MQTT_PATH) -lpahomqtt3c -L$(TTN_PATH)/obj -lttn-gateway-connector -lprotobuf-c 

### General build targets

all: $(APP_NAME)

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(APP_NAME)

### Sub-modules compilation

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c $(INCLUDES) | $(OBJDIR)
	$(CC) -c $(CFLAGS_C) $< -o $@

### Main program compilation and assembly

$(APP_NAME): $(LGW_PATH)/libloragw.a $(OBJDIR)/parson.o $(OBJDIR)/base64.o $(OBJDIR)/jitqueue.o  $(OBJDIR)/ghost.o $(OBJDIR)/endianext.o $(OBJDIR)/semtech_serv.o $(OBJDIR)/service.o $(OBJDIR)/stats.o $(OBJDIR)/gwtraf_serv.o $(OBJDIR)/pkt_serv.o $(OBJDIR)/db.o $(OBJDIR)/utilities.o $(OBJDIR)/lgwmm.o $(OBJDIR)/aes.o $(OBJDIR)/cmac.o $(OBJDIR)/mac-header-decode.o $(OBJDIR)/loramac-crypto.o $(OBJDIR)/timersync.o $(OBJDIR)/gwcfg.o $(OBJDIR)/fwd.o
	$(CC) $^ -o $@ $(LIBS)

### EOF
