#
#   ioto-vxworks-default.mk -- Makefile to build Ioto for vxworks
#

NAME                  := ioto
VERSION               := 2.5.0
PROFILE               ?= default
ARCH                  ?= $(shell echo $(WIND_HOST_TYPE) | sed 's/-.*$(ME_ROOT_PREFIX)/')
CPU                   ?= $(subst X86,PENTIUM,$(shell echo $(ARCH) | tr a-z A-Z))
OS                    ?= vxworks
CC                    ?= cc$(subst x86,pentium,$(ARCH))
LD                    ?= ldundefined
AR                    ?= arundefined
BUILD                 ?= build/$(OS)-$(ARCH)-$(PROFILE)
CONFIG                ?= $(OS)-$(ARCH)-$(PROFILE)
LBIN                  ?= $(BUILD)/bin
PATH                  := $(LBIN):$(PATH)

#
# Components
#
ME_COM_COMPILER       ?= 1
ME_COM_DB             ?= 1
ME_COM_IOTO           ?= 1
ME_COM_JSON           ?= 1
ME_COM_LIB            ?= 1
ME_COM_LINK           ?= 1
ME_COM_MBEDTLS        ?= 0
ME_COM_MQTT           ?= 1
ME_COM_OPENSSL        ?= 1
ME_COM_OSDEP          ?= 1
ME_COM_R              ?= 1
ME_COM_SSL            ?= 1
ME_COM_UCTX           ?= 1
ME_COM_URL            ?= 1
ME_COM_VXWORKS        ?= 0
ME_COM_WEB            ?= 1

ME_COM_OPENSSL_PATH   ?= "/path/to/openssl"

ifeq ($(ME_COM_LIB),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_LINK),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_MBEDTLS),1)
    ME_COM_SSL := 1
endif
ifeq ($(ME_COM_OPENSSL),1)
    ME_COM_SSL := 1
endif

#
# Settings
#
ME_AUTHOR             ?= \"Embedthis Software.\"
ME_COMPANY            ?= \"embedthis\"
ME_COMPATIBLE         ?= \"2.5\"
ME_COMPILER_HAS_ATOMIC ?= 0
ME_COMPILER_HAS_ATOMIC64 ?= 0
ME_COMPILER_HAS_DOUBLE_BRACES ?= 0
ME_COMPILER_HAS_DYN_LOAD ?= 1
ME_COMPILER_HAS_LIB_EDIT ?= 0
ME_COMPILER_HAS_LIB_RT ?= 0
ME_COMPILER_HAS_MMU   ?= 1
ME_COMPILER_HAS_MTUNE ?= 0
ME_COMPILER_HAS_PAM   ?= 0
ME_COMPILER_HAS_STACK_PROTECTOR ?= 1
ME_COMPILER_HAS_SYNC  ?= 0
ME_COMPILER_HAS_SYNC64 ?= 0
ME_COMPILER_HAS_SYNC_CAS ?= 0
ME_COMPILER_HAS_UNNAMED_UNIONS ?= 1
ME_COMPILER_WARN64TO32 ?= 0
ME_COMPILER_WARN_UNUSED ?= 0
ME_CONFIGURE          ?= \"me -d -q -platform vxworks-arm-default -configure . -gen make\"
ME_CONFIGURED         ?= 1
ME_DEBUG              ?= 1
ME_DEPTH              ?= 1
ME_DESCRIPTION        ?= \"Ioto Device agent\"
ME_GROUP              ?= \"ioto\"
ME_MANIFEST           ?= \"installs/manifest.me\"
ME_NAME               ?= \"ioto\"
ME_PARTS              ?= \"undefined\"
ME_PLATFORMS          ?= \"local\"
ME_PREFIXES           ?= \"install-prefixes\"
ME_STATIC             ?= 1
ME_TITLE              ?= \"Ioto\"
ME_TLS                ?= \"openssl\"
ME_TUNE               ?= \"size\"
ME_USER               ?= \"ioto\"
ME_VERSION            ?= \"2.5.0\"
ME_WEB_GROUP          ?= \"$(WEB_GROUP)\"
ME_WEB_USER           ?= \"$(WEB_USER)\"

