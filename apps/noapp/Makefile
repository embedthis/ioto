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

.PHONY: build config export post

all: config

config: export schema

export:
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
	if [ ! -f $(CONFIG)/schema.json5 ] ; then \
		json --blend . config/schema.json5 >$(CONFIG)/schema.json5 ; \
	fi

build clean post:

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
