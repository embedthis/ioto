#
#	Build the ai app
#
TOP			:= $(realpath ../..)

ifndef SHOW
.SILENT:
endif

.PHONY: api build config

all: config build

config: 
	@$(TOP)/bin/export-app-config $(TOP)

build:
	@echo "      [Info] Built with the AI App"

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
