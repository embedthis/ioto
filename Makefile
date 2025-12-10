#
#   Makefile - Top-level Ioto makefile
#
#	This Makefile is used for native builds and for ESP32 configuration.
#	It is a wrapper over generated makefiles under ./projects.
#
#	Use "make help" for a list of available make variable options.
#

SHELL		:= /bin/bash
PROFILE 	?= dev
OPTIMIZE	?= debug
TOOLS	    := $(shell bin/prep-build)
TOP			:= $(shell realpath .)
APP			?= $(shell if [ -f build/.app ] ; then cat build/.app ; else echo unit ; fi)
MAKE		:= $(shell if which gmake >/dev/null 2>&1; then echo gmake ; else echo make ; fi) --no-print-directory
OS			:= $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')
ARCH		?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/mips.*/mips/;s/aarch/arm/')
LOCAL 		:= $(strip $(wildcard ./.local.mk))
PROJECT		:= projects/ioto-$(OS)-default.mk
CONFIG		:= $(TOP)/state/config
BUILD		:= build

BIN			:= $(TOP)/$(BUILD)/bin
PATH		:= $(TOP)/bin:$(BIN):$(PATH)
CDPATH		:=

.EXPORT_ALL_VARIABLES:

.PHONY:		app build clean compile config doc info projects show test

ifndef SHOW
.SILENT:
endif

all: build

build: config compile info

config:
	@mkdir -p $(BUILD)/bin $(CONFIG)/certs $(CONFIG)/db $(CONFIG)/site 
	if [ ! -d apps/$(APP) ] ; then \
		echo "      [Error] Unknown app \"$(APP)\"" ; exit 255 ; \
	fi ; \
	if [ -f build/.app ] ; then \
		if [ "$(APP)" != "`cat build/.app`" ] ; then \
			echo "      [Clean] Previous app build" ; \
			$(MAKE) clean ; \
		fi ; \
	fi 
	echo $(APP) >build/.app

compile:
	@if [ ! -f $(PROJECT) ] ; then \
		echo "The build configuration $(PROJECT) is not supported" ; exit 255 ; \
	fi
	LAPP=$(shell echo $(APP) | tr a-z A-Z) ; \
	$(MAKE) -f $(PROJECT) OPTIMIZE=$(OPTIMIZE) ME_COM_$${LAPP}=1 PROFILE=$(PROFILE) BUILD=$(BUILD) compile 

clean:
	@echo '       [Run] $@'
	@$(MAKE) -f $(PROJECT) TOP=$(TOP) APP=$(APP) PROFILE=$(PROFILE) $@
	if [ -d samples/agent ] ; then make -C samples/agent clean ; fi
	rm -f $(CONFIG)/db/*.jnl $(CONFIG)/db/*.db 
	rm -f $(CONFIG)/db.json5 $(CONFIG)/display.json5 $(CONFIG)/local.json5
	rm -f $(CONFIG)/signature.json5 $(CONFIG)/web.json5 $(CONFIG)/schema.json5
	rm -fr projects/ioto-macosx-mine.xcodeproj
	rm -fr ./bin/json
	rm -fr ./state ./test/state ./test/*/state/certs
	rm -fr test/.testme test/*/.testme test/*/certs test/scale/*/certs
	rm -f .DS_Store */.DS_Store */*/.DS_Store */*/*/.DS_Store
	rm -fr test/web/fuzz/crashes-archive/*
	find . -name provision.json5 | xargs rm -f
	find . -name ioto.key | xargs rm -f
	find . -name ioto.crt | xargs rm -f
	find . -name .testme | xargs rm -fr
	find . -name '*K.txt' | xargs rm -f

config-esp32:
	@./bin/config-esp32 $(APP)

install installBinary uninstall:
	@echo '       [Run] $@'
	@$(MAKE) -f $(PROJECT) TOP=$(TOP) APP=$(APP) PROFILE=$(PROFILE) $@

info:
	@VERSION=`json --default 1.2.3 version pak.json` ; \
	echo "      [Info] Built Ioto $${VERSION} optimized for \"$(OPTIMIZE)\" with the \"$(APP)\" app"
	echo "      [Info] Run via: \"sudo make run\". Run \"ioto\" manually with \"$(BUILD)/bin\" in your path."

test:
	# Check for test prerequisites
	@./bin/prep-test.sh
	tm test
	
run:
	$(BUILD)/bin/ioto -v

path:
	echo $(PATH)

#
#	Dump the local database contents. Note: the db may have data cached in memory for a little while
#
dump:
	db --schema $(CONFIG)/schema.json5 $(CONFIG)/db/device.db

#
#	Convenience targets for building various apps
#
ai blank demo http unit: 
	$(MAKE) TOP=$(TOP) APP=$@

help:
	@echo '' >&2
	@echo 'usage: make [clean, build, run]' >&2
	@echo '' >&2
	@echo 'Change the configuration by editing $(CONFIG)/ioto.json.' >&2
	@echo '' >&2
	@echo '' >&2
	@echo 'Select from the following apps:' >&2
	@echo '  ai 	Test invoking AI LLMs.' >&2
	@echo '  blank	Build without an app.' >&2
	@echo '  blink	Blink the LED on ESP32.' >&2
	@echo '  demo   Cloud-based Ioto demo sample app.' >&2
	@echo '  http	Run just the web server.' >&2
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

ifneq ($(LOCAL),)
include $(LOCAL)
endif

# vim: set expandtab tabstop=4 shiftwidth=4 softtabstop=4:
