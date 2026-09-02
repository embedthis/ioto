#
#   Makefile -- Top-level Ioto Makefile
#
#   Uses pre-generated project files from projects/gmake2/.
#   Auto-detects platform. No premake5 required for building.
#
#   Only targets that work in the published source distribution belong here. Anything that needs a
#   tool, script or checkout that the archive does not carry -- documentation generation, packaging,
#   formatting, project regeneration, the security gates -- lives in .local.mk, which is not in the
#   repository. See the header of that file.
#
#   Use "make help" for a list of available make variable options.
#

SHELL       := /bin/bash
NAME        := ioto
APPS        := ai blank http unit
APP         ?= http
OPTIMIZE    ?= debug
override TOP := $(shell realpath .)
BUILD       := build
BIN         := $(TOP)/$(BUILD)/bin
LOCAL       := $(strip $(wildcard ./.local.mk))

#
#   Detect make command (prefer gmake)
#
MAKE        := $(shell if which gmake >/dev/null 2>&1; then echo gmake ; else echo make ; fi) --no-print-directory

#
#   Auto-detect platform from host OS
#
UNAME       := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    PLATFORM := macosx
else ifeq ($(UNAME),Linux)
    PLATFORM := linux
else ifeq ($(UNAME),FreeBSD)
    PLATFORM := freebsd
else
    $(error Unsupported platform: $(UNAME). Use premake5 to regenerate for your OS.)
endif

CONFIG      := $(OPTIMIZE)_$(PLATFORM)
PATH        := $(TOP)/bin:$(BIN):$(PATH)
CDPATH      :=

.EXPORT_ALL_VARIABLES:

.PHONY: all app app-build apps build clean config dump help info lib library path run test verify-projects

ifndef SHOW
.SILENT:
endif

all: build

build: lib apps

#
#   Build library and tools using pre-generated premake makefiles
#
lib library:
	@mkdir -p $(BUILD)/bin
	@if [ ! -f projects/gmake2/Makefile ] ; then \
		echo "      [Error] projects/gmake2/Makefile not found. Run: cd projects && premake5 gmake" ; exit 255 ; \
	fi
	$(MAKE) -C projects/gmake2 config=$(CONFIG) verbose=$(SHOW)

#
#   Prepare apps (gen-config runs as POSTBUILDCMDS during library build)
#
config:
	@for app in $(APPS); do \
		bash bin/prepare $$app ; \
	done

#
#   Build all app binaries
#
apps: config
	@for app in $(APPS); do \
		$(MAKE) -C apps/$$app/projects/gmake2 config=$(CONFIG) verbose=$(SHOW) || exit 1; \
	done

#
#   Build a specific app using pre-generated premake makefiles
#
app app-build:
	@if [ ! -d apps/$(APP) ]; then \
		echo "      [Error] Unknown app \"$(APP)\""; exit 255; \
	fi
	@if [ ! -f apps/$(APP)/projects/gmake2/Makefile ] ; then \
		echo "      [Error] apps/$(APP)/projects/gmake2/Makefile not found. Run: cd apps/$(APP)/projects && premake5 gmake" ; exit 255 ; \
	fi
	$(MAKE) -C apps/$(APP)/projects/gmake2 config=$(CONFIG) verbose=$(SHOW)

#
#   Build and run a single app
#
run:
	@if [ ! -d "apps/$(APP)" ]; then \
		echo "      [Error] Unknown app \"$(APP)\""; exit 255; \
	fi
	@$(MAKE) lib
	@bash bin/prepare $(APP)
	@$(MAKE) app APP=$(APP)
	cd apps/$(APP) && $(BIN)/ioto-$(APP) -v

clean:
	@echo '       [Run] $@'
	rm -fr $(BUILD)
	rm -fr ./test/state ./test/*/state/certs
	rm -fr test/.testme test/*/.testme test/*/certs test/scale/*/certs
	rm -f .DS_Store */.DS_Store */*/.DS_Store */*/*/.DS_Store
	rm -fr test/web/fuzz/crashes-archive/*
	find . -name .testme | xargs rm -fr
	find . -name '*K.txt' | xargs rm -f

test:
	@./bin/prep-test.sh
	tm test

#
#   Prove projects/gmake2 is what projects/premake5.lua generates. A hand edit to a generated
#   makefile survives until the next regeneration and is then silently reverted.
#
verify-projects:
	@bash bin/verify-projects.sh

info:
	@VERSION=`$(BIN)/json --default 1.2.3 version package.json` ; \
	echo "      [Info] Built Ioto $${VERSION} $(OPTIMIZE) [$(PLATFORM)] — all apps"
	@echo "      [Info] Run via: \"make run APP=<name>\""

path:
	echo $(PATH)

#
#   Dump the local database contents
#
dump:
	db --schema apps/$(APP)/state/config/schema.json5 apps/$(APP)/state/db/device.db

#
#   Convenience targets for building a single app
#
ai blank http unit:
	@$(MAKE) lib
	@bash bin/prepare $@
	@$(MAKE) app APP=$@
	@VERSION=`$(BIN)/json --default 1.2.3 version package.json` ; \
	echo "      [Info] Built Ioto $${VERSION} $(OPTIMIZE) [$(PLATFORM)] with the \"$@\" app"
	@echo "      [Info] Run via: \"make run APP=$@\""

help:
	@echo '' >&2
	@echo 'usage: make [clean, build, run, test]' >&2
	@echo '' >&2
	@echo 'The default "make" builds all apps. To build a single app:' >&2
	@echo '' >&2
	@echo '  make http                 Build http app only' >&2
	@echo '  make ai                   Build AI app only' >&2
	@echo '' >&2
	@echo 'Available apps:' >&2
	@echo '  ai     Test invoking AI LLMs.' >&2
	@echo '  blank  Build without an app.' >&2
	@echo '  blink  Simple blink example (ESP32 only).' >&2
	@echo '  http   Run just the web server.' >&2
	@echo '  unit   Run unit tests.' >&2
	@echo '' >&2
	@echo 'Run a specific app:' >&2
	@echo '' >&2
	@echo '  make run APP=http' >&2
	@echo '' >&2
	@echo 'Other targets:' >&2
	@echo '  make lib                  Build libioto.a and the utility tools only' >&2
	@echo '  make apps                 Build all app binaries' >&2
	@echo '  make test                 Run unit tests' >&2
	@echo '  make clean                Remove build artifacts' >&2
	@echo '  make verify-projects      Verify projects/gmake2 matches what premake5.lua generates' >&2
	@echo '' >&2
	@echo 'Make variables:' >&2
	@echo '  OPTIMIZE=debug|release    Optimization level' >&2
	@echo '  SHOW=1                    Show build commands' >&2
	@echo '' >&2

ifneq ($(LOCAL),)
include $(LOCAL)
endif

# vim: set expandtab tabstop=4 shiftwidth=4 softtabstop=4:
