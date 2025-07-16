#
#   ioto-linux-default.mk -- Makefile to build Ioto for linux
#

NAME                  := ioto
VERSION               := 2.7.1
PROFILE               ?= default
ARCH                  ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/mips.*/mips/')
CC_ARCH               ?= $(shell echo $(ARCH) | sed 's/x86/i686/;s/x64/x86_64/')
OS                    ?= linux
CC                    ?= gcc
AR                    ?= ar
BUILD                 ?= build/$(OS)-$(ARCH)-$(PROFILE)
CONFIG                ?= $(OS)-$(ARCH)-$(PROFILE)
LBIN                  ?= $(BUILD)/bin
PATH                  := $(LBIN):$(PATH)

#
# Components
#
ME_COM_AI             ?= 0
ME_COM_APPS           ?= 1
ME_COM_AUTH           ?= 0
ME_COM_BLANK          ?= 0
ME_COM_BLINK          ?= 0
ME_COM_COMPILER       ?= 1
ME_COM_DB             ?= 1
ME_COM_DEMO           ?= 0
ME_COM_IOTO           ?= 1
ME_COM_JSON           ?= 1
ME_COM_LIB            ?= 1
ME_COM_MBEDTLS        ?= 0
ME_COM_MQTT           ?= 1
ME_COM_OPENAI         ?= 1
ME_COM_OPENSSL        ?= 1
ME_COM_OSDEP          ?= 1
ME_COM_R              ?= 1
ME_COM_SSL            ?= 1
ME_COM_UCTX           ?= 1
ME_COM_UNIT           ?= 0
ME_COM_URL            ?= 1
ME_COM_VXWORKS        ?= 0
ME_COM_WEB            ?= 1
ME_COM_WEBSOCKETS     ?= 1


ifeq ($(ME_COM_AI),1)
    ME_COM_APPS := 1
endif
ifeq ($(ME_COM_AUTH),1)
    ME_COM_APPS := 1
endif
ifeq ($(ME_COM_BLANK),1)
    ME_COM_APPS := 1
endif
ifeq ($(ME_COM_BLINK),1)
    ME_COM_APPS := 1
endif
ifeq ($(ME_COM_DEMO),1)
    ME_COM_APPS := 1
endif
ifeq ($(ME_COM_LIB),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_MBEDTLS),1)
    ME_COM_SSL := 1
endif
ifeq ($(ME_COM_OPENSSL),1)
    ME_COM_SSL := 1
endif
ifeq ($(ME_COM_UNIT),1)
    ME_COM_APPS := 1
endif
ifeq ($(ME_COM_IOTO),1)
    ME_COM_APPS := 1
endif

#
# Settings
#
ME_APP                ?= \"demo\"
ME_AUTHOR             ?= \"Embedthis Software.\"
ME_COMPANY            ?= \"embedthis\"
ME_COMPATIBLE         ?= \"2.7\"
ME_COMPILER_HAS_ATOMIC ?= 1
ME_COMPILER_HAS_ATOMIC64 ?= 1
ME_COMPILER_HAS_DOUBLE_BRACES ?= 1
ME_COMPILER_HAS_DYN_LOAD ?= 1
ME_COMPILER_HAS_LIB_EDIT ?= 0
ME_COMPILER_HAS_LIB_RT ?= 1
ME_COMPILER_HAS_MMU   ?= 1
ME_COMPILER_HAS_MTUNE ?= 1
ME_COMPILER_HAS_PAM   ?= 0
ME_COMPILER_HAS_STACK_PROTECTOR ?= 1
ME_COMPILER_HAS_SYNC  ?= 1
ME_COMPILER_HAS_SYNC64 ?= 1
ME_COMPILER_HAS_SYNC_CAS ?= 1
ME_COMPILER_HAS_UNNAMED_UNIONS ?= 1
ME_COMPILER_NOEXECSTACK ?= 1
ME_COMPILER_WARN64TO32 ?= 0
ME_COMPILER_WARN_UNUSED ?= 1
ME_CONFIGURE          ?= \"me -d -q -platform linux-x64-default -configure . -gen make\"
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
ME_VERSION            ?= \"2.7.1\"
ME_WEB_GROUP          ?= \"$(WEB_GROUP)\"
ME_WEB_USER           ?= \"$(WEB_USER)\"

CFLAGS                += -w
DFLAGS                +=  $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_COM_AI=$(ME_COM_AI) -DME_COM_APPS=$(ME_COM_APPS) -DME_COM_AUTH=$(ME_COM_AUTH) -DME_COM_BLANK=$(ME_COM_BLANK) -DME_COM_BLINK=$(ME_COM_BLINK) -DME_COM_COMPILER=$(ME_COM_COMPILER) -DME_COM_DB=$(ME_COM_DB) -DME_COM_DEMO=$(ME_COM_DEMO) -DME_COM_IOTO=$(ME_COM_IOTO) -DME_COM_JSON=$(ME_COM_JSON) -DME_COM_LIB=$(ME_COM_LIB) -DME_COM_MBEDTLS=$(ME_COM_MBEDTLS) -DME_COM_MQTT=$(ME_COM_MQTT) -DME_COM_OPENAI=$(ME_COM_OPENAI) -DME_COM_OPENSSL=$(ME_COM_OPENSSL) -DME_COM_OSDEP=$(ME_COM_OSDEP) -DME_COM_R=$(ME_COM_R) -DME_COM_SSL=$(ME_COM_SSL) -DME_COM_UCTX=$(ME_COM_UCTX) -DME_COM_UNIT=$(ME_COM_UNIT) -DME_COM_URL=$(ME_COM_URL) -DME_COM_VXWORKS=$(ME_COM_VXWORKS) -DME_COM_WEB=$(ME_COM_WEB) -DME_COM_WEBSOCKETS=$(ME_COM_WEBSOCKETS) 
IFLAGS                += "-I$(BUILD)/inc"
LDFLAGS               += -z noexecstack -g
LIBPATHS              += -L$(BUILD)/bin
LIBS                  += -lrt -ldl -lpthread -lm

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

