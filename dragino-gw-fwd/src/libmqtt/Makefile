
.PHONY: clean 

SOURCE_FILES_C = *.c

# The names of libraries to be built
MQTT_EMBED_LIB_C = pahomqtt3c

CC ?= gcc

EMBED_MQTTLIB_C_TARGET = lib${MQTT_EMBED_LIB_C}.so

CFLAGS_SO = $(CFLAGS) -fPIC -shared -O2 -Wall -DLINUX_SO -DREVERSED

all: ${EMBED_MQTTLIB_C_TARGET}
	
${EMBED_MQTTLIB_C_TARGET}: ${SOURCE_FILES_C} 
	${CC} ${CFLAGS_SO} -o $@ $^  -lpthread

clean:
	rm -rf ${EMBED_MQTTLIB_C_TARGET}
	