export PATH           := $(WIND_GNU_PATH)/$(WIND_HOST_TYPE)/bin:$(PATH)
CFLAGS                += -fomit-frame-pointer -fno-builtin -fno-defer-pop -fvolatile  -w
DFLAGS                += -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h" $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_COM_COMPILER=$(ME_COM_COMPILER) -DME_COM_DB=$(ME_COM_DB) -DME_COM_IOTO=$(ME_COM_IOTO) -DME_COM_JSON=$(ME_COM_JSON) -DME_COM_LIB=$(ME_COM_LIB) -DME_COM_LINK=$(ME_COM_LINK) -DME_COM_MBEDTLS=$(ME_COM_MBEDTLS) -DME_COM_MQTT=$(ME_COM_MQTT) -DME_COM_OPENSSL=$(ME_COM_OPENSSL) -DME_COM_OSDEP=$(ME_COM_OSDEP) -DME_COM_R=$(ME_COM_R) -DME_COM_SSL=$(ME_COM_SSL) -DME_COM_UCTX=$(ME_COM_UCTX) -DME_COM_URL=$(ME_COM_URL) -DME_COM_VXWORKS=$(ME_COM_VXWORKS) -DME_COM_WEB=$(ME_COM_WEB) 
IFLAGS                += "-I$(BUILD)/inc"
LDFLAGS               += -Wl,-r
LIBPATHS              += -L$(BUILD)/bin
LIBS                  += -lgcc

OPTIMIZE              ?= debug
CFLAGS-debug          ?= -g
DFLAGS-debug          ?= -DME_DEBUG=1
LDFLAGS-debug         ?= -g
DFLAGS-release        ?= 
CFLAGS-release        ?= -O2
LDFLAGS-release       ?= 
CFLAGS                += $(CFLAGS-$(OPTIMIZE))
DFLAGS                += $(DFLAGS-$(OPTIMIZE))
LDFLAGS               += $(LDFLAGS-$(OPTIMIZE))

ME_ROOT_PREFIX        ?= deploy
ME_BASE_PREFIX        ?= $(ME_ROOT_PREFIX)
ME_DATA_PREFIX        ?= $(ME_VAPP_PREFIX)
ME_STATE_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_BIN_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_INC_PREFIX         ?= $(ME_VAPP_PREFIX)/inc
ME_LIB_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_MAN_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_SBIN_PREFIX        ?= $(ME_VAPP_PREFIX)
ME_ETC_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_WEB_PREFIX         ?= $(ME_VAPP_PREFIX)/web
ME_LOG_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_SPOOL_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_CACHE_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_APP_PREFIX         ?= $(ME_BASE_PREFIX)
ME_VAPP_PREFIX        ?= $(ME_APP_PREFIX)
ME_SRC_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/src/$(NAME)-$(VERSION)

WEB_USER              ?= $(shell egrep 'www-data|_www|nobody' /etc/passwd | sed 's^:.*^^' |  tail -1)
WEB_GROUP             ?= $(shell egrep 'www-data|_www|nobody|nogroup' /etc/group | sed 's^:.*^^' |  tail -1)

TARGETS               += $(BUILD)/bin/db.out
TARGETS               += $(BUILD)/bin/ioto.out
TARGETS               += $(BUILD)/bin/json.out
TARGETS               += $(BUILD)/bin/password.out
TARGETS               += $(BUILD)/bin/webserver.out


DEPEND := $(strip $(wildcard ./projects/depend.mk))
ifneq ($(DEPEND),)
include $(DEPEND)
endif

unexport CDPATH

ifndef SHOW
.SILENT:
endif

all build compile: prep $(TARGETS)

.PHONY: prep