ME_ROOT_PREFIX        ?= 
ME_BASE_PREFIX        ?= $(ME_ROOT_PREFIX)/usr/local
ME_DATA_PREFIX        ?= $(ME_ROOT_PREFIX)/
ME_STATE_PREFIX       ?= $(ME_ROOT_PREFIX)/var
ME_APP_PREFIX         ?= $(ME_BASE_PREFIX)/lib/$(NAME)
ME_VAPP_PREFIX        ?= $(ME_APP_PREFIX)/$(VERSION)
ME_BIN_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/bin
ME_INC_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/include
ME_LIB_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/lib
ME_MAN_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/share/man
ME_SBIN_PREFIX        ?= $(ME_ROOT_PREFIX)/usr/local/sbin
ME_ETC_PREFIX         ?= $(ME_ROOT_PREFIX)/etc/$(NAME)
ME_WEB_PREFIX         ?= $(ME_ROOT_PREFIX)/var/www/$(NAME)
ME_LOG_PREFIX         ?= $(ME_ROOT_PREFIX)/var/log/$(NAME)
ME_VLIB_PREFIX        ?= $(ME_ROOT_PREFIX)/var/lib/$(NAME)
ME_SPOOL_PREFIX       ?= $(ME_ROOT_PREFIX)/var/spool/$(NAME)
ME_CACHE_PREFIX       ?= $(ME_ROOT_PREFIX)/var/spool/$(NAME)/cache
ME_SRC_PREFIX         ?= $(ME_ROOT_PREFIX)$(NAME)-$(VERSION)

WEB_USER              ?= $(shell egrep 'www-data|_www|nobody' /etc/passwd | sed 's^:.*^^' |  tail -1)
WEB_GROUP             ?= $(shell egrep 'www-data|_www|nobody|nogroup' /etc/group | sed 's^:.*^^' |  tail -1)

TARGETS               += $(BUILD)/bin/db
TARGETS               += $(BUILD)/bin/ioto
TARGETS               += $(BUILD)/bin/json
TARGETS               += $(BUILD)/bin/password
TARGETS               += $(BUILD)/bin/webserver


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
	@[ ! -x $(BUILD)/bin ] && mkdir -p $(BUILD)/bin; true
	@[ ! -x $(BUILD)/inc ] && mkdir -p $(BUILD)/inc; true
	@[ ! -x $(BUILD)/obj ] && mkdir -p $(BUILD)/obj; true
	@[ ! -f $(BUILD)/inc/me.h ] && cp projects/ioto-linux-$(PROFILE)-me.h $(BUILD)/inc/me.h ; true
	@if ! diff $(BUILD)/inc/me.h projects/ioto-linux-$(PROFILE)-me.h >/dev/null ; then\
		cp projects/ioto-linux-$(PROFILE)-me.h $(BUILD)/inc/me.h  ; \
	fi; true
	@if [ -f "$(BUILD)/.makeflags" ] ; then \
		if [ "$(MAKEFLAGS)" != "`cat $(BUILD)/.makeflags`" ] ; then \
			echo "   [Warning] Make flags have changed since the last build" ; \
			echo "   [Warning] Previous build command: "`cat $(BUILD)/.makeflags`"" ; \
		fi ; \
	fi
	@echo "$(MAKEFLAGS)" >$(BUILD)/.makeflags

clean:
	rm -f "$(BUILD)/obj/aiApp.o"
	rm -f "$(BUILD)/obj/authApp.o"
	rm -f "$(BUILD)/obj/authUser.o"
	rm -f "$(BUILD)/obj/blank.o"
	rm -f "$(BUILD)/obj/cryptLib.o"
	rm -f "$(BUILD)/obj/db.o"
	rm -f "$(BUILD)/obj/dbLib.o"
	rm -f "$(BUILD)/obj/demoApp.o"
	rm -f "$(BUILD)/obj/iotoLib.o"
	rm -f "$(BUILD)/obj/json.o"
	rm -f "$(BUILD)/obj/jsonLib.o"
	rm -f "$(BUILD)/obj/main.o"
	rm -f "$(BUILD)/obj/mqttLib.o"
	rm -f "$(BUILD)/obj/openaiLib.o"
	rm -f "$(BUILD)/obj/password.o"
	rm -f "$(BUILD)/obj/rLib.o"
	rm -f "$(BUILD)/obj/uctxAssembly.o"
	rm -f "$(BUILD)/obj/uctxLib.o"
	rm -f "$(BUILD)/obj/unitApp.o"
	rm -f "$(BUILD)/obj/unitTests.o"
	rm -f "$(BUILD)/obj/urlLib.o"
	rm -f "$(BUILD)/obj/web.o"
	rm -f "$(BUILD)/obj/webLib.o"
	rm -f "$(BUILD)/obj/websocketsLib.o"
	rm -f "$(BUILD)/bin/db"
	rm -f "$(BUILD)/bin/ioto"
	rm -f "$(BUILD)/bin/json"
	rm -f "$(BUILD)/bin/libioto.a"
	rm -f "$(BUILD)/bin/password"
	rm -f "$(BUILD)/bin/webserver"

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
#   ioto-config.h
#
DEPS_2 += include/ioto-config.h

