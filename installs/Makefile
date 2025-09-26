#
#   Makefile - Top-level Ioto makefile
#
#	This Makefile is used for native builds and for ESP32 configuration.
#
#	Use "make help" for a list of available make variable options.
#
#	Note: In this Makefile and projects/*.mk, the PROFILE is set to "default"
#	OPTIMIZE is set to debug or release.
#

SHELL		:= /bin/bash
TOOLS		:= $(shell bin/prep-build)
NAME		:= ioto
PROFILE 	:= dev
TOP			:= $(shell realpath .)
BASE		:= $(shell realpath .)
STATE		:= $(TOP)/state
CONFIG		:= $(STATE)/config
DB			:= $(STATE)/db
CERTS 		:= $(STATE)/certs
SITE		:= $(STATE)/site
BUILD		:= build
BIN			:= $(BASE)/$(BUILD)/bin
PATH		:= $(BASE)/bin:$(BIN):$(PATH)
CDPATH		:=
OS			:= $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')
VERSION		:= $(shell ./bin/json version pak.json)
OPTIMIZE	:= $(shell ./bin/json -n --profile $(PROFILE) --default debug optimize $(CONFIG)/ioto.json5)
APP			:= $(shell ./bin/json -n --default demo app $(CONFIG)/ioto.json5)
LAPP		:= $(shell echo $(APP) | tr a-z A-Z)
MAKE		:= $(shell if which gmake >/dev/null 2>&1; then echo gmake ; else echo make ; fi) --no-print-directory
PROJECT		:= projects/$(NAME)-$(OS)-default.mk

ifeq ($(ARCH),)
	ARCH	:= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/mips.*/mips/;s/aarch/arm/')
endif

#
#	Dynamically create the build control environment from ioto.json5
#
ENV			:= $(shell ./bin/json -q --env services $(CONFIG)/ioto.json5 >.env)

include		.env

.EXPORT_ALL_VARIABLES:
.PHONY:		app build certs clean compile compile-app compile-agent config prep profile info show

ifndef SHOW
.SILENT:
endif

all: build

build: prep config compile certs info

#
#	Prepare to build natively using the O/S host platform
#
prep:
	@if [ ! -d "include" -a ! -f ".local.mk"] ; then \
		echo ; echo "Ioto has not been correctly installed to use this Makefile" ; echo ; \
		exit 255 ; \
	fi
	mkdir -p $(CONFIG) $(SITE) $(DB) $(CERTS)
	@if [ "$(APP)" = "blank" ] ; then \
		echo "      [Info] Building Ioto $(VERSION) optimized for \"$(OPTIMIZE)\"" ; \
	else \
		echo "      [Info] Building Ioto $(VERSION) optimized for \"$(OPTIMIZE)\" with the \"$(APP)\" app" ; \
	fi

#
#	Configure the selected app
#
config: config-app include/ioto-config.h certs/test.crt

