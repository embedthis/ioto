#
#	Build the demo app
#
TOP			:= $(realpath ../..)

ifndef SHOW
.SILENT:
endif

.PHONY: build config upload

all: config build

config:
	@$(TOP)/bin/export-app-config $(TOP)

build: upload
	@echo "      [Info] Built with the Demo App"

upload:
	if [ $(TOP)/state/config/schema.json5 -nt upload/schema.json5 ]; then \
		cp $(TOP)/state/config/schema.json5 upload/schema.json5 ; \
	fi

clean:

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
