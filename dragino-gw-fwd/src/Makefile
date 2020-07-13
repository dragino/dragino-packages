### Environment constants

ARCH ?=
CROSS_COMPILE ?=
export

### general build targets

.PHONY: all clean install install_conf sx1302hal fwd util_chip_id

all: sx1302hal fwd util_chip_id

sx1302hal: 
	$(MAKE) all -e -C $@

util_chip_id: sx1302hal
	$(MAKE) all -e -C $@

fwd: sx1302hal
	$(MAKE) all -e -C $@

clean:
	$(MAKE) clean -e -C sx1302hal
	$(MAKE) clean -e -C fwd
	$(MAKE) clean -e -C util_chip_id

install:
	$(MAKE) install -e -C sx1302hal
	$(MAKE) install -e -C fwd
	$(MAKE) install -e -C util_chip_id

install_conf:
	$(MAKE) install_conf -e -C fwd

### EOF
