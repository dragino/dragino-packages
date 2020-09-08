
### Application-specific constants
APP_NAME := fwd

### Environment constants 

ARCH ?=
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

LGW_PATH ?= ../hal
MQTT_PATH ?= ../libmqtt

LCFLAGS = $(CFLAGS) -fPIC -Iinc -I. -I../hal -I../hal/lg02hal -I../hal/sx1301hal -I../hal/sx1302hal -I../libmqtt

OBJDIR = obj

INCLUDES = $(wildcard inc/*.h)

### linking options

LLIBS := -lloragw -lrt -lsqlite3 -lpthread -lm -lmpsse -L../libmqtt -lpahomqtt3c

### general build targets

all: $(APP_NAME)

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(APP_NAME)

### Sub-modules compilation

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c $(INCLUDES) | $(OBJDIR)
	$(CC) -g -c $(LCFLAGS) $< -o $@

### Main program compilation and assembly

$(APP_NAME): $(LGW_PATH)/libloragw.a $(OBJDIR)/parson.o $(OBJDIR)/base64.o $(OBJDIR)/jitqueue.o  $(OBJDIR)/ghost.o $(OBJDIR)/endianext.o $(OBJDIR)/semtech_serv.o $(OBJDIR)/service.o $(OBJDIR)/stats.o $(OBJDIR)/gwtraf_serv.o $(OBJDIR)/pkt_serv.o $(OBJDIR)/mqtt_serv.o $(OBJDIR)/db.o $(OBJDIR)/utilities.o $(OBJDIR)/lgwmm.o $(OBJDIR)/aes.o $(OBJDIR)/cmac.o $(OBJDIR)/mac-header-decode.o $(OBJDIR)/loramac-crypto.o $(OBJDIR)/timersync.o $(OBJDIR)/gwcfg.o $(OBJDIR)/fwd.o | $(OBJDIR)
	$(CC) -g -fPIE -L$(LGW_PATH) $^ -o $@ $(LLIBS)

### test programs

### EOF
