#
#	Build the blink App on ESP32
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

all: config

build api:

config:
	json -q --check config/ioto.json5 || echo "Cannot parse ioto.json5"
	if [ ! -f $(CONFIG)/web.json5 ] ; then \
		ccp config/web.json5 $(CONFIG)/web.json5 ; \
	fi

clean: