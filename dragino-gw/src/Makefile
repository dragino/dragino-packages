### Environment constants 

ARCH ?=
CROSS_COMPILE ?=
export

### general build targets

all:
	$(MAKE) all -e -C libmqtt
	$(MAKE) all -e -C libttn
	$(MAKE) all -e -C libloragw
	$(MAKE) all -e -C dragino-gw-fwd

clean:
	$(MAKE) clean -e -C libmqtt
	$(MAKE) clean -e -C libttn
	$(MAKE) clean -e -C libloragw
	$(MAKE) clean -e -C dragino-gw-fwd

### EOF