$(BUILD)/inc/ioto-config.h: $(DEPS_2)
	@echo '      [Copy] $(BUILD)/inc/ioto-config.h'
	mkdir -p "$(BUILD)/inc"
	cp include/ioto-config.h $(BUILD)/inc/ioto-config.h

#
#   osdep.h
#
DEPS_3 += include/osdep.h
DEPS_3 += $(BUILD)/inc/me.h

$(BUILD)/inc/osdep.h: $(DEPS_3)
	@echo '      [Copy] $(BUILD)/inc/osdep.h'
	mkdir -p "$(BUILD)/inc"
	cp include/osdep.h $(BUILD)/inc/osdep.h

#
#   r.h
#
DEPS_4 += include/r.h
DEPS_4 += $(BUILD)/inc/osdep.h

$(BUILD)/inc/r.h: $(DEPS_4)
	@echo '      [Copy] $(BUILD)/inc/r.h'
	mkdir -p "$(BUILD)/inc"
	cp include/r.h $(BUILD)/inc/r.h

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
#   crypt.h
#
DEPS_6 += include/crypt.h
DEPS_6 += $(BUILD)/inc/me.h
DEPS_6 += $(BUILD)/inc/r.h

$(BUILD)/inc/crypt.h: $(DEPS_6)
	@echo '      [Copy] $(BUILD)/inc/crypt.h'
	mkdir -p "$(BUILD)/inc"
	cp include/crypt.h $(BUILD)/inc/crypt.h

#
#   db.h
#
DEPS_7 += include/db.h
DEPS_7 += $(BUILD)/inc/json.h

$(BUILD)/inc/db.h: $(DEPS_7)
	@echo '      [Copy] $(BUILD)/inc/db.h'
	mkdir -p "$(BUILD)/inc"
	cp include/db.h $(BUILD)/inc/db.h

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
#   websockets.h
#
DEPS_9 += include/websockets.h
DEPS_9 += $(BUILD)/inc/me.h
DEPS_9 += $(BUILD)/inc/r.h
DEPS_9 += $(BUILD)/inc/crypt.h
DEPS_9 += $(BUILD)/inc/json.h

$(BUILD)/inc/websockets.h: $(DEPS_9)
	@echo '      [Copy] $(BUILD)/inc/websockets.h'
	mkdir -p "$(BUILD)/inc"
	cp include/websockets.h $(BUILD)/inc/websockets.h

#
#   url.h
#
DEPS_10 += include/url.h
DEPS_10 += $(BUILD)/inc/me.h
DEPS_10 += $(BUILD)/inc/r.h
DEPS_10 += $(BUILD)/inc/json.h
DEPS_10 += $(BUILD)/inc/websockets.h

$(BUILD)/inc/url.h: $(DEPS_10)
	@echo '      [Copy] $(BUILD)/inc/url.h'
	mkdir -p "$(BUILD)/inc"
	cp include/url.h $(BUILD)/inc/url.h

#
#   web.h
#
DEPS_11 += include/web.h
DEPS_11 += $(BUILD)/inc/me.h
DEPS_11 += $(BUILD)/inc/r.h
DEPS_11 += $(BUILD)/inc/json.h
DEPS_11 += $(BUILD)/inc/crypt.h
DEPS_11 += $(BUILD)/inc/websockets.h

$(BUILD)/inc/web.h: $(DEPS_11)
	@echo '      [Copy] $(BUILD)/inc/web.h'
	mkdir -p "$(BUILD)/inc"
	cp include/web.h $(BUILD)/inc/web.h

#
#   openai.h
#
DEPS_12 += include/openai.h
DEPS_12 += $(BUILD)/inc/me.h
DEPS_12 += $(BUILD)/inc/r.h
DEPS_12 += $(BUILD)/inc/json.h
DEPS_12 += $(BUILD)/inc/url.h

$(BUILD)/inc/openai.h: $(DEPS_12)
	@echo '      [Copy] $(BUILD)/inc/openai.h'
	mkdir -p "$(BUILD)/inc"
	cp include/openai.h $(BUILD)/inc/openai.h

#
#   ioto.h
#
DEPS_13 += include/ioto.h
DEPS_13 += $(BUILD)/inc/ioto-config.h
DEPS_13 += $(BUILD)/inc/r.h
DEPS_13 += $(BUILD)/inc/json.h
DEPS_13 += $(BUILD)/inc/crypt.h
DEPS_13 += $(BUILD)/inc/db.h
DEPS_13 += $(BUILD)/inc/mqtt.h
DEPS_13 += $(BUILD)/inc/url.h
DEPS_13 += $(BUILD)/inc/web.h
DEPS_13 += $(BUILD)/inc/websockets.h
DEPS_13 += $(BUILD)/inc/openai.h

