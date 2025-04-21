#
#	Build the ai app
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
	@if [ -f config/local.json5 -a ! -f $(CONFIG)/local.json5 ] ; then \
		json -q --check config/local.json5 || echo "Cannot parse local.json5" ; \
		ccp config/local.json5 $(CONFIG)/local.json5 ; \
	fi
	json -q --check config/device.json5 || echo "Cannot parse device.json5"
	json -q --check config/ioto.json5 || echo "Cannot parse ioto.json5"
	json -q --check config/web.json5 || echo "Cannot parse web.json5"
	if [ ! -f $(CONFIG)/device.json5 ] ; then \
		ccp config/device.json5 $(CONFIG)/device.json5 ; \
	fi
	if [ ! -f $(CONFIG)/ioto.json5 ] ; then \
		ccp config/ioto.json5 $(CONFIG)/ioto.json5 ; \
	fi
	if [ ! -f $(CONFIG)/web.json5 ] ; then \
		ccp config/web.json5 $(CONFIG)/web.json5 ; \
	fi
	mkdir -p $(SITE)
	cp -r site/* $(SITE)

schema:
	json --blend . config/schema.json5 >$(CONFIG)/schema.json5

api post clean:

build:
	@echo "      [Info] Built with the AI App"

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
