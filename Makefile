export DEVKITPRO ?= /opt/devkitpro
export DEVKITPPC ?= /opt/devkitpro/devkitPPC
export PATH := $(DEVKITPRO)/tools/bin:$(DEVKITPPC)/bin:$(PATH)

.PHONY: all launcher forwarder booter test clean

all: launcher forwarder booter

launcher: booter
	@mkdir -p launcher/data
	cp -p booter/app_booter.bin launcher/data/app_booter.bin
	$(MAKE) -C launcher

forwarder:
	@echo "forwarder: not yet"

booter:
	$(MAKE) -C booter

test:
	$(MAKE) -C tests test

clean:
	-$(MAKE) -C launcher clean
	-$(MAKE) -C booter clean
	rm -f launcher/data/app_booter.bin
	$(MAKE) -C tests clean
