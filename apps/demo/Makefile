#
#	Build the demo app
#
TOP			:= $(realpath ../..)
STATE		:= $(TOP)/state
CONFIG		:= $(STATE)/config
SITE		:= $(STATE)/site
DB			:= $(STATE)/db

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
		ccp config/local.json5 $(CONFIG)/local.json5 ; \
	fi
	json -q --check config/device.json5 || echo "Cannot parse device.json5"
	json -q --check config/ioto.json5 || echo "Cannot parse ioto.json5"
	json -q --check config/web.json5 || echo "Cannot parse web.json5"
	ccp config/device.json5 $(CONFIG)/device.json5
	ccp config/ioto.json5 $(CONFIG)/ioto.json5
	ccp config/web.json5 $(CONFIG)/web.json5
	mkdir -p $(SITE)
	cp -r site/* $(SITE)

schema:
	json --blend . config/schema.json5 >$(CONFIG)/schema.json5

api post clean:

build:
	@echo "      [Info] Built with the Demo App"

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