prep:
	@if [ "$(BUILD)" = "" ] ; then echo WARNING: BUILD not set ; exit 255 ; fi
	@if [ "$(ME_APP_PREFIX)" = "" ] ; then echo WARNING: ME_APP_PREFIX not set ; exit 255 ; fi
	@if [ "$(WIND_BASE)" = "" ] ; then echo WARNING: WIND_BASE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_HOST_TYPE)" = "" ] ; then echo WARNING: WIND_HOST_TYPE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_GNU_PATH)" = "" ] ; then echo WARNING: WIND_GNU_PATH not set. Run wrenv.sh. ; exit 255 ; fi
	@[ ! -x $(BUILD)/bin ] && mkdir -p $(BUILD)/bin; true
	@[ ! -x $(BUILD)/inc ] && mkdir -p $(BUILD)/inc; true
	@[ ! -x $(BUILD)/obj ] && mkdir -p $(BUILD)/obj; true
	@[ ! -f $(BUILD)/inc/me.h ] && cp projects/ioto-vxworks-$(PROFILE)-me.h $(BUILD)/inc/me.h ; true
	@if ! diff $(BUILD)/inc/me.h projects/ioto-vxworks-$(PROFILE)-me.h >/dev/null ; then\
		cp projects/ioto-vxworks-$(PROFILE)-me.h $(BUILD)/inc/me.h  ; \
	fi; true
	@if [ -f "$(BUILD)/.makeflags" ] ; then \
		if [ "$(MAKEFLAGS)" != "`cat $(BUILD)/.makeflags`" ] ; then \
			echo "   [Warning] Make flags have changed since the last build" ; \
			echo "   [Warning] Previous build command: "`cat $(BUILD)/.makeflags`"" ; \
		fi ; \
	fi
	@echo "$(MAKEFLAGS)" >$(BUILD)/.makeflags

clean:
	rm -f "$(BUILD)/obj/cryptLib.o"
	rm -f "$(BUILD)/obj/db.o"
	rm -f "$(BUILD)/obj/dbLib.o"
	rm -f "$(BUILD)/obj/iotoLib.o"
	rm -f "$(BUILD)/obj/json.o"
	rm -f "$(BUILD)/obj/jsonLib.o"
	rm -f "$(BUILD)/obj/main.o"
	rm -f "$(BUILD)/obj/mqttLib.o"
	rm -f "$(BUILD)/obj/password.o"
	rm -f "$(BUILD)/obj/rLib.o"
	rm -f "$(BUILD)/obj/start.o"
	rm -f "$(BUILD)/obj/uctxAssembly.o"
	rm -f "$(BUILD)/obj/uctxLib.o"
	rm -f "$(BUILD)/obj/urlLib.o"
	rm -f "$(BUILD)/obj/webLib.o"
	rm -f "$(BUILD)/obj/webserver.o"
	rm -f "$(BUILD)/bin/db.out"
	rm -f "$(BUILD)/bin/ioto.out"
	rm -f "$(BUILD)/bin/json.out"
	rm -f "$(BUILD)/bin/libioto.a"
	rm -f "$(BUILD)/bin/password.out"
	rm -f "$(BUILD)/bin/webserver.out"

clobber: clean
	rm -fr ./$(BUILD)

#
#   me.h
#
DEPS_1 += include/me.h

$(BUILD)/inc/me.h: $(DEPS_1)
	@echo '      [Copy] $(BUILD)/inc/me.h'
	mkdir -p "$(BUILD)/inc"
	cp include/me.h $(BUILD)/inc/me.h

#
#   osdep.h
#
DEPS_2 += include/osdep.h
DEPS_2 += $(BUILD)/inc/me.h

$(BUILD)/inc/osdep.h: $(DEPS_2)
	@echo '      [Copy] $(BUILD)/inc/osdep.h'
	mkdir -p "$(BUILD)/inc"
	cp include/osdep.h $(BUILD)/inc/osdep.h

#
#   r.h
#
DEPS_3 += include/r.h
DEPS_3 += $(BUILD)/inc/osdep.h

$(BUILD)/inc/r.h: $(DEPS_3)
	@echo '      [Copy] $(BUILD)/inc/r.h'
	mkdir -p "$(BUILD)/inc"
	cp include/r.h $(BUILD)/inc/r.h

#
#   crypt.h
#
DEPS_4 += include/crypt.h
DEPS_4 += $(BUILD)/inc/me.h
DEPS_4 += $(BUILD)/inc/r.h

$(BUILD)/inc/crypt.h: $(DEPS_4)
	@echo '      [Copy] $(BUILD)/inc/crypt.h'
	mkdir -p "$(BUILD)/inc"
	cp include/crypt.h $(BUILD)/inc/crypt.h

#
#   json.h
#
DEPS_5 += include/json.h
DEPS_5 += $(BUILD)/inc/r.h

$(BUILD)/inc/json.h: $(DEPS_5)
	@echo '      [Copy] $(BUILD)/inc/json.h'
	mkdir -p "$(BUILD)/inc"
	cp include/json.h $(BUILD)/inc/json.h

#
#   db.h
#
DEPS_6 += include/db.h
DEPS_6 += $(BUILD)/inc/json.h

$(BUILD)/inc/db.h: $(DEPS_6)
	@echo '      [Copy] $(BUILD)/inc/db.h'
	mkdir -p "$(BUILD)/inc"
	cp include/db.h $(BUILD)/inc/db.h

#
#   ioto-config.h
#
DEPS_7 += include/ioto-config.h

$(BUILD)/inc/ioto-config.h: $(DEPS_7)
	@echo '      [Copy] $(BUILD)/inc/ioto-config.h'
	mkdir -p "$(BUILD)/inc"
	cp include/ioto-config.h $(BUILD)/inc/ioto-config.h

#
#   mqtt.h
#
DEPS_8 += include/mqtt.h
DEPS_8 += $(BUILD)/inc/me.h
DEPS_8 += $(BUILD)/inc/r.h

$(BUILD)/inc/mqtt.h: $(DEPS_8)
	@echo '      [Copy] $(BUILD)/inc/mqtt.h'
	mkdir -p "$(BUILD)/inc"
	cp include/mqtt.h $(BUILD)/inc/mqtt.h

#
#   url.h
#
DEPS_9 += include/url.h
DEPS_9 += $(BUILD)/inc/me.h
DEPS_9 += $(BUILD)/inc/r.h
DEPS_9 += $(BUILD)/inc/json.h

$(BUILD)/inc/url.h: $(DEPS_9)
	@echo '      [Copy] $(BUILD)/inc/url.h'
	mkdir -p "$(BUILD)/inc"
	cp include/url.h $(BUILD)/inc/url.h

#
#   web.h
#
DEPS_10 += include/web.h
DEPS_10 += $(BUILD)/inc/me.h
DEPS_10 += $(BUILD)/inc/r.h
DEPS_10 += $(BUILD)/inc/json.h
DEPS_10 += $(BUILD)/inc/crypt.h

$(BUILD)/inc/web.h: $(DEPS_10)
	@echo '      [Copy] $(BUILD)/inc/web.h'
	mkdir -p "$(BUILD)/inc"
	cp include/web.h $(BUILD)/inc/web.h

#
#   ioto.h
#
DEPS_11 += include/ioto.h
DEPS_11 += $(BUILD)/inc/ioto-config.h
DEPS_11 += $(BUILD)/inc/r.h
DEPS_11 += $(BUILD)/inc/json.h
DEPS_11 += $(BUILD)/inc/crypt.h
DEPS_11 += $(BUILD)/inc/db.h
DEPS_11 += $(BUILD)/inc/mqtt.h
DEPS_11 += $(BUILD)/inc/url.h
DEPS_11 += $(BUILD)/inc/web.h

$(BUILD)/inc/ioto.h: $(DEPS_11)
	@echo '      [Copy] $(BUILD)/inc/ioto.h'
	mkdir -p "$(BUILD)/inc"
	cp include/ioto.h $(BUILD)/inc/ioto.h

#
#   uctx-defs.h
#
DEPS_12 += include/uctx-defs.h

$(BUILD)/inc/uctx-defs.h: $(DEPS_12)
	@echo '      [Copy] $(BUILD)/inc/uctx-defs.h'
	mkdir -p "$(BUILD)/inc"
	cp include/uctx-defs.h $(BUILD)/inc/uctx-defs.h

#
#   uctx-os.h
#
DEPS_13 += include/uctx-os.h

$(BUILD)/inc/uctx-os.h: $(DEPS_13)
	@echo '      [Copy] $(BUILD)/inc/uctx-os.h'
	mkdir -p "$(BUILD)/inc"
	cp include/uctx-os.h $(BUILD)/inc/uctx-os.h

#
#   uctx.h
#
DEPS_14 += include/uctx.h
DEPS_14 += $(BUILD)/inc/uctx-os.h

$(BUILD)/inc/uctx.h: $(DEPS_14)
	@echo '      [Copy] $(BUILD)/inc/uctx.h'
	mkdir -p "$(BUILD)/inc"
	cp include/uctx.h $(BUILD)/inc/uctx.h

#
#   cryptLib.o
#
DEPS_15 += $(BUILD)/inc/crypt.h

$(BUILD)/obj/cryptLib.o: \
    lib/cryptLib.c $(DEPS_15)
	@echo '   [Compile] $(BUILD)/obj/cryptLib.o'
	$(CC) -c -o $(BUILD)/obj/cryptLib.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/cryptLib.c

