#
#	Makefile - Ioto Makefile for building on ESP32
#
TOP		:= $(shell realpath .)
BASE	:= $(shell realpath components/ioto)
STATE	:= $(TOP)/state
CONFIG	:= $(STATE)/config
CERTS	:= $(STATE)/certs
SITE	:= $(STATE)/site
DB		:= $(STATE)/db
PATH 	:= $(BASE)/bin:$(PATH)
TOOLS	:= $(shell cd $(BASE) ; bin/prep-build)
APP 	:= $(shell $(BASE)/bin/json -q app $(CONFIG)/ioto.json5 || ls -1 $(BASE)/apps | egrep 'demo')
EDIR	:= $(BASE)/apps/$(APP)/esp32
TARGET	:= $(touch sdkconfig ; grep CONFIG_IDF_TARGET sdkconfig || echo esp32-s3)
FORCE	:= $(shell test "`cat .app 2>/dev/null`" = "$(APP)" || echo force)

.EXPORT_ALL_VARIABLES:

ifndef SHOW
.SILENT:
endif

.PHONY: check install-app force notice install-site build local

all: prepare

prepare: check build-tools config-ioto install-app build-app-ui notice local

check:
	@if [ ! -d $(TOP)/components/ioto ] ; then \
		echo ; echo "Ioto must be installed under components/ioto" ; \
		exit 2 ; \
	fi
		@if [ ! -d "$(BASE)/apps/$(APP) ] ; then \
		echo ; echo "Unknown app \"$(APP)\"" ; \
		exit 2 ; \
	fi
	@if [ ! -d "$(BASE)/apps/$(APP)/esp32" ] ; then \
		echo ; echo "Unsupported ESP32 app \"$(APP)\"" ; \
		exit 2 ; \
	fi
	@echo $(APP) >.app
	mkdir -p state/config

build-tools:
	cd components/ioto >/dev/null ; bin/prep-build

notice:
	@if [ -f $(TOP)/local.json5 ] ; then cp $(TOP)/local.json5 $(TOP)/state/config ; fi
	@echo " [Installed] App $(APP)"
	@if egrep 'wifi-ssid|wifi-password' main/main.c >/dev/null ; then \
		echo "      [Info] Define your WIFI credentials in main/main.c before building" ; \
	fi

config-ioto:
	$(MAKE) -C $(BASE) TOP=$(TOP) BASE=$(BASE) APP=$(APP) prep config certs

#
#	Install the app files from components/ioto/apps/* to the top level for exectution
#
install-app: \
	$(TOP)/main/main.c \
	$(TOP)/CMakeLists.txt \
	$(TOP)/partitions.csv \
	$(TOP)/sdkconfig.defaults \
	$(CONFIG)/ioto.json5 \
	$(BASE)/include/config.h \
	$(CERTS)/test.crt \
	$(BASE)/include/me.h \
	$(BASE)/CMakeLists.txt \
	$(BASE)/Kconfig 

build-app-ui:
	@if [ -d $(BASE)/apps/$(APP)/ui ] ; then \
		$(MAKE) TOP=$(TOP) BASE=$(BASE) OPTIMIZE=prod -C $(BASE)/apps/$(APP)/ui build ; \
	fi

configure:
	idf.py set-target $(TARGET)

build:
	@echo "  [Building] Making ESP, Ioto and $(APP) app"
	idf.py build

clean: 
	idf.py clean

clobber:
	idf.py fullclean

flash:
	idf.py -p $(PORT) flash

monitor:
	idf.py monitor

menu:
	idf.py menuconfig

size: 
	idf.py size

$(BASE)/include/me.h: $(BASE)/projects/ioto*-freertos-default-me.h $(FORCE)
	@echo "   [Install] Ioto include/me.h"
	cp $(BASE)/projects/ioto*-freertos-default-me.h $(BASE)/include/me.h 

$(CONFIG)/ioto.json5: $(BASE)/apps/$(APP)/config/ioto.json5 $(FORCE)
	@echo "   [Install] App Ioto configuration"
	mkdir -p $(CONFIG)
	cp $(BASE)/apps/$(APP)/config/ioto.json5 $(CONFIG)


$(BASE)/include/config.h: $(BASE)/state/config/ioto.json5
	json --header services $(BASE)/state/config/ioto.json5 >$(BASE)/include/config.h

$(CERTS)/test.crt: $(BASE)/certs/test.crt $(FORCE)
	@echo "   [Install] Test Certificates"
	mkdir -p $(CERTS)
	cp -r $(BASE)/certs/test.crt $(CERTS)
	cp -r $(BASE)/certs/test.key $(CERTS)
	cp -r $(BASE)/certs/roots.crt $(CERTS)
	cp -r $(BASE)/certs/aws.crt $(CERTS)

$(BASE)/CMakeLists.txt: CMakeLists.txt
	@echo "   [Install] Ioto CMakeLists.txt"
	cp $(BASE)/projects/esp32/CMakeLists.txt $(BASE)

$(BASE)/Kconfig: $(EDIR)/Kconfig $(FORCE)
	@echo "   [Install] Ioto Kconfig"
	cp -r $(EDIR)/Kconfig $(BASE)

$(TOP)/partitions.csv: $(EDIR)/partitions.csv $(FORCE)
	@echo "   [Install] App file system partitions.csv"
	cp $(EDIR)/partitions.csv $(TOP)

$(TOP)/CMakeLists.txt: $(EDIR)/CMakeLists.txt $(FORCE)
	@echo "   [Install] App CMakeLists.txt"
	cp $(EDIR)/CMakeLists.txt $(TOP)

#
#	Copies main.c, CMakeLists.txt and idf_component.yml
#
$(TOP)/main/main.c: $(EDIR)/main/main.c $(FORCE)
	@echo "   [Install] App main.c"
	cp -r $(EDIR)/main $(TOP)
	if [ -d $(BASE)/apps/$(APP)/src ] ; then \
		cp $(BASE)/apps/$(APP)/src/* $(TOP)/main ; \
	fi

$(TOP)/sdkconfig.defaults: $(EDIR)/sdkconfig.defaults $(FORCE)
	@echo "   [Install] App sdkconfig.defaults"
	cp -r $(EDIR)/sdkconfig.defaults $(TOP)

install-site: $(FORCE)
	@if [ -d $(BASE)/apps/$(APP)/site ] ; then \
		mkdir -p $(SITE) ; \
		cp -r $(BASE)/apps/$(APP)/site/* $(SITE) ; \
		echo "   [Install] App web site" ; \
	fi

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
