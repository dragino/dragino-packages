# In case of windows, use os_cygwin=yes setting in the configuration file (smsd.conf).

# Select your setup size:
CFLAGS = -D NUMBER_OF_MODEMS=64

# Uncomment for Solaris
# CFLAGS += -D SOLARIS
# This might be also needed for Solaris:
# CC=gcc

# Comment this out, to enable statistics
CFLAGS += -D NOSTATS

# Comment this out if iconv is not available on the system
#CFLAGS += -D USE_ICONV

# Uncomment this is iconv is used and it's not included in libc:
#LFLAGS += -liconv

# Uncomment this to force smsd to run in terminal mode
#CFLAGS += -D TERMINAL

# Uncomment this to disable usage of inet socket
#CFLAGS += -D DISABLE_INET_SOCKET

# Uncomment to add debug info
# CFLAGS += -ggdb -O0

# The following option is only used by developers
# CFLAGS += -D DEBUGMSG

CFLAGS += -W -Wall

# The following enables logfile size more than 2GB
CFLAGS += -D_FILE_OFFSET_BITS=64

# Use the following only on GNU/Linux and only if you need ps listing like "smsd: MAINPROCESS" and "smsd: GSM1"
# CFLAGS += -D USE_LINUX_PS_TRICK

all: smsd 

smsd: smsd.c extras.o locking.o cfgfile.o logging.o alarm.o smsd_cfg.o charset.o stats.o blacklist.o whitelist.o modeminit.o pdu.o

ifneq (,$(findstring SOLARIS,$(CFLAGS)))
ifeq (,$(findstring DISABLE_INET_SOCKET,$(CFLAGS)))
override LFLAGS += -lsocket -lnsl
endif
endif

ifneq (,$(findstring NOSTATS,$(CFLAGS)))
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)
else 
	$(CC) `mm-config --cflags` $(CFLAGS) -o $@ $^ `mm-config --ldflags --libs` $(LFLAGS)
endif

clean:
	rm -f *.o smsd *.exe *~
