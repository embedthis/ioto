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
APPS        := blank http unit
APP         ?= http
OPTIMIZE    ?= release
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

.PHONY: all app app-build apps build clean config config-esp32 dump help info lib library path run switch-config \
        test test-linux verify-projects

ifndef SHOW
.SILENT:
endif

all: build

build: lib apps

#
#   The debug and release configurations keep their objects apart but write the same build/bin, so
#   building one after the other could leave the other's newer libioto.a, tools or apps in place.
#   build/bin/.config records the configuration that built build/bin; a different one removes it.
#
switch-config:
	@if [ -f $(BIN)/.config ] && [ "$$(cat $(BIN)/.config)" != "$(CONFIG)" ] ; then \
		echo "      [Info] $(BUILD)/bin was built $$(cat $(BIN)/.config), building $(CONFIG): removing it" ; \
		rm -fr $(BIN) ; \
	fi
	@mkdir -p $(BIN)
	@echo "$(CONFIG)" > $(BIN)/.config

#
#   Build library and tools using pre-generated premake makefiles
#
lib library: switch-config
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

# ESP_TOP selects the ESP-IDF project; APP selects the app.
config-esp32:
	@bash bin/config-esp32 $(APP) $(ESP_TOP)

#
#   An app links libioto.a by name from another workspace, so premake gives its makefile no
#   dependency on the library and an app would keep the old code after the library changed. Name
#   the library as the link dependency.
#
APP_LDDEPS  := $(BIN)/libioto.a

#
#   Build all app binaries
#
apps: switch-config config
	@for app in $(APPS); do \
		$(MAKE) -C apps/$$app/projects/gmake2 config=$(CONFIG) verbose=$(SHOW) LDDEPS=$(APP_LDDEPS) || exit 1; \
	done

#
#   Build a specific app using pre-generated premake makefiles
#
app app-build: switch-config
	@if [ ! -d apps/$(APP) ]; then \
		echo "      [Error] Unknown app \"$(APP)\""; exit 255; \
	fi
	@if [ ! -f apps/$(APP)/projects/gmake2/Makefile ] ; then \
		echo "      [Error] apps/$(APP)/projects/gmake2/Makefile not found. Run: cd apps/$(APP)/projects && premake5 gmake" ; exit 255 ; \
	fi
	$(MAKE) -C apps/$(APP)/projects/gmake2 config=$(CONFIG) verbose=$(SHOW) LDDEPS=$(APP_LDDEPS)

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
	rm -fr ./test/state ./test/*/state/certs ./test/soak/state ./test/stress/state ./test/*/.pidfile
	rm -fr test/.testme test/*/.testme test/*/certs test/scale/*/certs
	rm -f .DS_Store */.DS_Store */*/.DS_Store */*/*/.DS_Store
	rm -fr test/web/fuzz/crashes-archive/*
	find . -name .testme | xargs rm -fr
	find . -name '*K.txt' | xargs rm -f

#
#   TestMe rebuilds a test only when its own source is newer than the binary it cached, so a change
#   to libioto.a - which is what "make import" delivers - is silently not tested and the suite
#   reports a pass for code that is no longer there. db/rbtree.tst.c ran that way for as long as it
#   took to notice. Re-link everything when the library is newer than the binaries.
#
#   The suite needs the debug configuration (its web tests link the /test actions only the debug
#   library compiles), so it builds that first, whatever OPTIMIZE says.
#
test:
	@$(MAKE) OPTIMIZE=debug build
	@./bin/prep-test.sh
	@if [ -n "$$(find test -type f -perm -u+x ! -newer $(BIN)/libioto.a -print -quit 2>/dev/null)" ] ; then \
		echo "      [Info] library is newer than the test binaries - rebuilding" ; \
		tm --rebuild test ; \
	else \
		tm test ; \
	fi

#
#   Build and run the suite under Linux in a container, on a macOS host. Verifies the working tree,
#   not a pushed commit, so a Linux-only defect is caught before CI rather than by it. ARGS is
#   passed through to tm, so 'make test-linux ARGS="web"' runs that group.
#
#   Deliberately absent from "make help": it needs Apple's container tool and bin/test-linux.sh,
#   which bin/make-dist does not stage, so it is not a target a customer can run.
#
test-linux:
	@bash bin/test-linux.sh $(ARGS)

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
blank http unit:
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
	@echo '  make blank                Build blank app only' >&2
	@echo '' >&2
	@echo 'Available apps:' >&2
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
	@echo '  make test                 Build the debug configuration and run the unit tests' >&2
	@echo '  make clean                Remove build artifacts' >&2
	@echo '  make verify-projects      Verify projects/gmake2 matches what premake5.lua generates' >&2
	@echo '' >&2
	@echo 'Make variables:' >&2
	@echo '  OPTIMIZE=release|debug    Build configuration (default: release)' >&2
	@echo '  SHOW=1                    Show build commands' >&2
	@echo '' >&2

ifneq ($(LOCAL),)
include $(LOCAL)
endif

# vim: set expandtab tabstop=4 shiftwidth=4 softtabstop=4:
