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

config: export

export:
	json -q --check config/ioto.json5 || echo "Cannot parse ioto.json5"
	ccp config/ioto.json5 $(CONFIG)/ioto.json5

build clean post: