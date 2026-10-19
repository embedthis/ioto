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
APP		?= $(shell if [ -f .app ] ; then cat .app ; else echo blink ; fi)

EDIR	:= $(BASE)/apps/$(APP)/esp32
ADIR	:= $(BASE)/apps/$(APP)
TARGET	:= $(shell touch sdkconfig ; grep CONFIG_IDF_TARGET= sdkconfig | sed 's/CONFIG_IDF_TARGET="//;s/"//' || echo esp32-c6)
FORCE	:= $(shell test "`cat .app 2>/dev/null`" = "$(APP)" || echo force)
PORT	:= /dev/tty.usbmodem101

.EXPORT_ALL_VARIABLES:

ifndef SHOW
.SILENT:
endif

.PHONY: check config build-tools force notice install-certs install-site build local force

all: prepare build

prepare: check build-tools config install-site build-app-ui notice local

check:
	@if [ ! -d "$(TOP)/components/ioto" ] ; then \
		echo ; echo "Ioto must be installed under components/ioto" ; \
		exit 2 ; \
	fi
	echo $(APP) >.app
	@if [ ! -d "$(BASE)/apps/$(APP)" ] ; then \
		echo ; echo "Unknown app \"$(APP)\"" ; \
		exit 2 ; \
	fi
	@if [ ! -d "$(BASE)/apps/$(APP)/esp32" ] ; then \
		echo ; echo "Unsupported ESP32 app \"$(APP)\"" ; \
		exit 2 ; \
	fi
	if [ -f .app ] ; then \
		if [ "$(APP)" != "`cat .app`" ] ; then \
			echo "      [Clean] Previous app build" ; \
			$(MAKE) clean ; \
			idf.py reconfigure ; \
		fi ; \
	fi 
	@echo $(APP) >.app
	@echo "  [Building] App $(APP)"
	mkdir -p state/config state/db state/certs state/site
	@echo "     [Using] Target $(TARGET) on $(PORT)"

build-tools:
	$(MAKE) -C $(BASE) lib

notice:
	@if [ -f $(TOP)/local.json5 ] ; then cp $(TOP)/local.json5 $(TOP)/state/config ; fi
	@echo " [Installed] App $(APP)"
	@if egrep 'wifi-ssid|wifi-password' main/main.c >/dev/null ; then \
		echo "      [Info] Define your WIFI credentials in main/main.c before building" ; \
		exit 2 ; \
	fi

#
#	Configure Ioto with the selected app
#
config: check config-ioto \
	$(TOP)/main/main.c \
	$(TOP)/CMakeLists.txt \
	$(TOP)/partitions.csv \
	$(CONFIG)/ioto.json5 \
	$(CONFIG)/device.json5 \
	$(CONFIG)/schema.json5 \
	$(CONFIG)/db.json5 \
	$(CONFIG)/web.json5 \
	$(DB)/.keep \
	$(BASE)/include/config.h \
	install-certs \
	$(BASE)/CMakeLists.txt \
	$(BASE)/Kconfig 

config-ioto: $(TOP)/sdkconfig.defaults
	$(MAKE) -C $(BASE) BASE=$(BASE) APP=$(APP) APPS=$(APP) ESP_TOP=$(TOP) config config-esp32

build-app-ui:
	@if [ -d $(BASE)/apps/$(APP)/ui ] ; then \
		$(MAKE) TOP=$(TOP) BASE=$(BASE) OPTIMIZE=prod -C $(BASE)/apps/$(APP)/ui build ; \
	fi

#
#	Configure the project based on the sdkconfig.defaults
#
configure:
	idf.py set-target $(TARGET)

build: config
	@echo "  [Building] Making ESP, Ioto and $(APP) app"
	@APP=$(APP) idf.py build

clean: 
	$(MAKE) -C $(BASE) TOP=$(TOP) BASE=$(BASE) APP=$(APP) clean
	idf.py clean
	rm -fr state
	rm -f sdkconfig
	rm -fr main
	mkdir -p state/config state/db state/certs state/site

clobber: clean
	idf.py fullclean
	rm -f sdkconfig.defaults sdkconfig

flash:
	idf.py -p $(PORT) flash

monitor:
	idf.py monitor

target:
	idf.py set-target $(TARGET)

menu:
	idf.py menuconfig

size: 
	idf.py size

$(CONFIG)/ioto.json5: $(ADIR)/state/config/ioto.json5 $(FORCE)
	@echo "   [Install] App Ioto configuration"
	cp $(ADIR)/state/config/ioto.json5 $(CONFIG)

