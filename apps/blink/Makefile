#
#	Build the blink App on ESP32
#
TOP	:= $(realpath ../..)

ifndef SHOW
.SILENT:
endif

.PHONY: build config

all: config build

config:
	@$(TOP)/bin/export-app-config $(TOP)

build:
	@echo "      [Info] Built with the Blink App"

clean:

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
