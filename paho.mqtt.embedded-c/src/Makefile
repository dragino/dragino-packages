#*******************************************************************************
#  Copyright (c) 2009, 2014 IBM Corp.
# 
#  All rights reserved. This program and the accompanying materials
#  are made available under the terms of the Eclipse Public License v1.0
#  and Eclipse Distribution License v1.0 which accompany this distribution. 
# 
#  The Eclipse Public License is available at 
#     http://www.eclipse.org/legal/epl-v10.html
#  and the Eclipse Distribution License is available at 
#    http://www.eclipse.org/org/documents/edl-v10.php.
# 
#  Contributors:
#     Xiang Rong - 442039 Add makefile to Embedded C client
#*******************************************************************************/

# Note: on OS X you should install XCode and the associated command-line tools

SHELL = /bin/sh
.PHONY: clean, install, uninstall, html 

# assume this is normally run in the main Paho directory
ifndef srcdir
  srcdir = MQTTPacket/src
endif

ifndef blddir
  blddir = build/output
endif

ifndef prefix
	prefix = /usr/local
endif

ifndef exec_prefix
	exec_prefix = ${prefix}
endif

bindir = $(exec_prefix)/bin
includedir = $(prefix)/include
libdir = $(exec_prefix)/lib

SOURCE_FILES_C = $(srcdir)/*.c

HEADERS = $(srcdir)/*.h

# The names of libraries to be built
MQTT_EMBED_LIB_C = paho-embed-mqtt3c


# determine current platform
ifeq ($(OS),Windows_NT)
	OSTYPE = $(OS)
else
	OSTYPE = $(shell uname -s)
	MACHINETYPE = $(shell uname -m)
endif

CC ?= gcc

ifndef INSTALL
INSTALL = install
endif
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA =  $(INSTALL) -m 644

MAJOR_VERSION = 1
MINOR_VERSION = 0
VERSION = ${MAJOR_VERSION}.${MINOR_VERSION}

EMBED_MQTTLIB_C_TARGET = ${blddir}/lib${MQTT_EMBED_LIB_C}.so.${VERSION}


CCFLAGS_SO = -g -fPIC -Os -Wall -fvisibility=hidden -DLINUX_SO
FLAGS_EXE = -I ${srcdir}  -L ${blddir}

LDFLAGS_C = -shared -Wl,-soname,lib$(MQTT_EMBED_LIB_C).so.${MAJOR_VERSION}

all: ${EMBED_MQTTLIB_C_TARGET} 
	
clean:
	rm -rf ${blddir}/*
	
${EMBED_MQTTLIB_C_TARGET}: ${SOURCE_FILES_C} ${HEADERS_C}
	${CC} ${CCFLAGS_SO} -o $@ ${SOURCE_FILES_C} ${LDFLAGS_C}
	-ln -s lib$(MQTT_EMBED_LIB_C).so.${VERSION}  ${blddir}/lib$(MQTT_EMBED_LIB_C).so.${MAJOR_VERSION}
	-ln -s lib$(MQTT_EMBED_LIB_C).so.${MAJOR_VERSION} ${blddir}/lib$(MQTT_EMBED_LIB_C).so


strip_options:
	$(eval INSTALL_OPTS := -s)

install-strip: ${EMBED_MQTTLIB_C_TARGET} strip_options install

install: ${EMBED_MQTTLIB_C_TARGET} 
	$(INSTALL_DATA) ${INSTALL_OPTS} ${EMBED_MQTTLIB_C_TARGET} $(DESTDIR)${libdir}

	/sbin/ldconfig $(DESTDIR)${libdir}
	ln -s lib$(MQTT_EMBED_LIB_C).so.${MAJOR_VERSION} $(DESTDIR)${libdir}/lib$(MQTT_EMBED_LIB_C).so


uninstall:
	rm $(DESTDIR)${libdir}/lib$(MQTT_EMBED_LIB_C).so.${VERSION}

	/sbin/ldconfig $(DESTDIR)${libdir}
	rm $(DESTDIR)${libdir}/lib$(MQTT_EMBED_LIB_C).so


