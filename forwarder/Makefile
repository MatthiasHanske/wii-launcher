#---------------------------------------------------------------------------------
# Priiloader-Forwarder: libogc + libfat, kein GRRLIB.
# data/app_booter.bin kopiert das Top-Level-Makefile vor dem Bau hierher.
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

TARGET		:=	forwarder
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	source

CFLAGS	= -Os -Wall $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS)
# printf/sprintf/snprintf -> Integer-Varianten (source/intprintf.c), spart ~40 KB.
LDFLAGS	=	$(MACHDEP) -Wl,-Map,$(notdir $@).map -Wl,--gc-sections \
		-Wl,--wrap=printf -Wl,--wrap=sprintf -Wl,--wrap=snprintf

LIBS	:=	-lfat -logc

LIBDIRS	:=

ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin)))

export LD	:=	$(CC)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SOURCES	:=	$(CFILES:.c=.o)
export OFILES	:=	$(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC)

export LIBPATHS	:=	-L$(LIBOGC_LIB)

.PHONY: $(BUILD) clean run

$(BUILD):
	@[ -f $(CURDIR)/data/app_booter.bin ] || { echo "data/app_booter.bin fehlt: 'make forwarder' im Repo-Root ausfuehren"; exit 1; }
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

run:
	wiiload $(TARGET).dol

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SOURCES) : $(HFILES)

# app_booter.bin -> app_booter_bin.h / app_booter.bin.o
%.bin.o	%_bin.h :	%.bin
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