#
#   db.o
#
DEPS_16 += $(BUILD)/inc/r.h
DEPS_16 += $(BUILD)/inc/db.h

$(BUILD)/obj/db.o: \
    cmds/db.c $(DEPS_16)
	@echo '   [Compile] $(BUILD)/obj/db.o'
	$(CC) -c -o $(BUILD)/obj/db.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" cmds/db.c

#
#   dbLib.o
#
DEPS_17 += $(BUILD)/inc/db.h
DEPS_17 += $(BUILD)/inc/crypt.h

$(BUILD)/obj/dbLib.o: \
    lib/dbLib.c $(DEPS_17)
	@echo '   [Compile] $(BUILD)/obj/dbLib.o'
	$(CC) -c -o $(BUILD)/obj/dbLib.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/dbLib.c

#
#   iotoLib.o
#
DEPS_18 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/iotoLib.o: \
    lib/iotoLib.c $(DEPS_18)
	@echo '   [Compile] $(BUILD)/obj/iotoLib.o'
	$(CC) -c -o $(BUILD)/obj/iotoLib.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/iotoLib.c

#
#   json.o
#
DEPS_19 += $(BUILD)/inc/osdep.h
DEPS_19 += $(BUILD)/inc/r.h
DEPS_19 += $(BUILD)/inc/json.h

$(BUILD)/obj/json.o: \
    cmds/json.c $(DEPS_19)
	@echo '   [Compile] $(BUILD)/obj/json.o'
	$(CC) -c -o $(BUILD)/obj/json.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" cmds/json.c

#
#   jsonLib.o
#
DEPS_20 += $(BUILD)/inc/json.h

$(BUILD)/obj/jsonLib.o: \
    lib/jsonLib.c $(DEPS_20)
	@echo '   [Compile] $(BUILD)/obj/jsonLib.o'
	$(CC) -c -o $(BUILD)/obj/jsonLib.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/jsonLib.c

#
#   main.o
#
DEPS_21 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/main.o: \
    cmds/main.c $(DEPS_21)
	@echo '   [Compile] $(BUILD)/obj/main.o'
	$(CC) -c -o $(BUILD)/obj/main.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" cmds/main.c

#
#   mqttLib.o
#
DEPS_22 += $(BUILD)/inc/mqtt.h

$(BUILD)/obj/mqttLib.o: \
    lib/mqttLib.c $(DEPS_22)
	@echo '   [Compile] $(BUILD)/obj/mqttLib.o'
	$(CC) -c -o $(BUILD)/obj/mqttLib.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/mqttLib.c

#
#   password.o
#
DEPS_23 += $(BUILD)/inc/r.h
DEPS_23 += $(BUILD)/inc/crypt.h
DEPS_23 += $(BUILD)/inc/json.h

$(BUILD)/obj/password.o: \
    cmds/password.c $(DEPS_23)
	@echo '   [Compile] $(BUILD)/obj/password.o'
	$(CC) -c -o $(BUILD)/obj/password.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" cmds/password.c

#
#   rLib.o
#
DEPS_24 += $(BUILD)/inc/r.h

$(BUILD)/obj/rLib.o: \
    lib/rLib.c $(DEPS_24)
	@echo '   [Compile] $(BUILD)/obj/rLib.o'
	$(CC) -c -o $(BUILD)/obj/rLib.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/rLib.c

#
#   start.o
#
DEPS_25 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/start.o: \
    cmds/start.c $(DEPS_25)
	@echo '   [Compile] $(BUILD)/obj/start.o'
	$(CC) -c -o $(BUILD)/obj/start.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" cmds/start.c

#
#   uctxAssembly.o
#
DEPS_26 += $(BUILD)/inc/uctx-os.h
DEPS_26 += $(BUILD)/inc/uctx-defs.h

$(BUILD)/obj/uctxAssembly.o: \
    lib/uctxAssembly.S $(DEPS_26)
	@echo '   [Compile] $(BUILD)/obj/uctxAssembly.o'
	$(CC) -c -o $(BUILD)/obj/uctxAssembly.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/uctxAssembly.S

#
#   uctxLib.o
#
DEPS_27 += $(BUILD)/inc/uctx.h
DEPS_27 += $(BUILD)/inc/uctx-defs.h

