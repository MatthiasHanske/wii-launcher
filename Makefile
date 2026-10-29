export DEVKITPRO ?= /opt/devkitpro
export DEVKITPPC ?= /opt/devkitpro/devkitPPC
export PATH := $(DEVKITPRO)/tools/bin:$(DEVKITPPC)/bin:$(PATH)

.PHONY: all launcher forwarder booter test clean

all: launcher forwarder booter

launcher:
	$(MAKE) -C launcher

forwarder:
	@echo "forwarder: not yet"

booter:
	@echo "booter: not yet"

test:
	$(MAKE) -C tests test

clean:
	-$(MAKE) -C launcher clean
	$(MAKE) -C tests clean
