# This makefile defines how to build 'unix-spi-host', a POSIX EZSP-SPI ZigBee host targeted for the Raspberry Pi
#

ZNET_DIR = .

ifneq ("$(wildcard )","")
  BASE_DIR = $(ZNET_DIR)/
else
  BASE_DIR = $(ZNET_DIR)
endif

$(info ZNET_DIR = $(ZNET_DIR))
$(info BASE_DIR = $(BASE_DIR))

MAKEFILE = $(CURDIR)/$(firstword $(MAKEFILE_LIST))
MAKEFILE_DEBUG_VARS = $(MAKEFILE)-debug-vars
MAKEFILE_DEBUG_RULES = $(MAKEFILE)-debug-rules

-include $(MAKEFILE_DEBUG_VARS)

APP_NAME = unix-spi-host
APP_DIR = $(ZNET_DIR)/app/$(APP_NAME)
OUTPUT_DIR = $(APP_DIR)/build
EXECUTABLE_DIR = $(OUTPUT_DIR)/exe
EXECUTABLE_OUTPUT= $(OUTPUT_DIR)/exe/$(APP_NAME)

E_CC ?= gcc
CC = $(E_CC)
LD = $(E_CC)
SHELL = /bin/sh

CC_INCLUDE_DIRS= \
  -I$(ZNET_DIR) \
  -I$(ZNET_DIR)/app/host \
  -I$(ZNET_DIR)/app/test \
  -I$(ZNET_DIR)/app/util \
  -I$(ZNET_DIR)/app/util/common \
  -I$(ZNET_DIR)/app/util/ezsp \
  -I$(ZNET_DIR)/app/util/serial \
  -I$(ZNET_DIR)/app/util/zigbee-framework \
  -I$(ZNET_DIR)/stack \
  -I$(ZNET_DIR)/stack/core \
  -I$(ZNET_DIR)/stack/framework \
  -I$(ZNET_DIR)/stack/ip \
  -I$(ZNET_DIR)/stack/platform/micro/generic \
  -I$(BASE_DIR) \
  -I$(BASE_DIR)/hal \
  -I$(BASE_DIR)/hal/micro/generic \
  -I$(BASE_DIR)/hal/micro/unix/host \
  -I$(BASE_DIR)/hal/micro/unix/simulation \
  -I$(BASE_DIR)/phy \
  -I$(APPFRAMEWORK_DIR)/util \

CC_DEFINES= \
  -DPLATFORM_HEADER=\"$(BASE_DIR)/hal/micro/unix/compiler/gcc.h\" \
  -DHAL_MICRO \
  -DUNIX \
  -DUNIX_HOST \
  -DEZSP_HOST \
  -DEZSP_SPI \
  -DPHY_NULL \
  -DBOARD_HOST \
  -DCONFIGURATION_HEADER=\"$(ZNET_DIR)/app/test/ezsp-host-configuration.h\" \
  -DAPP_SERIAL=0 \
  -DEMBER_SERIAL0_BLOCKING \
  -DEMBER_SERIAL0_MODE=EMBER_SERIAL_FIFO \
  -DEMBER_SERIAL0_TX_QUEUE_SIZE=128 \
  -DEMBER_SERIAL0_RX_QUEUE_SIZE=128 \
  -DNO_READLINE \
  -D_GNU_SOURCE \
  -DAPP_NAME=\"$(APP_NAME)\" \


# Negate -Werror for some specific warnings.  Clang warns about many more
# things than GCC, so it gets some additional negations.
WNO_ERRORS = \
  -Wno-error=cast-align \
  -Wno-error=pointer-to-int-cast \

ifeq "$(shell $(CC) 2>&1 | grep -iqs clang ; echo $$?)" "0"
  WNO_ERRORS += \
    -Wno-error=pointer-sign \
    -Wno-error=tautological-constant-out-of-range-compare \

endif

