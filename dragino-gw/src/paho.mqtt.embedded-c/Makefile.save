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

.PHONY: clean 

# assume this is normally run in the main Paho directory
ifndef srcdir
  srcdir = MQTTPacket/src
endif

ifndef blddir
  blddir = build/output
endif

SOURCE_FILES_C = $(srcdir)/*.c

# The names of libraries to be built
MQTT_EMBED_LIB_C = paho-embed-mqtt3c

CC ?= gcc

EMBED_MQTTLIB_C_TARGET = ${blddir}/lib${MQTT_EMBED_LIB_C}.so.1

CFLAGS_SO = -g -fPIC -shared -O2 -Wall -DLINUX_SO

all: ${EMBED_MQTTLIB_C_TARGET}
	
${EMBED_MQTTLIB_C_TARGET}: ${SOURCE_FILES_C} 
	${CC} ${CFLAGS_SO} -o $@ $^ 
	-ln -s lib${MQTT_EMBED_LIB_C}.so.1 ${blddir}/lib${MQTT_EMBED_LIB_C}.so

clean:
	rm -rf ${blddir}/*
	