$(BUILD)/inc/ioto.h: $(DEPS_13)
	@echo '      [Copy] $(BUILD)/inc/ioto.h'
	mkdir -p "$(BUILD)/inc"
	cp include/ioto.h $(BUILD)/inc/ioto.h

#
#   auth.h
#
DEPS_14 += apps/auth/src/auth.h
DEPS_14 += $(BUILD)/inc/me.h
DEPS_14 += $(BUILD)/inc/ioto.h

$(BUILD)/inc/auth.h: $(DEPS_14)
	@echo '      [Copy] $(BUILD)/inc/auth.h'
	mkdir -p "$(BUILD)/inc"
	cp apps/auth/src/auth.h $(BUILD)/inc/auth.h

#
#   gpio.h
#

$(BUILD)/inc/driver/gpio.h: $(DEPS_15)

#
#   gpio.h
#

$(BUILD)/inc/rom/gpio.h: $(DEPS_16)

#
#   uctx-defs.h
#
DEPS_17 += include/uctx-defs.h

$(BUILD)/inc/uctx-defs.h: $(DEPS_17)
	@echo '      [Copy] $(BUILD)/inc/uctx-defs.h'
	mkdir -p "$(BUILD)/inc"
	cp include/uctx-defs.h $(BUILD)/inc/uctx-defs.h

#
#   uctx-os.h
#
DEPS_18 += include/uctx-os.h

$(BUILD)/inc/uctx-os.h: $(DEPS_18)
	@echo '      [Copy] $(BUILD)/inc/uctx-os.h'
	mkdir -p "$(BUILD)/inc"
	cp include/uctx-os.h $(BUILD)/inc/uctx-os.h

#
#   uctx.h
#
DEPS_19 += include/uctx.h
DEPS_19 += $(BUILD)/inc/uctx-os.h

$(BUILD)/inc/uctx.h: $(DEPS_19)
	@echo '      [Copy] $(BUILD)/inc/uctx.h'
	mkdir -p "$(BUILD)/inc"
	cp include/uctx.h $(BUILD)/inc/uctx.h

#
#   unit.h
#
DEPS_20 += apps/unit/src/unit.h
DEPS_20 += $(BUILD)/inc/ioto.h

$(BUILD)/inc/unit.h: $(DEPS_20)
	@echo '      [Copy] $(BUILD)/inc/unit.h'
	mkdir -p "$(BUILD)/inc"
	cp apps/unit/src/unit.h $(BUILD)/inc/unit.h

#
#   aiApp.o
#
DEPS_21 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/aiApp.o: \
    apps/ai/src/aiApp.c $(DEPS_21)
	@echo '   [Compile] $(BUILD)/obj/aiApp.o'
	$(CC) -c -o $(BUILD)/obj/aiApp.o $(CFLAGS) $(DFLAGS) $(IFLAGS) apps/ai/src/aiApp.c

#
#   authApp.o
#
DEPS_22 += $(BUILD)/inc/auth.h

$(BUILD)/obj/authApp.o: \
    apps/auth/src/authApp.c $(DEPS_22)
	@echo '   [Compile] $(BUILD)/obj/authApp.o'
	$(CC) -c -o $(BUILD)/obj/authApp.o $(CFLAGS) $(DFLAGS) $(IFLAGS) apps/auth/src/authApp.c

#
#   authUser.o
#
DEPS_23 += $(BUILD)/inc/auth.h

$(BUILD)/obj/authUser.o: \
    apps/auth/src/authUser.c $(DEPS_23)
	@echo '   [Compile] $(BUILD)/obj/authUser.o'
	$(CC) -c -o $(BUILD)/obj/authUser.o $(CFLAGS) $(DFLAGS) $(IFLAGS) apps/auth/src/authUser.c

#
#   blank.o
#
DEPS_24 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/blank.o: \
    apps/blank/src/blank.c $(DEPS_24)
	@echo '   [Compile] $(BUILD)/obj/blank.o'
	$(CC) -c -o $(BUILD)/obj/blank.o $(CFLAGS) $(DFLAGS) $(IFLAGS) apps/blank/src/blank.c

#
#   cryptLib.o
#
DEPS_25 += $(BUILD)/inc/crypt.h

$(BUILD)/obj/cryptLib.o: \
    lib/cryptLib.c $(DEPS_25)
	@echo '   [Compile] $(BUILD)/obj/cryptLib.o'
	$(CC) -c -o $(BUILD)/obj/cryptLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/cryptLib.c

#
#   db.o
#
DEPS_26 += $(BUILD)/inc/r.h
DEPS_26 += $(BUILD)/inc/db.h

$(BUILD)/obj/db.o: \
    cmds/db.c $(DEPS_26)
	@echo '   [Compile] $(BUILD)/obj/db.o'
	$(CC) -c -o $(BUILD)/obj/db.o $(CFLAGS) $(DFLAGS) $(IFLAGS) cmds/db.c