$(CONFIG)/device.json5: $(ADIR)/state/config/device.json5 $(FORCE)
	@echo "   [Install] device.json5"
	cp $(ADIR)/state/config/device.json5 $(CONFIG)

$(CONFIG)/schema.json5: $(ADIR)/state/config/schema.json5 $(FORCE)
	@echo "   [Install] schema.json5"
	cp $(ADIR)/state/config/schema.json5 $(CONFIG)

$(CONFIG)/db.json5: force
	@if [ -f $(ADIR)/state/config/db.json5 ] ; then \
		if [ ! -f $(CONFIG)/db.json5 -o $(ADIR)/state/config/db.json5 -nt $(CONFIG)/db.json5 ] ; then \
			echo "   [Install] db.json5" ; \
			cp $(ADIR)/state/config/db.json5 $(CONFIG)/db.json5 ; \
		fi ; \
	else \
		rm -f $(CONFIG)/db.json5 ; \
	fi

$(CONFIG)/web.json5: force
	@if [ -f $(ADIR)/state/config/web.json5 ] ; then \
		if [ ! -f $(CONFIG)/web.json5 -o $(ADIR)/state/config/web.json5 -nt $(CONFIG)/web.json5 ] ; then \
			echo "   [Install] web.json5" ; \
			cp $(ADIR)/state/config/web.json5 $(CONFIG)/web.json5 ; \
		fi ; \
	else \
		rm -f $(CONFIG)/web.json5 ; \
	fi

$(DB)/.keep: $(FORCE)
	@echo "   [Install] database directory"
	mkdir -p $(DB)
	touch $(DB)/.keep

$(BASE)/include/config.h: $(ADIR)/state/config/ioto.json5
	json --header services $(ADIR)/state/config/ioto.json5 >$(BASE)/include/config.h

#
#	Install the trust anchors, and the device certificate for the build profile. The profile is
#	the app's own "profile" setting unless overridden with "make PROFILE=name". The published test
#	certificate and key are installed only for the dev profile. Any other profile installs neither
#	and requires the certificate and key named by the profile's tls settings to be provisioned in
#	state/certs, and to differ from the published pair.
#
install-certs: $(CONFIG)/ioto.json5
	mkdir -p $(CERTS)
	cp $(BASE)/certs/roots.crt $(BASE)/certs/aws.crt $(CERTS)
	profile="$${PROFILE:-`json profile $(CONFIG)/ioto.json5 2>/dev/null`}" ; \
	if [ "$$profile" = "dev" ] ; then \
		cp $(BASE)/certs/test.crt $(BASE)/certs/test.key $(CERTS) ; \
		echo "   [Install] Test certificate and key: development only, never ship them" ; \
		exit 0 ; \
	fi ; \
	rm -f $(CERTS)/test.crt $(CERTS)/test.key ; \
	for prop in certificate key ; do \
		name=`json conditional.profile.$$profile.tls.$$prop $(CONFIG)/ioto.json5 2>/dev/null || \
			json tls.$$prop $(CONFIG)/ioto.json5 2>/dev/null` ; \
		file=`echo "$$name" | sed 's|^@certs/|$(CERTS)/|'` ; \
		if [ -z "$$name" -o ! -f "$$file" ] ; then \
			echo "PROFILE=$$profile: provision the device tls.$$prop in state/certs and name it in ioto.json5" ; \
			echo "before building. The published test certificate is installed only for PROFILE=dev." ; \
			exit 2 ; \
		fi ; \
		if cmp -s "$$file" $(BASE)/certs/test.crt || cmp -s "$$file" $(BASE)/certs/test.key ; then \
			echo "PROFILE=$$profile: $$name is the published test $$prop. Provision a device identity." ; \
			exit 2 ; \
		fi ; \
	done ; \
	echo "   [Install] Device certificate for profile $$profile"

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

$(TOP)/sdkconfig.defaults: $(EDIR)/sdkconfig.defaults $(FORCE)
	@echo "   [Install] App sdkconfig.defaults from $(EDIR)"
	cp -r $(EDIR)/sdkconfig.defaults $(TOP)
	rm -f sdkconfig
	@echo "  [Generate] sdkconfig ..."
	idf.py reconfigure >/dev/null 2>&1

install-site: $(FORCE)
	@if [ -d $(BASE)/apps/$(APP)/site ] ; then \
		mkdir -p $(SITE) ; \
		cp -r $(BASE)/apps/$(APP)/site/* $(SITE) ; \
		echo "   [Install] App web site" ; \
	fi

debug:
	openocd -f board/esp32s3-builtin.cfg \
		-c "adapter speed 5000" \
		-c "init" \
		-c "reset init"

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