CC_OPTIONS= \
  -Wall \
  -Wcast-align \
  -Wformat \
  -Wimplicit \
  -Wimplicit-int \
  -Wimplicit-function-declaration \
  -Winline \
  -Wno-long-long \
  -Wmain \
  -Wnested-externs \
  -Wno-import \
  -Wparentheses \
  -Wpointer-arith \
  -Wredundant-decls \
  -Wreturn-type \
  -Wstrict-prototypes \
  -Wswitch \
  -Wunused-label \
  -Wunused-value \
  -Werror \
  $(WNO_ERRORS) \
  -ggdb \
  -O0 \
  --std=c99 \


SOURCE_FILES_ZNET= \
  $(ZNET_DIR)/app/test/ezsp-host.c \
  $(ZNET_DIR)/app/util/ezsp/ezsp-callbacks.c \
  $(ZNET_DIR)/app/util/ezsp/ezsp-frame-utilities.c \
  $(ZNET_DIR)/app/util/ezsp/ezsp.c \
  $(ZNET_DIR)/app/util/ezsp/serial-interface-spi.c \
  $(ZNET_DIR)/app/util/gateway/backchannel.c \
  $(ZNET_DIR)/app/util/gateway/gateway-stub.c \
  $(ZNET_DIR)/app/util/serial/command-interpreter.c \
  $(ZNET_DIR)/app/util/serial/ember-printf-convert.c \
  $(ZNET_DIR)/app/util/serial/linux-serial.c \
  $(ZNET_DIR)/app/util/source-route-common.c \
  $(ZNET_DIR)/app/util/source-route-host.c \
  $(ZNET_DIR)/app/util/zigbee-framework/fragment-host.c \
  $(ZNET_DIR)/app/util/zigbee-framework/zigbee-device-common.c \
  $(ZNET_DIR)/app/util/zigbee-framework/zigbee-device-host.c \
  $(ZNET_DIR)/stack/framework/debug-extended-stub.c \
  $(ZNET_DIR)/stack/framework/packet-buffer.c \


SOURCE_FILES_BASE= \
  $(BASE_DIR)/hal/micro/generic/system-timer.c \
  $(BASE_DIR)/hal/micro/unix/host/micro.c \
  $(BASE_DIR)/hal/micro/unix/host/spi-protocol-linux.c \


SOURCE_FILES_OTHER= \


SOURCE_FILES= \
  $(SOURCE_FILES_ZNET) \
  $(SOURCE_FILES_BASE) \
  $(SOURCE_FILES_OTHER) \


# Build a list of object files from the source file list, but all objects
# live in the $(OUTPUT_DIR) above.  The list of object files
# created assumes that the file part of the filepath is unique
# (i.e. the bar.c of foo/bar.c is unique across all sub-directories included).
COMPILED_OBJECTS = $(addprefix $(OUTPUT_DIR)/, $(notdir $(SOURCE_FILES:.c=.o)))
DEPENDENCY_FILES = $(COMPILED_OBJECTS:.o=.d)
VPATH = $(dir $(SOURCE_FILES))

APP_LIBRARIES= \


LINK_FLAGS= \
  -pthread \


# Rules

.PHONY: clean all exe clean-output-dir clean-executable-dir

exe: $(EXECUTABLE_OUTPUT)
all: exe

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPENDENCY_FILES)
endif

-include $(MAKEFILE_DEBUG_RULES)

$(EXECUTABLE_OUTPUT): $(COMPILED_OBJECTS) $(APP_LIBRARIES) | $(OUTPUT_DIR) $(EXECUTABLE_DIR)
	$(LD) $^ $(LINK_FLAGS) $(APP_LIBRARIES) -o $(EXECUTABLE_OUTPUT)
	@echo
	@echo $@ build success
	@echo

$(OUTPUT_DIR)/%.o: %.c | $(OUTPUT_DIR)
	$(CC) $(CC_INCLUDE_DIRS) $(CC_DEFINES) $(CC_OPTIONS) -MF $(@:.o=.d) -MMD -MP -c $< -o $@

clean: clean-output-dir clean-executable-dir

clean-output-dir:
	rm -rf $(OUTPUT_DIR)

clean-executable-dir:
	rm -rf $(EXECUTABLE_DIR)

$(OUTPUT_DIR):
	@mkdir -p $(OUTPUT_DIR)

$(EXECUTABLE_DIR):
	@mkdir -p $(EXECUTABLE_DIR)
