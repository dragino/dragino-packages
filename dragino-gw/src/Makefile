### Environment constants 

ARCH ?=
CROSS_COMPILE ?=
export

### general build targets

all:
	$(MAKE) all -e -C paho.mqtt.embedded-c
	$(MAKE) all -e -C ttn-gateway-connector
	$(MAKE) all -e -C libloragw
	$(MAKE) all -e -C dragino-gw-fwd

clean:
	$(MAKE) clean -e -C paho.mqtt.embedded-c
	$(MAKE) clean -e -C ttn-gateway-connector
	$(MAKE) clean -e -C libloragw
	$(MAKE) clean -e -C dragino-gw-fwd

### EOF