#
#   dbLib.o
#
DEPS_27 += $(BUILD)/inc/db.h
DEPS_27 += $(BUILD)/inc/crypt.h

$(BUILD)/obj/dbLib.o: \
    lib/dbLib.c $(DEPS_27)
	@echo '   [Compile] $(BUILD)/obj/dbLib.o'
	$(CC) -c -o $(BUILD)/obj/dbLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/dbLib.c

#
#   demoApp.o
#
DEPS_28 += $(BUILD)/inc/ioto.h
DEPS_28 += $(BUILD)/inc/driver/gpio.h
DEPS_28 += $(BUILD)/inc/rom/gpio.h

$(BUILD)/obj/demoApp.o: \
    apps/demo/src/demoApp.c $(DEPS_28)
	@echo '   [Compile] $(BUILD)/obj/demoApp.o'
	$(CC) -c -o $(BUILD)/obj/demoApp.o $(CFLAGS) $(DFLAGS) $(IFLAGS) apps/demo/src/demoApp.c

#
#   iotoLib.o
#
DEPS_29 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/iotoLib.o: \
    lib/iotoLib.c $(DEPS_29)
	@echo '   [Compile] $(BUILD)/obj/iotoLib.o'
	$(CC) -c -o $(BUILD)/obj/iotoLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/iotoLib.c

#
#   json.o
#
DEPS_30 += $(BUILD)/inc/osdep.h
DEPS_30 += $(BUILD)/inc/r.h
DEPS_30 += $(BUILD)/inc/json.h

$(BUILD)/obj/json.o: \
    cmds/json.c $(DEPS_30)
	@echo '   [Compile] $(BUILD)/obj/json.o'
	$(CC) -c -o $(BUILD)/obj/json.o $(CFLAGS) $(DFLAGS) $(IFLAGS) cmds/json.c

#
#   jsonLib.o
#
DEPS_31 += $(BUILD)/inc/json.h

$(BUILD)/obj/jsonLib.o: \
    lib/jsonLib.c $(DEPS_31)
	@echo '   [Compile] $(BUILD)/obj/jsonLib.o'
	$(CC) -c -o $(BUILD)/obj/jsonLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/jsonLib.c

#
#   main.o
#
DEPS_32 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/main.o: \
    cmds/main.c $(DEPS_32)
	@echo '   [Compile] $(BUILD)/obj/main.o'
	$(CC) -c -o $(BUILD)/obj/main.o $(CFLAGS) $(DFLAGS) $(IFLAGS) cmds/main.c

#
#   mqttLib.o
#
DEPS_33 += $(BUILD)/inc/mqtt.h

$(BUILD)/obj/mqttLib.o: \
    lib/mqttLib.c $(DEPS_33)
	@echo '   [Compile] $(BUILD)/obj/mqttLib.o'
	$(CC) -c -o $(BUILD)/obj/mqttLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/mqttLib.c

#
#   openaiLib.o
#
DEPS_34 += $(BUILD)/inc/openai.h

$(BUILD)/obj/openaiLib.o: \
    lib/openaiLib.c $(DEPS_34)
	@echo '   [Compile] $(BUILD)/obj/openaiLib.o'
	$(CC) -c -o $(BUILD)/obj/openaiLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/openaiLib.c

#
#   password.o
#
DEPS_35 += $(BUILD)/inc/r.h
DEPS_35 += $(BUILD)/inc/crypt.h
DEPS_35 += $(BUILD)/inc/json.h

$(BUILD)/obj/password.o: \
    cmds/password.c $(DEPS_35)
	@echo '   [Compile] $(BUILD)/obj/password.o'
	$(CC) -c -o $(BUILD)/obj/password.o $(CFLAGS) $(DFLAGS) $(IFLAGS) cmds/password.c

#
#   rLib.o
#
DEPS_36 += $(BUILD)/inc/r.h

$(BUILD)/obj/rLib.o: \
    lib/rLib.c $(DEPS_36)
	@echo '   [Compile] $(BUILD)/obj/rLib.o'
	$(CC) -c -o $(BUILD)/obj/rLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/rLib.c

#
#   uctxAssembly.o
#
DEPS_37 += $(BUILD)/inc/uctx-os.h
DEPS_37 += $(BUILD)/inc/uctx-defs.h

$(BUILD)/obj/uctxAssembly.o: \
    lib/uctxAssembly.S $(DEPS_37)
	@echo '   [Compile] $(BUILD)/obj/uctxAssembly.o'
	$(CC) -c -o $(BUILD)/obj/uctxAssembly.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/uctxAssembly.S

#
#   uctxLib.o
#
DEPS_38 += $(BUILD)/inc/uctx.h
DEPS_38 += $(BUILD)/inc/uctx-defs.h

$(BUILD)/obj/uctxLib.o: \
    lib/uctxLib.c $(DEPS_38)
	@echo '   [Compile] $(BUILD)/obj/uctxLib.o'
	$(CC) -c -o $(BUILD)/obj/uctxLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/uctxLib.c

#
#   unitApp.o
#
DEPS_39 += $(BUILD)/inc/unit.h

$(BUILD)/obj/unitApp.o: \
    apps/unit/src/unitApp.c $(DEPS_39)
	@echo '   [Compile] $(BUILD)/obj/unitApp.o'
	$(CC) -c -o $(BUILD)/obj/unitApp.o $(CFLAGS) $(DFLAGS) $(IFLAGS) apps/unit/src/unitApp.c

