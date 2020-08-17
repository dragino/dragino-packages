### Environment constants

ARCH ?=
CROSS_COMPILE ?=
export

### general build targets

.PHONY: all clean install install_conf hal fwd 

all: hal fwd 

hal: 
	$(MAKE) all -e -C $@

fwd: hal
	$(MAKE) all -e -C $@

clean:
	$(MAKE) clean -e -C hal
	$(MAKE) clean -e -C fwd

install:
	$(MAKE) install -e -C hal
	$(MAKE) install -e -C fwd

install_conf:
	$(MAKE) install_conf -e -C fwd

### EOF