$(BUILD)/obj/uctxLib.o: \
    lib/uctxLib.c $(DEPS_27)
	@echo '   [Compile] $(BUILD)/obj/uctxLib.o'
	$(CC) -c -o $(BUILD)/obj/uctxLib.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/uctxLib.c

#
#   urlLib.o
#
DEPS_28 += $(BUILD)/inc/url.h

$(BUILD)/obj/urlLib.o: \
    lib/urlLib.c $(DEPS_28)
	@echo '   [Compile] $(BUILD)/obj/urlLib.o'
	$(CC) -c -o $(BUILD)/obj/urlLib.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/urlLib.c

#
#   webLib.o
#
DEPS_29 += $(BUILD)/inc/web.h
DEPS_29 += $(BUILD)/inc/url.h

$(BUILD)/obj/webLib.o: \
    lib/webLib.c $(DEPS_29)
	@echo '   [Compile] $(BUILD)/obj/webLib.o'
	$(CC) -c -o $(BUILD)/obj/webLib.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" lib/webLib.c

#
#   webserver.o
#
DEPS_30 += $(BUILD)/inc/web.h

$(BUILD)/obj/webserver.o: \
    cmds/webserver.c $(DEPS_30)
	@echo '   [Compile] $(BUILD)/obj/webserver.o'
	$(CC) -c -o $(BUILD)/obj/webserver.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=ARMARCH7 -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" cmds/webserver.c

#
#   libioto
#
DEPS_31 += $(BUILD)/inc/crypt.h
DEPS_31 += $(BUILD)/inc/db.h
DEPS_31 += $(BUILD)/inc/ioto-config.h
DEPS_31 += $(BUILD)/inc/ioto.h
DEPS_31 += $(BUILD)/inc/json.h
DEPS_31 += $(BUILD)/inc/me.h
DEPS_31 += $(BUILD)/inc/mqtt.h
DEPS_31 += $(BUILD)/inc/osdep.h
DEPS_31 += $(BUILD)/inc/r.h
DEPS_31 += $(BUILD)/inc/uctx-defs.h
DEPS_31 += $(BUILD)/inc/uctx-os.h
DEPS_31 += $(BUILD)/inc/uctx.h
DEPS_31 += $(BUILD)/inc/url.h
DEPS_31 += $(BUILD)/inc/web.h
DEPS_31 += $(BUILD)/obj/cryptLib.o
DEPS_31 += $(BUILD)/obj/dbLib.o
DEPS_31 += $(BUILD)/obj/iotoLib.o
DEPS_31 += $(BUILD)/obj/jsonLib.o
DEPS_31 += $(BUILD)/obj/mqttLib.o
DEPS_31 += $(BUILD)/obj/rLib.o
DEPS_31 += $(BUILD)/obj/uctxAssembly.o
DEPS_31 += $(BUILD)/obj/uctxLib.o
DEPS_31 += $(BUILD)/obj/urlLib.o
DEPS_31 += $(BUILD)/obj/webLib.o

$(BUILD)/bin/libioto.a: $(DEPS_31)
	@echo '      [Link] $(BUILD)/bin/libioto.a'
	$(AR) -cr $(BUILD)/bin/libioto.a "$(BUILD)/obj/cryptLib.o" "$(BUILD)/obj/dbLib.o" "$(BUILD)/obj/iotoLib.o" "$(BUILD)/obj/jsonLib.o" "$(BUILD)/obj/mqttLib.o" "$(BUILD)/obj/rLib.o" "$(BUILD)/obj/uctxAssembly.o" "$(BUILD)/obj/uctxLib.o" "$(BUILD)/obj/urlLib.o" "$(BUILD)/obj/webLib.o"

ifeq ($(ME_COM_DB),1)
#
#   db
#
DEPS_32 += $(BUILD)/bin/libioto.a
DEPS_32 += $(BUILD)/obj/db.o

LIBS_32 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_32 += -lssl
    LIBPATHS_32 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_32 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_32 += -lcrypto
    LIBPATHS_32 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_32 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/db.out: $(DEPS_32)
	@echo '      [Link] $(BUILD)/bin/db.out'
	$(CC) -o $(BUILD)/bin/db.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/db.o" $(LIBPATHS_32) $(LIBS_32) $(LIBS_32) $(LIBS) -Wl,-r 
endif