#
#   unitTests.o
#
DEPS_40 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/unitTests.o: \
    apps/unit/src/unitTests.c $(DEPS_40)
	@echo '   [Compile] $(BUILD)/obj/unitTests.o'
	$(CC) -c -o $(BUILD)/obj/unitTests.o $(CFLAGS) $(DFLAGS) $(IFLAGS) apps/unit/src/unitTests.c

#
#   urlLib.o
#
DEPS_41 += $(BUILD)/inc/url.h
DEPS_41 += $(BUILD)/inc/websockets.h

$(BUILD)/obj/urlLib.o: \
    lib/urlLib.c $(DEPS_41)
	@echo '   [Compile] $(BUILD)/obj/urlLib.o'
	$(CC) -c -o $(BUILD)/obj/urlLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/urlLib.c

#
#   web.o
#
DEPS_42 += $(BUILD)/inc/web.h

$(BUILD)/obj/web.o: \
    cmds/web.c $(DEPS_42)
	@echo '   [Compile] $(BUILD)/obj/web.o'
	$(CC) -c -o $(BUILD)/obj/web.o $(CFLAGS) $(DFLAGS) $(IFLAGS) cmds/web.c

#
#   webLib.o
#
DEPS_43 += $(BUILD)/inc/web.h
DEPS_43 += $(BUILD)/inc/url.h

$(BUILD)/obj/webLib.o: \
    lib/webLib.c $(DEPS_43)
	@echo '   [Compile] $(BUILD)/obj/webLib.o'
	$(CC) -c -o $(BUILD)/obj/webLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/webLib.c

#
#   websocketsLib.o
#
DEPS_44 += $(BUILD)/inc/websockets.h
DEPS_44 += $(BUILD)/inc/crypt.h

$(BUILD)/obj/websocketsLib.o: \
    lib/websocketsLib.c $(DEPS_44)
	@echo '   [Compile] $(BUILD)/obj/websocketsLib.o'
	$(CC) -c -o $(BUILD)/obj/websocketsLib.o $(CFLAGS) $(DFLAGS) $(IFLAGS) lib/websocketsLib.c

#
#   libioto
#
DEPS_45 += $(BUILD)/inc/crypt.h
DEPS_45 += $(BUILD)/inc/db.h
DEPS_45 += $(BUILD)/inc/ioto-config.h
DEPS_45 += $(BUILD)/inc/ioto.h
DEPS_45 += $(BUILD)/inc/json.h
DEPS_45 += $(BUILD)/inc/me.h
DEPS_45 += $(BUILD)/inc/mqtt.h
DEPS_45 += $(BUILD)/inc/openai.h
DEPS_45 += $(BUILD)/inc/osdep.h
DEPS_45 += $(BUILD)/inc/r.h
DEPS_45 += $(BUILD)/inc/uctx-defs.h
DEPS_45 += $(BUILD)/inc/uctx-os.h
DEPS_45 += $(BUILD)/inc/uctx.h
DEPS_45 += $(BUILD)/inc/url.h
DEPS_45 += $(BUILD)/inc/web.h
DEPS_45 += $(BUILD)/inc/websockets.h
DEPS_45 += $(BUILD)/obj/cryptLib.o
DEPS_45 += $(BUILD)/obj/dbLib.o
DEPS_45 += $(BUILD)/obj/iotoLib.o
DEPS_45 += $(BUILD)/obj/jsonLib.o
DEPS_45 += $(BUILD)/obj/mqttLib.o
DEPS_45 += $(BUILD)/obj/openaiLib.o
DEPS_45 += $(BUILD)/obj/rLib.o
DEPS_45 += $(BUILD)/obj/uctxAssembly.o
DEPS_45 += $(BUILD)/obj/uctxLib.o
DEPS_45 += $(BUILD)/obj/urlLib.o
DEPS_45 += $(BUILD)/obj/webLib.o
DEPS_45 += $(BUILD)/obj/websocketsLib.o

$(BUILD)/bin/libioto.a: $(DEPS_45)
	@echo '      [Link] $(BUILD)/bin/libioto.a'
	$(AR) -cr $(BUILD)/bin/libioto.a "$(BUILD)/obj/cryptLib.o" "$(BUILD)/obj/dbLib.o" "$(BUILD)/obj/iotoLib.o" "$(BUILD)/obj/jsonLib.o" "$(BUILD)/obj/mqttLib.o" "$(BUILD)/obj/openaiLib.o" "$(BUILD)/obj/rLib.o" "$(BUILD)/obj/uctxAssembly.o" "$(BUILD)/obj/uctxLib.o" "$(BUILD)/obj/urlLib.o" "$(BUILD)/obj/webLib.o" "$(BUILD)/obj/websocketsLib.o"

ifeq ($(ME_COM_DB),1)
#
#   db
#
DEPS_46 += $(BUILD)/bin/libioto.a
DEPS_46 += $(BUILD)/obj/db.o

LIBS_46 += -lioto
ifeq ($(ME_COM_MBEDTLS),1)
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_46 += -lmbedtls
endif
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_46 += -lmbedcrypto
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_46 += -lmbedx509
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_46 += -lssl
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_46 += -lcrypto
endif

