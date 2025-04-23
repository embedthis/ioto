#
#	Build the NoApp
#
TOP			:= $(realpath ../..)
STATE		:= $(TOP)/state
CONFIG		:= $(STATE)/config
SITE		:= $(STATE)/site
DB			:= $(STATE)/db

export PATH

ifndef SHOW
.SILENT:
endif

.PHONY: build config

all: config build

config:
	@export-app-config $(TOP)

build:
	@echo "      [Info] Built with No App"

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