ifeq ($(ME_COM_IOTO),1)
#
#   ioto
#
DEPS_33 += $(BUILD)/bin/libioto.a
DEPS_33 += $(BUILD)/obj/main.o
DEPS_33 += $(BUILD)/obj/start.o

LIBS_33 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_33 += -lssl
    LIBPATHS_33 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_33 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_33 += -lcrypto
    LIBPATHS_33 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_33 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/ioto.out: $(DEPS_33)
	@echo '      [Link] $(BUILD)/bin/ioto.out'
	$(CC) -o $(BUILD)/bin/ioto.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/main.o" "$(BUILD)/obj/start.o" $(LIBPATHS_33) $(LIBS_33) $(LIBS_33) $(LIBS) -Wl,-r 
endif

ifeq ($(ME_COM_JSON),1)
#
#   json
#
DEPS_34 += $(BUILD)/bin/libioto.a
DEPS_34 += $(BUILD)/obj/json.o

LIBS_34 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_34 += -lssl
    LIBPATHS_34 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_34 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_34 += -lcrypto
    LIBPATHS_34 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_34 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/json.out: $(DEPS_34)
	@echo '      [Link] $(BUILD)/bin/json.out'
	$(CC) -o $(BUILD)/bin/json.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/json.o" $(LIBPATHS_34) $(LIBS_34) $(LIBS_34) $(LIBS) -Wl,-r 
endif

#
#   password
#
DEPS_35 += $(BUILD)/bin/libioto.a
DEPS_35 += $(BUILD)/obj/password.o

LIBS_35 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_35 += -lssl
    LIBPATHS_35 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_35 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_35 += -lcrypto
    LIBPATHS_35 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_35 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/password.out: $(DEPS_35)
	@echo '      [Link] $(BUILD)/bin/password.out'
	$(CC) -o $(BUILD)/bin/password.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/password.o" $(LIBPATHS_35) $(LIBS_35) $(LIBS_35) $(LIBS) -Wl,-r 

#
#   webserver
#
DEPS_36 += $(BUILD)/bin/libioto.a
DEPS_36 += $(BUILD)/obj/webserver.o

LIBS_36 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_36 += -lssl
    LIBPATHS_36 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_36 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_36 += -lcrypto
    LIBPATHS_36 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_36 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/webserver.out: $(DEPS_36)
	@echo '      [Link] $(BUILD)/bin/webserver.out'
	$(CC) -o $(BUILD)/bin/webserver.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/webserver.o" $(LIBPATHS_36) $(LIBS_36) $(LIBS_36) $(LIBS) -Wl,-r 

#
#   stop
#

stop: $(DEPS_37)

#
#   installBinary
#

installBinary: $(DEPS_38)

#
#   start
#

start: $(DEPS_39)

#
#   install
#
DEPS_40 += stop
DEPS_40 += installBinary
DEPS_40 += start

install: $(DEPS_40)
	echo "      [Info] Ioto installed at " ; \
	echo "      [Info] Configuration directory " ; \
	echo "      [Info] Documents directory " ; \
	echo "      [Info] Executables directory " ; \
	echo '      [Info] Use "man ioto" for usage' ; \
	echo "      [Info] Run via 'cd  ; sudo ioto'"

#
#   installPrep
#

installPrep: $(DEPS_41)
	if [ "`id -u`" != 0 ] ; \
	then echo "Must run as root. Rerun with sudo." ; \
	exit 255 ; \
	fi

#
#   uninstall
#
DEPS_42 += stop

uninstall: $(DEPS_42)
	( \
	cd installs; \
	rm -f "$(ME_VAPP_PREFIX)/appweb.conf" ; \
	rm -f "$(ME_VAPP_PREFIX)/esp.conf" ; \
	rm -f "$(ME_VAPP_PREFIX)/mine.types" ; \
	rm -f "$(ME_VAPP_PREFIX)/install.conf" ; \
	rm -fr "$(ME_VAPP_PREFIX)/inc/ioto" ; \
	)

#
#   uninstallBinary
#

uninstallBinary: $(DEPS_43)

#
#   version
#

version: $(DEPS_44)
	echo $(VERSION)


EXTRA_MAKEFILE := $(strip $(wildcard ./projects/extra.mk))
ifneq ($(EXTRA_MAKEFILE),)
include $(EXTRA_MAKEFILE)
endif
