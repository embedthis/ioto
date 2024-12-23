#
#	Build the eco demo 
#
TOP			:= $(realpath ../..)
STATE		:= $(TOP)/state
CONFIG		:= $(STATE)/config
SITE		:= $(STATE)/site
DB			:= $(STATE)/db
OPTIMIZE	?= debug
MAKE        := $(shell if which gmake >/dev/null 2>&1; then echo gmake ; else echo make ; fi) --no-print-directory

export

ifndef SHOW
.SILENT:
endif

.PHONY: api build config export schema post

all: config build post

config: export api schema

export:
	@if [ -f config/local.json5 ] ; then \
		json -q --check config/local.json5 || echo "Cannot parse local.json5" ; \
		$(TOP)/tools/ccp config/local.json5 $(CONFIG)/local.json5 ; \
	fi
	json -q --check config/device.json5 || echo "Cannot parse device.json5"
	json -q --check config/ioto.json5 || echo "Cannot parse ioto.json5"
	json -q --check config/Dashboard.json5 || echo "Cannot parse Dashboard.json5"
	$(TOP)/tools/ccp config/device.json5 $(CONFIG)/device.json5
	$(TOP)/tools/ccp config/ioto.json5 $(CONFIG)/ioto.json5
	$(TOP)/tools/ccp config/Dashboard.json5 $(CONFIG)/Dashboard.json5

build:
	@echo "      [Info] Built with the Eco House App"

schema:
	json --blend . config/schema.json5 >$(CONFIG)/schema.json5

api clean post:

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