$(BUILD)/bin/db: $(DEPS_46)
	@echo '      [Link] $(BUILD)/bin/db'
	$(CC) -o $(BUILD)/bin/db $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/db.o" $(LIBPATHS_46) $(LIBS_46) $(LIBS_46) $(LIBS) $(LIBS) 
endif



ifeq ($(ME_COM_DEMO),1)
#
#   libdemo
#
DEPS_47 += $(BUILD)/obj/demoApp.o

$(BUILD)/bin/libapp.a: $(DEPS_47)
	@echo '      [Link] $(BUILD)/bin/libapp.a'
	$(AR) -cr $(BUILD)/bin/libapp.a "$(BUILD)/obj/demoApp.o"
endif

ifeq ($(ME_COM_AI),1)
#
#   libai
#
DEPS_48 += $(BUILD)/obj/aiApp.o

$(BUILD)/bin/libapp.a: $(DEPS_48)
	@echo '      [Link] $(BUILD)/bin/libapp.a'
	$(AR) -cr $(BUILD)/bin/libapp.a "$(BUILD)/obj/aiApp.o"
endif

ifeq ($(ME_COM_AUTH),1)
#
#   libauth
#
DEPS_49 += $(BUILD)/inc/auth.h
DEPS_49 += $(BUILD)/obj/authApp.o
DEPS_49 += $(BUILD)/obj/authUser.o

$(BUILD)/bin/libapp.a: $(DEPS_49)
	@echo '      [Link] $(BUILD)/bin/libapp.a'
	$(AR) -cr $(BUILD)/bin/libapp.a "$(BUILD)/obj/authApp.o" "$(BUILD)/obj/authUser.o"
endif

ifeq ($(ME_COM_BLANK),1)
#
#   libblank
#
DEPS_50 += $(BUILD)/obj/blank.o

$(BUILD)/bin/libapp.a: $(DEPS_50)
	@echo '      [Link] $(BUILD)/bin/libapp.a'
	$(AR) -cr $(BUILD)/bin/libapp.a "$(BUILD)/obj/blank.o"
endif

ifeq ($(ME_COM_UNIT),1)
#
#   libunit
#
DEPS_51 += $(BUILD)/inc/unit.h
DEPS_51 += $(BUILD)/obj/unitApp.o
DEPS_51 += $(BUILD)/obj/unitTests.o

$(BUILD)/bin/libapp.a: $(DEPS_51)
	@echo '      [Link] $(BUILD)/bin/libapp.a'
	$(AR) -cr $(BUILD)/bin/libapp.a "$(BUILD)/obj/unitApp.o" "$(BUILD)/obj/unitTests.o"
endif

ifeq ($(ME_COM_APPS),1)
endif

ifeq ($(ME_COM_IOTO),1)
#
#   ioto
#
DEPS_52 += $(BUILD)/bin/libioto.a
DEPS_52 += $(BUILD)/bin/libapp.a
ifeq ($(ME_COM_DEMO),1)
    DEPS_52 += $(BUILD)/bin/libapp.a
endif
ifeq ($(ME_COM_AI),1)
    DEPS_52 += $(BUILD)/bin/libapp.a
endif
ifeq ($(ME_COM_AUTH),1)
    DEPS_52 += $(BUILD)/bin/libapp.a
endif
ifeq ($(ME_COM_BLANK),1)
    DEPS_52 += $(BUILD)/bin/libapp.a
endif
ifeq ($(ME_COM_UNIT),1)
    DEPS_52 += $(BUILD)/bin/libapp.a
endif
DEPS_52 += $(BUILD)/obj/main.o

LIBS_52 += -lioto
ifeq ($(ME_COM_MBEDTLS),1)
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_52 += -lmbedtls
endif
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_52 += -lmbedcrypto
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_52 += -lmbedx509
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_52 += -lssl
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_52 += -lcrypto
endif

$(BUILD)/bin/ioto: $(DEPS_52)
	@echo '      [Link] $(BUILD)/bin/ioto'
	$(CC) -o $(BUILD)/bin/ioto $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/main.o" -lapp $(LIBPATHS_52) $(LIBS_52) $(LIBS_52) $(LIBS) -lapp -lapp $(LIBS) -lapp 
endif

ifeq ($(ME_COM_JSON),1)
#
#   json
#
DEPS_53 += $(BUILD)/bin/libioto.a
DEPS_53 += $(BUILD)/obj/json.o

LIBS_53 += -lioto
ifeq ($(ME_COM_MBEDTLS),1)
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_53 += -lmbedtls
endif
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_53 += -lmbedcrypto
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_53 += -lmbedx509
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_53 += -lssl
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_53 += -lcrypto
endif

$(BUILD)/bin/json: $(DEPS_53)
	@echo '      [Link] $(BUILD)/bin/json'
	$(CC) -o $(BUILD)/bin/json $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/json.o" $(LIBPATHS_53) $(LIBS_53) $(LIBS_53) $(LIBS) $(LIBS) 
endif

#
#   password
#
DEPS_54 += $(BUILD)/bin/libioto.a
DEPS_54 += $(BUILD)/obj/password.o