config-app:
	@if [ -f "$(BUILD)/.app" ] ; then \
		if [ "$(APP)" != "`cat $(BUILD)/.app`" ] ; then \
			echo "   [Warning] Selected App has changed since the last build." ; \
			echo "  [Selected] Selected App \"$(APP)\"." ; \
			$(MAKE) clean ; \
			rm -fr $(SITE)/* $(DB)/*.jnl $(DB)/*.db ; \
			find $(CONFIG) -type f -print | egrep -v 'ioto.json5|device.json5|local.json5' | xargs -r rm ; \
		fi ; \
	fi
	@mkdir -p $(BUILD) ; echo "$(APP)" >$(BUILD)/.app
	@$(MAKE) -C apps/$(APP) BASE=$(BASE) TOP=$(TOP) SHOW=$(SHOW) config

compile: compile-ioto compile-app

compile-ioto: include/ioto-config.h
	@if [ ! -f $(PROJECT) ] ; then \
		echo "The build configuration $(PROJECT) is not supported" ; exit 255 ; \
	fi
	@$(MAKE) -f $(PROJECT) OPTIMIZE=$(OPTIMIZE) ME_COM_$(LAPP)=1 PROFILE=default BUILD=$(BUILD) compile 
	@echo '      [Info] Built the $(NAME) agent and library'

compile-app:
	$(MAKE) -C apps/$(APP) SHOW=$(SHOW) BASE=$(BASE) TOP=$(TOP) OPTIMIZE=$(OPTIMIZE) build

#
#	Create the ioto-config.h header from the app's ioto.json5
#
include/ioto-config.h: $(CONFIG)/ioto.json5
	@echo '    [Create] ioto-config.h'
	./bin/json --header services $(CONFIG)/ioto.json5 >include/ioto-config.h
	rm -f $(BUILD)/obj/*.o

certs: certs/test.crt

certs/test.crt:
	@echo '      [Info] Generate test certs'
	bin/make-certs
	mkdir -p $(BUILD)/bin
	cp state/certs/roots.crt $(BUILD)/bin

config-esp32:
	@./bin/config-esp32 $(APP)

clean clobber:
	@echo '       [Run] $@'
	@$(MAKE) -f $(PROJECT) TOP=$(TOP) APP=$(APP) $@
	rm -f $(DB)/*.jnl $(DB)/*.db 
	rm -f $(CONFIG)/db.json5 $(CONFIG)/display.json5 $(CONFIG)/local.json5
	rm -f $(CONFIG)/signature.json5 $(CONFIG)/web.json5 $(CONFIG)/schema.json5
	rm -fr ./build ./state
	if [ -d apps/$(APP) ] ; then \
		$(MAKE) -C apps/$(APP) SHOW=$(SHOW) BASE=$(BASE) TOP=$(TOP) clean ; \
	fi

install installBinary uninstall:
	@echo '       [Run] $@'
	@$(MAKE) -f $(PROJECT) TOP=$(TOP) APP=$(APP) $@

info:
	@if [ "$(APP)" != 'blank' ] ; then \
		echo "      [Info] Run via: \"make run\". Run \"ioto\" manually with \"$(BUILD)/bin\" in your path." ; \
		echo "" ; \
	fi

run:
	$(BUILD)/bin/ioto -v

path:
	echo $(PATH)

#
#	Dump the local database contents. Note: the db may have data cached in memory for a little while
#
dump:
	db --schema $(CONFIG)/schema.json5 $(DB)/device.db

#
#	Convenience targets for building various apps
#
auth eco demo blank unit: 
	make APP=$@
	@$(MAKE) TOP=$(TOP) APP=$@

help:
	@echo '' >&2
	@echo 'usage: make [clean, build, run]' >&2
	@echo '' >&2
	@echo 'Change the configuration by editing $(CONFIG)/ioto.json.' >&2
	@echo '' >&2
	@echo '' >&2
	@echo 'Select from the following apps:' >&2
	@echo '  ai 	Test invoking AI LLMs.' >&2
	@echo '  auth	Test user login and authentication app.' >&2
	@echo '  blank	Build without an app.' >&2
	@echo '  demo   Cloud-based Ioto demo sample app.' >&2
	@echo '' >&2
	@echo 'To select your App, add APP=NAME:' >&2
	@echo '' >&2
	@echo '  make APP=demo' >&2
	@echo '' >&2
	@echo 'Other make environment variables:' >&2
	@echo '  ARCH               # CPU architecture (x86, x64, ppc, ...)' >&2
	@echo '  OS                 # Operating system (linux, macosx, ...)' >&2
	@echo '  CC                 # Compiler to use ' >&2
	@echo '  LD                 # Linker to use' >&2
	@echo '  OPTIMIZE           # Set to "debug" or "release" for a debug or release build of the agent.' >&2
	@echo '  CFLAGS             # Add compiler options. For example: -Wall' >&2
	@echo '  DFLAGS             # Add compiler defines. For example: -DCOLOR=blue' >&2
	@echo '  IFLAGS             # Add compiler include directories. For example: -I/extra/includes' >&2
	@echo '  LDFLAGS            # Add linker options' >&2
	@echo '  LIBPATHS           # Add linker library search directories. For example: -L/libraries' >&2
	@echo '  LIBS               # Add linker libraries. For example: -lpthreads' >&2
	@echo '' >&2
	@echo 'Use "SHOW=1 make" to show executed commands.' >&2
	@echo '' >&2

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif

# vim: set expandtab tabstop=4 shiftwidth=4 softtabstop=4:
