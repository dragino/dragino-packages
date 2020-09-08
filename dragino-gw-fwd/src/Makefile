### Environment constants

ARCH ?=
CROSS_COMPILE ?=
export

### general build targets

.PHONY: all clean install install_conf hal fwd libmqtt

all: hal libmqtt fwd 

hal: 
	$(MAKE) all -e -C $@

fwd: hal libmqtt
	$(MAKE) all -e -C $@

libmqtt: 
	$(MAKE) all -e -C $@

clean:
	$(MAKE) clean -e -C hal
	$(MAKE) clean -e -C fwd
	$(MAKE) clean -e -C libmqtt

### EOF