LIBS_54 += -lioto
ifeq ($(ME_COM_MBEDTLS),1)
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_54 += -lmbedtls
endif
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_54 += -lmbedcrypto
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_54 += -lmbedx509
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_54 += -lssl
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_54 += -lcrypto
endif

$(BUILD)/bin/password: $(DEPS_54)
	@echo '      [Link] $(BUILD)/bin/password'
	$(CC) -o $(BUILD)/bin/password $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/password.o" $(LIBPATHS_54) $(LIBS_54) $(LIBS_54) $(LIBS) $(LIBS) 

#
#   webserver
#
DEPS_55 += $(BUILD)/bin/libioto.a
DEPS_55 += $(BUILD)/obj/web.o

LIBS_55 += -lioto
ifeq ($(ME_COM_MBEDTLS),1)
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_55 += -lmbedtls
endif
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_55 += -lmbedcrypto
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_55 += -lmbedx509
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_55 += -lssl
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_55 += -lcrypto
endif

$(BUILD)/bin/webserver: $(DEPS_55)
	@echo '      [Link] $(BUILD)/bin/webserver'
	$(CC) -o $(BUILD)/bin/webserver $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/web.o" $(LIBPATHS_55) $(LIBS_55) $(LIBS_55) $(LIBS) $(LIBS) 

#
#   stop
#

stop: $(DEPS_56)

#
#   installBinary
#

installBinary: $(DEPS_57)
	mkdir -p "$(ME_APP_PREFIX)" ; \
	rm -f "$(ME_APP_PREFIX)/latest" ; \
	ln -s "$(VERSION)" "$(ME_APP_PREFIX)/latest" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	mkdir -p "$(ME_BIN_PREFIX)" ; \
	rm -f "$(ME_BIN_PREFIX)/ioto" ; \
	ln -s "$(ME_VAPP_PREFIX)/bin/ioto" "$(ME_BIN_PREFIX)/ioto" ; \
	mkdir -p "/var/lib/ioto" ; \
	chmod 750 "/var/lib/ioto" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp installs/uninstall.sh $(ME_VAPP_PREFIX)/bin/uninstall ; \
	chmod 755 "$(ME_VAPP_PREFIX)/bin/uninstall" ; \
	mkdir -p "/var/lib/ioto/certs" ; \
	cp certs/roots.crt /var/lib/ioto/certs/roots.crt ; \
	cp certs/aws.crt /var/lib/ioto/certs/aws.crt ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin/scripts" ; \
	cp scripts/update $(ME_VAPP_PREFIX)/bin/scripts/update ; \
	chmod 755 "$(ME_VAPP_PREFIX)/bin/scripts/update" ; \
	mkdir -p $(ME_WEB_PREFIX) ; cp -r state/site/* $(ME_WEB_PREFIX) ; \
	mkdir -p "$(ME_ETC_PREFIX)" ; \
	cp state/config/device.json5 $(ME_ETC_PREFIX)/device.json5 ; \
	cp state/config/schema.json5 $(ME_ETC_PREFIX)/schema.json5 ; \
	mkdir -p "$(ME_ETC_PREFIX)" ; \
	cp state/config/ioto.json5 $(ME_ETC_PREFIX)/ioto.json5 ; \
	mkdir -p /var/lib/ioto/db ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/inc" ; \
	cp $(BUILD)/inc/me.h $(ME_VAPP_PREFIX)/inc/me.h

#
#   start
#

start: $(DEPS_58)

#
#   install
#
DEPS_59 += stop
DEPS_59 += installBinary
DEPS_59 += start

install: $(DEPS_59)
	echo "      [Info] Ioto installed at $(ME_VAPP_PREFIX)" ; \
	echo "      [Info] Configuration directory $(ME_ETC_PREFIX)" ; \
	echo "      [Info] Documents directory $(ME_WEB_PREFIX)" ; \
	echo "      [Info] Executables directory $(ME_VAPP_PREFIX)/bin" ; \
	echo '      [Info] Use "man ioto" for usage' ; \
	echo "      [Info] Run via 'cd $(ME_ETC_PREFIX) ; sudo ioto'" ; \
	bin/json profile=prod $(ME_ETC_PREFIX)/ioto.json5

#
#   installPrep
#

installPrep: $(DEPS_60)
	if [ "`id -u`" != 0 ] ; \
	then echo "Must run as root. Rerun with sudo." ; \
	exit 255 ; \
	fi

#
#   uninstall
#
DEPS_61 += stop

uninstall: $(DEPS_61)
	( \
	cd installs; \
	rm -f "$(ME_ETC_PREFIX)/appweb.conf" ; \
	rm -f "$(ME_ETC_PREFIX)/esp.conf" ; \
	rm -f "$(ME_ETC_PREFIX)/mine.types" ; \
	rm -f "$(ME_ETC_PREFIX)/install.conf" ; \
	rm -fr "$(ME_INC_PREFIX)/ioto" ; \
	)

#
#   uninstallBinary
#

uninstallBinary: $(DEPS_62)

#
#   version
#

version: $(DEPS_63)
	echo $(VERSION)


EXTRA_MAKEFILE := $(strip $(wildcard ./projects/extra.mk))
ifneq ($(EXTRA_MAKEFILE),)
include $(EXTRA_MAKEFILE)
endif
