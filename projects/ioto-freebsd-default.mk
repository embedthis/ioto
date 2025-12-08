#
#   ioto-freebsd-default.mk -- Makefile to build Ioto for freebsd
#

NAME                  := ioto
VERSION               := 3.0.0
PROJECT               := ioto-freebsd-default
PROFILE               ?= dev
ARCH                  ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/mips.*/mips/')
CC_ARCH               ?= $(shell echo $(ARCH) | sed 's/x86/i686/;s/x64/x86_64/')
OS                    ?= freebsd
CC                    ?= gcc
AR                    ?= ar
BUILD                 ?= build
CONFIG                ?= $(OS)-$(ARCH)-$(PROFILE)
LBIN                  ?= $(BUILD)/bin
PATH                  := $(LBIN):$(PATH)

#
# Components
#
ME_COM_COMPILER       ?= 1
ME_COM_LIB            ?= 1
ME_COM_MBEDTLS        ?= 0
ME_COM_OPENSSL        ?= 1
ME_COM_SSL            ?= 1
ME_COM_VXWORKS        ?= 0

ME_COM_OPENSSL_PATH   ?= /path/to/openssl

ifeq ($(ME_COM_LIB),1)
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
ME_APP                ?= \"unit\"
ME_AUTHOR             ?= \"Embedthis Software.\"
ME_BUILD              ?= \"build\"
ME_COM_CRYPT          ?= 1
ME_COM_DB             ?= 1
ME_COM_JSON           ?= 1
ME_COM_MQTT           ?= 1
ME_COM_OPENAI         ?= 1
ME_COM_R              ?= 1
ME_COM_UCTX           ?= 1
ME_COM_URL            ?= 1
ME_COM_WEB            ?= 1
ME_COM_WEBSOCK        ?= 1
ME_COMPANY            ?= \"embedthis\"
ME_COMPATIBLE         ?= \"3.0\"
ME_COMPILER_FORTIFY   ?= 1
ME_COMPILER_HAS_ATOMIC ?= 1
ME_COMPILER_HAS_ATOMIC64 ?= 1
ME_COMPILER_HAS_DOUBLE_BRACES ?= 1
ME_COMPILER_HAS_DYN_LOAD ?= 1
ME_COMPILER_HAS_LIB_EDIT ?= 0
ME_COMPILER_HAS_LIB_RT ?= 0
ME_COMPILER_HAS_MMU   ?= 1
ME_COMPILER_HAS_MTUNE ?= 1
ME_COMPILER_HAS_PAM   ?= 0
ME_COMPILER_HAS_STACK_PROTECTOR ?= 1
ME_COMPILER_HAS_SYNC  ?= 0
ME_COMPILER_HAS_SYNC64 ?= 0
ME_COMPILER_HAS_SYNC_CAS ?= 0
ME_COMPILER_HAS_UNNAMED_UNIONS ?= 1
ME_COMPILER_NOEXECSTACK ?= 0
ME_COMPILER_WARN64TO32 ?= 0
ME_COMPILER_WARN_UNUSED ?= 0
ME_CONFIGURE          ?= \"me -d -q -platform freebsd-arm-default -configure . -gen make\"
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
ME_PROFILE            ?= \"dev\"
ME_STATIC             ?= 1
ME_TITLE              ?= \"Ioto\"
ME_TLS                ?= \"openssl\"
ME_TUNE               ?= \"size\"
ME_USER               ?= \"ioto\"
ME_VERSION            ?= \"3.0.0\"
ME_WEB_AUTH           ?= 1
ME_WEB_LIMITS         ?= 1
ME_WEB_SESSIONS       ?= 1
ME_WEB_UPLOAD         ?= 1
ME_WEB_GROUP          ?= \"$(WEB_GROUP)\"
ME_WEB_USER           ?= \"$(WEB_USER)\"

CFLAGS                += -Wall -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Wsign-compare -Wsign-conversion -Wl,-z,relro,-z,now -Wl,--as-needed -Wl,--no-copy-dt-needed-entries -Wl,-z,noexecheap -Wl,--no-warn-execstack -pie -fPIE -fomit-frame-pointer
DFLAGS                +=  $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) "-DME_COM_COMPILER=$(ME_COM_COMPILER)" "-DME_COM_LIB=$(ME_COM_LIB)" "-DME_COM_MBEDTLS=$(ME_COM_MBEDTLS)" "-DME_COM_OPENSSL=$(ME_COM_OPENSSL)" "-DME_COM_SSL=$(ME_COM_SSL)" "-DME_COM_VXWORKS=$(ME_COM_VXWORKS)" "-DME_COM_CRYPT=$(ME_COM_CRYPT)" "-DME_COM_DB=$(ME_COM_DB)" "-DME_COM_JSON=$(ME_COM_JSON)" "-DME_COM_MQTT=$(ME_COM_MQTT)" "-DME_COM_OPENAI=$(ME_COM_OPENAI)" "-DME_COM_R=$(ME_COM_R)" "-DME_COM_UCTX=$(ME_COM_UCTX)" "-DME_COM_URL=$(ME_COM_URL)" "-DME_COM_WEB=$(ME_COM_WEB)" "-DME_COM_WEBSOCK=$(ME_COM_WEBSOCK)" "-DME_WEB_AUTH=$(ME_WEB_AUTH)" "-DME_WEB_LIMITS=$(ME_WEB_LIMITS)" "-DME_WEB_SESSIONS=$(ME_WEB_SESSIONS)" "-DME_WEB_UPLOAD=$(ME_WEB_UPLOAD)" 
IFLAGS                += "-I$(BUILD)/inc"
LDFLAGS               += 
LIBPATHS              += "-L$(BUILD)/bin"
LIBS                  += "-ldl" "-lpthread" "-lm"

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

TARGETS               += app
TARGETS               += $(BUILD)/bin/db
TARGETS               += $(BUILD)/bin/ioto
TARGETS               += $(BUILD)/bin/json
TARGETS               += $(BUILD)/bin/password
TARGETS               += $(BUILD)/bin/url
TARGETS               += $(BUILD)/bin/web


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
	@[ ! -f $(BUILD)/inc/me.h ] && cp projects/$(PROJECT)-me.h $(BUILD)/inc/me.h ; true
	@if ! diff $(BUILD)/inc/me.h projects/$(PROJECT)-me.h >/dev/null ; then\
		cp projects/$(PROJECT)-me.h $(BUILD)/inc/me.h  ; \
	fi; true

clean:
	rm -f "$(BUILD)/obj/agent.o"
	rm -f "$(BUILD)/obj/ai.o"
	rm -f "$(BUILD)/obj/app.o"
	rm -f "$(BUILD)/obj/cloud.o"
	rm -f "$(BUILD)/obj/cloudwatch.o"
	rm -f "$(BUILD)/obj/config.o"
	rm -f "$(BUILD)/obj/cron.o"
	rm -f "$(BUILD)/obj/cryptLib.o"
	rm -f "$(BUILD)/obj/database.o"
	rm -f "$(BUILD)/obj/db.o"
	rm -f "$(BUILD)/obj/dbLib.o"
	rm -f "$(BUILD)/obj/esp32.o"
	rm -f "$(BUILD)/obj/helpers.o"
	rm -f "$(BUILD)/obj/ioto.o"
	rm -f "$(BUILD)/obj/json.o"
	rm -f "$(BUILD)/obj/jsonLib.o"
	rm -f "$(BUILD)/obj/logs.o"
	rm -f "$(BUILD)/obj/mqtt.o"
	rm -f "$(BUILD)/obj/mqttLib.o"
	rm -f "$(BUILD)/obj/openaiLib.o"
	rm -f "$(BUILD)/obj/password.o"
	rm -f "$(BUILD)/obj/provision.o"
	rm -f "$(BUILD)/obj/rLib.o"
	rm -f "$(BUILD)/obj/register.o"
	rm -f "$(BUILD)/obj/serialize.o"
	rm -f "$(BUILD)/obj/setup.o"
	rm -f "$(BUILD)/obj/shadow.o"
	rm -f "$(BUILD)/obj/sync.o"
	rm -f "$(BUILD)/obj/uctxAssembly.o"
	rm -f "$(BUILD)/obj/uctxLib.o"
	rm -f "$(BUILD)/obj/update.o"
	rm -f "$(BUILD)/obj/url.o"
	rm -f "$(BUILD)/obj/urlLib.o"
	rm -f "$(BUILD)/obj/web.o"
	rm -f "$(BUILD)/obj/webLib.o"
	rm -f "$(BUILD)/obj/webserver.o"
	rm -f "$(BUILD)/obj/websockLib.o"
	rm -f "$(BUILD)/bin/db"
	rm -f "$(BUILD)/bin/ioto"
	rm -f "$(BUILD)/bin/json"
	rm -f "$(BUILD)/bin/libioto.a"
	rm -f "$(BUILD)/bin/password"
	rm -f "$(BUILD)/bin/url"
	rm -f "$(BUILD)/bin/web"
	rm -rf state; rm -f include/config.h

clobber: clean
	rm -fr ./$(BUILD)

#
#   app
#

app: $(DEPS_1)
	bin/prepare $(APP)

#
#   config.h
#
DEPS_2 += include/config.h

$(BUILD)/inc/config.h: $(DEPS_2)
	@echo '      [Copy] $(BUILD)/inc/config.h'
	mkdir -p "$(BUILD)/inc"
	cp include/config.h $(BUILD)/inc/config.h

#
#   me.h
#

$(BUILD)/inc/me.h: $(DEPS_3)

#
#   osdep.h
#
DEPS_4 += include/osdep.h
DEPS_4 += $(BUILD)/inc/me.h

$(BUILD)/inc/osdep.h: $(DEPS_4)
	@echo '      [Copy] $(BUILD)/inc/osdep.h'
	mkdir -p "$(BUILD)/inc"
	cp include/osdep.h $(BUILD)/inc/osdep.h

#
#   r.h
#
DEPS_5 += include/r.h
DEPS_5 += $(BUILD)/inc/osdep.h

$(BUILD)/inc/r.h: $(DEPS_5)
	@echo '      [Copy] $(BUILD)/inc/r.h'
	mkdir -p "$(BUILD)/inc"
	cp include/r.h $(BUILD)/inc/r.h

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
#   json.h
#
DEPS_7 += include/json.h
DEPS_7 += $(BUILD)/inc/r.h

$(BUILD)/inc/json.h: $(DEPS_7)
	@echo '      [Copy] $(BUILD)/inc/json.h'
	mkdir -p "$(BUILD)/inc"
	cp include/json.h $(BUILD)/inc/json.h

#
#   db.h
#
DEPS_8 += include/db.h
DEPS_8 += $(BUILD)/inc/json.h

$(BUILD)/inc/db.h: $(DEPS_8)
	@echo '      [Copy] $(BUILD)/inc/db.h'
	mkdir -p "$(BUILD)/inc"
	cp include/db.h $(BUILD)/inc/db.h

#
#   mqtt.h
#
DEPS_9 += include/mqtt.h
DEPS_9 += $(BUILD)/inc/me.h
DEPS_9 += $(BUILD)/inc/r.h

$(BUILD)/inc/mqtt.h: $(DEPS_9)
	@echo '      [Copy] $(BUILD)/inc/mqtt.h'
	mkdir -p "$(BUILD)/inc"
	cp include/mqtt.h $(BUILD)/inc/mqtt.h

#
#   websock.h
#
DEPS_10 += include/websock.h
DEPS_10 += $(BUILD)/inc/me.h
DEPS_10 += $(BUILD)/inc/r.h
DEPS_10 += $(BUILD)/inc/crypt.h
DEPS_10 += $(BUILD)/inc/json.h

$(BUILD)/inc/websock.h: $(DEPS_10)
	@echo '      [Copy] $(BUILD)/inc/websock.h'
	mkdir -p "$(BUILD)/inc"
	cp include/websock.h $(BUILD)/inc/websock.h

#
#   url.h
#
DEPS_11 += include/url.h
DEPS_11 += $(BUILD)/inc/me.h
DEPS_11 += $(BUILD)/inc/r.h
DEPS_11 += $(BUILD)/inc/json.h
DEPS_11 += $(BUILD)/inc/websock.h

$(BUILD)/inc/url.h: $(DEPS_11)
	@echo '      [Copy] $(BUILD)/inc/url.h'
	mkdir -p "$(BUILD)/inc"
	cp include/url.h $(BUILD)/inc/url.h

#
#   web.h
#
DEPS_12 += include/web.h
DEPS_12 += $(BUILD)/inc/me.h
DEPS_12 += $(BUILD)/inc/r.h
DEPS_12 += $(BUILD)/inc/json.h
DEPS_12 += $(BUILD)/inc/crypt.h
DEPS_12 += $(BUILD)/inc/websock.h

$(BUILD)/inc/web.h: $(DEPS_12)
	@echo '      [Copy] $(BUILD)/inc/web.h'
	mkdir -p "$(BUILD)/inc"
	cp include/web.h $(BUILD)/inc/web.h

#
#   openai.h
#
DEPS_13 += include/openai.h
DEPS_13 += $(BUILD)/inc/me.h
DEPS_13 += $(BUILD)/inc/r.h
DEPS_13 += $(BUILD)/inc/json.h
DEPS_13 += $(BUILD)/inc/url.h

$(BUILD)/inc/openai.h: $(DEPS_13)
	@echo '      [Copy] $(BUILD)/inc/openai.h'
	mkdir -p "$(BUILD)/inc"
	cp include/openai.h $(BUILD)/inc/openai.h

#
#   ioto.h
#
DEPS_14 += include/ioto.h
DEPS_14 += $(BUILD)/inc/config.h
DEPS_14 += $(BUILD)/inc/r.h
DEPS_14 += $(BUILD)/inc/json.h
DEPS_14 += $(BUILD)/inc/crypt.h
DEPS_14 += $(BUILD)/inc/db.h
DEPS_14 += $(BUILD)/inc/mqtt.h
DEPS_14 += $(BUILD)/inc/url.h
DEPS_14 += $(BUILD)/inc/web.h
DEPS_14 += $(BUILD)/inc/websock.h
DEPS_14 += $(BUILD)/inc/openai.h

$(BUILD)/inc/ioto.h: $(DEPS_14)
	@echo '      [Copy] $(BUILD)/inc/ioto.h'
	mkdir -p "$(BUILD)/inc"
	cp include/ioto.h $(BUILD)/inc/ioto.h

#
#   uctx-defs.h
#
DEPS_15 += include/uctx-defs.h

$(BUILD)/inc/uctx-defs.h: $(DEPS_15)
	@echo '      [Copy] $(BUILD)/inc/uctx-defs.h'
	mkdir -p "$(BUILD)/inc"
	cp include/uctx-defs.h $(BUILD)/inc/uctx-defs.h

#
#   uctx-os.h
#
DEPS_16 += include/uctx-os.h

$(BUILD)/inc/uctx-os.h: $(DEPS_16)
	@echo '      [Copy] $(BUILD)/inc/uctx-os.h'
	mkdir -p "$(BUILD)/inc"
	cp include/uctx-os.h $(BUILD)/inc/uctx-os.h

#
#   uctx.h
#
DEPS_17 += include/uctx.h
DEPS_17 += $(BUILD)/inc/osdep.h
DEPS_17 += $(BUILD)/inc/uctx-os.h

$(BUILD)/inc/uctx.h: $(DEPS_17)
	@echo '      [Copy] $(BUILD)/inc/uctx.h'
	mkdir -p "$(BUILD)/inc"
	cp include/uctx.h $(BUILD)/inc/uctx.h

#
#   agent.o
#
DEPS_18 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/agent.o: \
    src/agent.c $(DEPS_18)
	@echo '   [Compile] $(BUILD)/obj/agent.o'
	$(CC) -c -o "$(BUILD)/obj/agent.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/agent.c"

#
#   ai.o
#
DEPS_19 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/ai.o: \
    src/ai.c $(DEPS_19)
	@echo '   [Compile] $(BUILD)/obj/ai.o'
	$(CC) -c -o "$(BUILD)/obj/ai.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/ai.c"

#
#   app.o
#
DEPS_20 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/app.o: \
    src/app.c $(DEPS_20)
	@echo '   [Compile] $(BUILD)/obj/app.o'
	$(CC) -c -o "$(BUILD)/obj/app.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/app.c"

#
#   cloud.o
#
DEPS_21 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/cloud.o: \
    src/cloud/cloud.c $(DEPS_21)
	@echo '   [Compile] $(BUILD)/obj/cloud.o'
	$(CC) -c -o "$(BUILD)/obj/cloud.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cloud/cloud.c"

#
#   cloudwatch.o
#
DEPS_22 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/cloudwatch.o: \
    src/cloud/cloudwatch.c $(DEPS_22)
	@echo '   [Compile] $(BUILD)/obj/cloudwatch.o'
	$(CC) -c -o "$(BUILD)/obj/cloudwatch.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cloud/cloudwatch.c"

#
#   config.o
#
DEPS_23 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/config.o: \
    src/config.c $(DEPS_23)
	@echo '   [Compile] $(BUILD)/obj/config.o'
	$(CC) -c -o "$(BUILD)/obj/config.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/config.c"

#
#   cron.o
#
DEPS_24 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/cron.o: \
    src/cron.c $(DEPS_24)
	@echo '   [Compile] $(BUILD)/obj/cron.o'
	$(CC) -c -o "$(BUILD)/obj/cron.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cron.c"

#
#   cryptLib.o
#
DEPS_25 += $(BUILD)/inc/crypt.h

$(BUILD)/obj/cryptLib.o: \
    lib/cryptLib.c $(DEPS_25)
	@echo '   [Compile] $(BUILD)/obj/cryptLib.o'
	$(CC) -c -o "$(BUILD)/obj/cryptLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/cryptLib.c"

#
#   database.o
#
DEPS_26 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/database.o: \
    src/database.c $(DEPS_26)
	@echo '   [Compile] $(BUILD)/obj/database.o'
	$(CC) -c -o "$(BUILD)/obj/database.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/database.c"

#
#   db.o
#
DEPS_27 += $(BUILD)/inc/r.h
DEPS_27 += $(BUILD)/inc/db.h

$(BUILD)/obj/db.o: \
    src/cmds/db.c $(DEPS_27)
	@echo '   [Compile] $(BUILD)/obj/db.o'
	$(CC) -c -o "$(BUILD)/obj/db.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cmds/db.c"

#
#   dbLib.o
#
DEPS_28 += $(BUILD)/inc/db.h
DEPS_28 += $(BUILD)/inc/crypt.h

$(BUILD)/obj/dbLib.o: \
    lib/dbLib.c $(DEPS_28)
	@echo '   [Compile] $(BUILD)/obj/dbLib.o'
	$(CC) -c -o "$(BUILD)/obj/dbLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/dbLib.c"

#
#   esp32.o
#
DEPS_29 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/esp32.o: \
    src/esp32.c $(DEPS_29)
	@echo '   [Compile] $(BUILD)/obj/esp32.o'
	$(CC) -c -o "$(BUILD)/obj/esp32.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/esp32.c"

#
#   helpers.o
#
DEPS_30 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/helpers.o: \
    src/cloud/helpers.c $(DEPS_30)
	@echo '   [Compile] $(BUILD)/obj/helpers.o'
	$(CC) -c -o "$(BUILD)/obj/helpers.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cloud/helpers.c"

#
#   ioto.o
#
DEPS_31 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/ioto.o: \
    src/cmds/ioto.c $(DEPS_31)
	@echo '   [Compile] $(BUILD)/obj/ioto.o'
	$(CC) -c -o "$(BUILD)/obj/ioto.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cmds/ioto.c"

#
#   json.o
#
DEPS_32 += $(BUILD)/inc/osdep.h
DEPS_32 += $(BUILD)/inc/r.h
DEPS_32 += $(BUILD)/inc/json.h

$(BUILD)/obj/json.o: \
    src/cmds/json.c $(DEPS_32)
	@echo '   [Compile] $(BUILD)/obj/json.o'
	$(CC) -c -o "$(BUILD)/obj/json.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cmds/json.c"

#
#   jsonLib.o
#
DEPS_33 += $(BUILD)/inc/json.h

$(BUILD)/obj/jsonLib.o: \
    lib/jsonLib.c $(DEPS_33)
	@echo '   [Compile] $(BUILD)/obj/jsonLib.o'
	$(CC) -c -o "$(BUILD)/obj/jsonLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/jsonLib.c"

#
#   logs.o
#
DEPS_34 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/logs.o: \
    src/cloud/logs.c $(DEPS_34)
	@echo '   [Compile] $(BUILD)/obj/logs.o'
	$(CC) -c -o "$(BUILD)/obj/logs.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cloud/logs.c"

#
#   mqtt.o
#
DEPS_35 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/mqtt.o: \
    src/mqtt.c $(DEPS_35)
	@echo '   [Compile] $(BUILD)/obj/mqtt.o'
	$(CC) -c -o "$(BUILD)/obj/mqtt.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/mqtt.c"

#
#   mqttLib.o
#
DEPS_36 += $(BUILD)/inc/mqtt.h

$(BUILD)/obj/mqttLib.o: \
    lib/mqttLib.c $(DEPS_36)
	@echo '   [Compile] $(BUILD)/obj/mqttLib.o'
	$(CC) -c -o "$(BUILD)/obj/mqttLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/mqttLib.c"

#
#   openaiLib.o
#
DEPS_37 += $(BUILD)/inc/openai.h

$(BUILD)/obj/openaiLib.o: \
    lib/openaiLib.c $(DEPS_37)
	@echo '   [Compile] $(BUILD)/obj/openaiLib.o'
	$(CC) -c -o "$(BUILD)/obj/openaiLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/openaiLib.c"

#
#   password.o
#
DEPS_38 += $(BUILD)/inc/r.h
DEPS_38 += $(BUILD)/inc/crypt.h
DEPS_38 += $(BUILD)/inc/json.h

$(BUILD)/obj/password.o: \
    src/cmds/password.c $(DEPS_38)
	@echo '   [Compile] $(BUILD)/obj/password.o'
	$(CC) -c -o "$(BUILD)/obj/password.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cmds/password.c"

#
#   provision.o
#
DEPS_39 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/provision.o: \
    src/cloud/provision.c $(DEPS_39)
	@echo '   [Compile] $(BUILD)/obj/provision.o'
	$(CC) -c -o "$(BUILD)/obj/provision.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cloud/provision.c"

#
#   rLib.o
#
DEPS_40 += $(BUILD)/inc/r.h

$(BUILD)/obj/rLib.o: \
    lib/rLib.c $(DEPS_40)
	@echo '   [Compile] $(BUILD)/obj/rLib.o'
	$(CC) -c -o "$(BUILD)/obj/rLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/rLib.c"

#
#   register.o
#
DEPS_41 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/register.o: \
    src/register.c $(DEPS_41)
	@echo '   [Compile] $(BUILD)/obj/register.o'
	$(CC) -c -o "$(BUILD)/obj/register.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/register.c"

#
#   serialize.o
#
DEPS_42 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/serialize.o: \
    src/serialize.c $(DEPS_42)
	@echo '   [Compile] $(BUILD)/obj/serialize.o'
	$(CC) -c -o "$(BUILD)/obj/serialize.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/serialize.c"

#
#   setup.o
#
DEPS_43 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/setup.o: \
    src/setup.c $(DEPS_43)
	@echo '   [Compile] $(BUILD)/obj/setup.o'
	$(CC) -c -o "$(BUILD)/obj/setup.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/setup.c"

#
#   shadow.o
#
DEPS_44 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/shadow.o: \
    src/cloud/shadow.c $(DEPS_44)
	@echo '   [Compile] $(BUILD)/obj/shadow.o'
	$(CC) -c -o "$(BUILD)/obj/shadow.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cloud/shadow.c"

#
#   sync.o
#
DEPS_45 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/sync.o: \
    src/cloud/sync.c $(DEPS_45)
	@echo '   [Compile] $(BUILD)/obj/sync.o'
	$(CC) -c -o "$(BUILD)/obj/sync.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cloud/sync.c"

#
#   uctxAssembly.o
#
DEPS_46 += $(BUILD)/inc/uctx-os.h
DEPS_46 += $(BUILD)/inc/uctx-defs.h

$(BUILD)/obj/uctxAssembly.o: \
    lib/uctxAssembly.S $(DEPS_46)
	@echo '   [Compile] $(BUILD)/obj/uctxAssembly.o'
	$(CC) -c -o "$(BUILD)/obj/uctxAssembly.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/uctxAssembly.S"

#
#   uctxLib.o
#
DEPS_47 += $(BUILD)/inc/uctx.h
DEPS_47 += $(BUILD)/inc/uctx-defs.h

$(BUILD)/obj/uctxLib.o: \
    lib/uctxLib.c $(DEPS_47)
	@echo '   [Compile] $(BUILD)/obj/uctxLib.o'
	$(CC) -c -o "$(BUILD)/obj/uctxLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/uctxLib.c"

#
#   update.o
#
DEPS_48 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/update.o: \
    src/cloud/update.c $(DEPS_48)
	@echo '   [Compile] $(BUILD)/obj/update.o'
	$(CC) -c -o "$(BUILD)/obj/update.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cloud/update.c"

#
#   url.o
#
DEPS_49 += $(BUILD)/inc/url.h
DEPS_49 += $(BUILD)/inc/json.h

$(BUILD)/obj/url.o: \
    src/cmds/url.c $(DEPS_49)
	@echo '   [Compile] $(BUILD)/obj/url.o'
	$(CC) -c -o "$(BUILD)/obj/url.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cmds/url.c"

#
#   urlLib.o
#
DEPS_50 += $(BUILD)/inc/url.h
DEPS_50 += $(BUILD)/inc/crypt.h
DEPS_50 += $(BUILD)/inc/websock.h

$(BUILD)/obj/urlLib.o: \
    lib/urlLib.c $(DEPS_50)
	@echo '   [Compile] $(BUILD)/obj/urlLib.o'
	$(CC) -c -o "$(BUILD)/obj/urlLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/urlLib.c"

#
#   web.o
#
DEPS_51 += $(BUILD)/inc/web.h

$(BUILD)/obj/web.o: \
    src/cmds/web.c $(DEPS_51)
	@echo '   [Compile] $(BUILD)/obj/web.o'
	$(CC) -c -o "$(BUILD)/obj/web.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/cmds/web.c"

#
#   webLib.o
#
DEPS_52 += $(BUILD)/inc/web.h

$(BUILD)/obj/webLib.o: \
    lib/webLib.c $(DEPS_52)
	@echo '   [Compile] $(BUILD)/obj/webLib.o'
	$(CC) -c -o "$(BUILD)/obj/webLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/webLib.c"

#
#   webserver.o
#
DEPS_53 += $(BUILD)/inc/ioto.h

$(BUILD)/obj/webserver.o: \
    src/webserver.c $(DEPS_53)
	@echo '   [Compile] $(BUILD)/obj/webserver.o'
	$(CC) -c -o "$(BUILD)/obj/webserver.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "src/webserver.c"

#
#   websockLib.o
#
DEPS_54 += $(BUILD)/inc/websock.h
DEPS_54 += $(BUILD)/inc/crypt.h

$(BUILD)/obj/websockLib.o: \
    lib/websockLib.c $(DEPS_54)
	@echo '   [Compile] $(BUILD)/obj/websockLib.o'
	$(CC) -c -o "$(BUILD)/obj/websockLib.o" $(CFLAGS) $(DFLAGS) "-DAPP=$(APP)" "-DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" $(INPUT) "lib/websockLib.c"

#
#   libioto
#
DEPS_55 += $(BUILD)/inc/config.h
DEPS_55 += $(BUILD)/inc/crypt.h
DEPS_55 += $(BUILD)/inc/db.h
DEPS_55 += $(BUILD)/inc/ioto.h
DEPS_55 += $(BUILD)/inc/json.h
DEPS_55 += $(BUILD)/inc/mqtt.h
DEPS_55 += $(BUILD)/inc/openai.h
DEPS_55 += $(BUILD)/inc/osdep.h
DEPS_55 += $(BUILD)/inc/r.h
DEPS_55 += $(BUILD)/inc/uctx-defs.h
DEPS_55 += $(BUILD)/inc/uctx-os.h
DEPS_55 += $(BUILD)/inc/uctx.h
DEPS_55 += $(BUILD)/inc/url.h
DEPS_55 += $(BUILD)/inc/web.h
DEPS_55 += $(BUILD)/inc/websock.h
DEPS_55 += $(BUILD)/obj/cryptLib.o
DEPS_55 += $(BUILD)/obj/dbLib.o
DEPS_55 += $(BUILD)/obj/jsonLib.o
DEPS_55 += $(BUILD)/obj/mqttLib.o
DEPS_55 += $(BUILD)/obj/openaiLib.o
DEPS_55 += $(BUILD)/obj/rLib.o
DEPS_55 += $(BUILD)/obj/uctxAssembly.o
DEPS_55 += $(BUILD)/obj/uctxLib.o
DEPS_55 += $(BUILD)/obj/urlLib.o
DEPS_55 += $(BUILD)/obj/webLib.o
DEPS_55 += $(BUILD)/obj/websockLib.o
DEPS_55 += $(BUILD)/obj/agent.o
DEPS_55 += $(BUILD)/obj/ai.o
DEPS_55 += $(BUILD)/obj/app.o
DEPS_55 += $(BUILD)/obj/cloud.o
DEPS_55 += $(BUILD)/obj/cloudwatch.o
DEPS_55 += $(BUILD)/obj/helpers.o
DEPS_55 += $(BUILD)/obj/logs.o
DEPS_55 += $(BUILD)/obj/provision.o
DEPS_55 += $(BUILD)/obj/shadow.o
DEPS_55 += $(BUILD)/obj/sync.o
DEPS_55 += $(BUILD)/obj/update.o
DEPS_55 += $(BUILD)/obj/config.o
DEPS_55 += $(BUILD)/obj/cron.o
DEPS_55 += $(BUILD)/obj/database.o
DEPS_55 += $(BUILD)/obj/esp32.o
DEPS_55 += $(BUILD)/obj/mqtt.o
DEPS_55 += $(BUILD)/obj/register.o
DEPS_55 += $(BUILD)/obj/serialize.o
DEPS_55 += $(BUILD)/obj/setup.o
DEPS_55 += $(BUILD)/obj/webserver.o

$(BUILD)/bin/libioto.a: $(DEPS_55)
	@echo '      [Link] $(BUILD)/bin/libioto.a'
	$(AR) -cr "$(BUILD)/bin/libioto.a" $(INPUT) "$(BUILD)/obj/cryptLib.o" "$(BUILD)/obj/dbLib.o" "$(BUILD)/obj/jsonLib.o" "$(BUILD)/obj/mqttLib.o" "$(BUILD)/obj/openaiLib.o" "$(BUILD)/obj/rLib.o" "$(BUILD)/obj/uctxAssembly.o" "$(BUILD)/obj/uctxLib.o" "$(BUILD)/obj/urlLib.o" "$(BUILD)/obj/webLib.o" "$(BUILD)/obj/websockLib.o" "$(BUILD)/obj/agent.o" "$(BUILD)/obj/ai.o" "$(BUILD)/obj/app.o" "$(BUILD)/obj/cloud.o" "$(BUILD)/obj/cloudwatch.o" "$(BUILD)/obj/helpers.o" "$(BUILD)/obj/logs.o" "$(BUILD)/obj/provision.o" "$(BUILD)/obj/shadow.o" "$(BUILD)/obj/sync.o" "$(BUILD)/obj/update.o" "$(BUILD)/obj/config.o" "$(BUILD)/obj/cron.o" "$(BUILD)/obj/database.o" "$(BUILD)/obj/esp32.o" "$(BUILD)/obj/mqtt.o" "$(BUILD)/obj/register.o" "$(BUILD)/obj/serialize.o" "$(BUILD)/obj/setup.o" "$(BUILD)/obj/webserver.o"

#
#   db
#
DEPS_56 += $(BUILD)/bin/libioto.a
DEPS_56 += $(BUILD)/obj/db.o

LIBS_56 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_56 += -lssl
    LIBPATHS_56 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_56 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_56 += -lcrypto
    LIBPATHS_56 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_56 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/db: $(DEPS_56)
	@echo '      [Link] $(BUILD)/bin/db'
	$(CC) -o "$(BUILD)/bin/db" $(LIBPATHS) $(INPUT) "$(BUILD)/obj/db.o" $(LIBPATHS_56) $(LIBS_56) $(LIBS_56) $(LIBS) $(LIBS) 

#
#   ioto
#
DEPS_57 += $(BUILD)/bin/libioto.a
DEPS_57 += $(BUILD)/obj/ioto.o

LIBS_57 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_57 += -lssl
    LIBPATHS_57 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_57 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_57 += -lcrypto
    LIBPATHS_57 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_57 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/ioto: $(DEPS_57)
	@echo '      [Link] $(BUILD)/bin/ioto'
	$(CC) -o "$(BUILD)/bin/ioto" $(LIBPATHS) $(INPUT) "$(BUILD)/obj/ioto.o" $(LIBPATHS_57) $(LIBS_57) $(LIBS_57) $(LIBS) $(LIBS) 

#
#   json
#
DEPS_58 += $(BUILD)/bin/libioto.a
DEPS_58 += $(BUILD)/obj/json.o

LIBS_58 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_58 += -lssl
    LIBPATHS_58 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_58 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_58 += -lcrypto
    LIBPATHS_58 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_58 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/json: $(DEPS_58)
	@echo '      [Link] $(BUILD)/bin/json'
	$(CC) -o "$(BUILD)/bin/json" $(LIBPATHS) $(INPUT) "$(BUILD)/obj/json.o" $(LIBPATHS_58) $(LIBS_58) $(LIBS_58) $(LIBS) $(LIBS) 

#
#   password
#
DEPS_59 += $(BUILD)/bin/libioto.a
DEPS_59 += $(BUILD)/obj/password.o

LIBS_59 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_59 += -lssl
    LIBPATHS_59 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_59 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_59 += -lcrypto
    LIBPATHS_59 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_59 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/password: $(DEPS_59)
	@echo '      [Link] $(BUILD)/bin/password'
	$(CC) -o "$(BUILD)/bin/password" $(LIBPATHS) $(INPUT) "$(BUILD)/obj/password.o" $(LIBPATHS_59) $(LIBS_59) $(LIBS_59) $(LIBS) $(LIBS) 

#
#   url
#
DEPS_60 += $(BUILD)/bin/libioto.a
DEPS_60 += $(BUILD)/obj/url.o

LIBS_60 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_60 += -lssl
    LIBPATHS_60 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_60 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_60 += -lcrypto
    LIBPATHS_60 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_60 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/url: $(DEPS_60)
	@echo '      [Link] $(BUILD)/bin/url'
	$(CC) -o "$(BUILD)/bin/url" $(LIBPATHS) $(INPUT) "$(BUILD)/obj/url.o" $(LIBPATHS_60) $(LIBS_60) $(LIBS_60) $(LIBS) $(LIBS) 

#
#   web
#
DEPS_61 += $(BUILD)/bin/libioto.a
DEPS_61 += $(BUILD)/obj/web.o

LIBS_61 += -lioto
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_61 += -lssl
    LIBPATHS_61 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_61 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_61 += -lcrypto
    LIBPATHS_61 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_61 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/web: $(DEPS_61)
	@echo '      [Link] $(BUILD)/bin/web'
	$(CC) -o "$(BUILD)/bin/web" $(LIBPATHS) $(INPUT) "$(BUILD)/obj/web.o" $(LIBPATHS_61) $(LIBS_61) $(LIBS_61) $(LIBS) $(LIBS) 

#
#   stop
#

stop: $(DEPS_62)

#
#   installBinary
#

installBinary: $(DEPS_63)
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
	mkdir -p "$(ME_ETC_PREFIX)/certs" ; \
	cp certs/roots.crt $(ME_ETC_PREFIX)/certs/roots.crt ; \
	cp certs/test.ans $(ME_ETC_PREFIX)/certs/test.ans ; \
	cp certs/test.crt $(ME_ETC_PREFIX)/certs/test.crt ; \
	cp certs/test.csr $(ME_ETC_PREFIX)/certs/test.csr ; \
	cp certs/test.key $(ME_ETC_PREFIX)/certs/test.key ; \
	cp certs/ec.ans $(ME_ETC_PREFIX)/certs/ec.ans ; \
	cp certs/ec.crt $(ME_ETC_PREFIX)/certs/ec.crt ; \
	cp certs/ec.csr $(ME_ETC_PREFIX)/certs/ec.csr ; \
	cp certs/ec.key $(ME_ETC_PREFIX)/certs/ec.key ; \
	cp certs/ca.crt $(ME_ETC_PREFIX)/certs/ca.crt ; \
	cp certs/ca.db $(ME_ETC_PREFIX)/certs/ca.db ; \
	cp certs/ca.db.attr $(ME_ETC_PREFIX)/certs/ca.db.attr ; \
	cp certs/ca.db.old $(ME_ETC_PREFIX)/certs/ca.db.old ; \
	cp certs/ca.key $(ME_ETC_PREFIX)/certs/ca.key ; \
	cp certs/ca.srl $(ME_ETC_PREFIX)/certs/ca.srl ; \
	cp certs/ca.srl.old $(ME_ETC_PREFIX)/certs/ca.srl.old ; \
	cp certs/self.ans $(ME_ETC_PREFIX)/certs/self.ans ; \
	cp certs/self.crt $(ME_ETC_PREFIX)/certs/self.crt ; \
	cp certs/self.key $(ME_ETC_PREFIX)/certs/self.key ; \
	cp certs/aws.crt $(ME_ETC_PREFIX)/certs/aws.crt ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin/scripts" ; \
	cp scripts/update $(ME_VAPP_PREFIX)/bin/scripts/update ; \
	chmod 755 "$(ME_VAPP_PREFIX)/bin/scripts/update" ; \
	mkdir -p $(ME_WEB_PREFIX) ; cp -r state/site/* $(ME_WEB_PREFIX) ; \
	mkdir -p "$(ME_ETC_PREFIX)" ; \
	cp state/config/db.json5 $(ME_ETC_PREFIX)/db.json5 ; \
	cp state/config/device.json5 $(ME_ETC_PREFIX)/device.json5 ; \
	cp state/config/schema.json5 $(ME_ETC_PREFIX)/schema.json5 ; \
	cp state/config/web.json5 $(ME_ETC_PREFIX)/web.json5 ; \
	mkdir -p "$(ME_ETC_PREFIX)" ; \
	cp state/config/ioto.json5 $(ME_ETC_PREFIX)/ioto.json5 ; \
	mkdir -p $(ME_VAPP_PREFIX)/lib/db ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/inc" ; \
	cp $(BUILD)/inc/me.h $(ME_VAPP_PREFIX)/inc/me.h

#
#   start
#

start: $(DEPS_64)

#
#   install
#
DEPS_65 += stop
DEPS_65 += installBinary
DEPS_65 += start

install: $(DEPS_65)
	echo "      [Info] Ioto installed at $(ME_VAPP_PREFIX)" ; \
	echo "      [Info] Configuration directory $(ME_ETC_PREFIX)" ; \
	echo "      [Info] Documents directory $(ME_WEB_PREFIX)" ; \
	echo "      [Info] Executables directory $(ME_VAPP_PREFIX)/bin" ; \
	echo '      [Info] Use "man ioto" for usage' ; \
	echo "      [Info] Run via 'cd $(ME_ETC_PREFIX) ; sudo ioto'" ; \
	bin/json --overwrite profile=prod $(ME_ETC_PREFIX)/ioto.json5

#
#   installPrep
#

installPrep: $(DEPS_66)
	if [ "`id -u`" != 0 ] ; \
	then echo "Must run as root. Rerun with sudo." ; \
	exit 255 ; \
	fi

#
#   uninstall
#
DEPS_67 += stop

uninstall: $(DEPS_67)
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

uninstallBinary: $(DEPS_68)


EXTRA_MAKEFILE := $(strip $(wildcard ./projects/extra.mk))
ifneq ($(EXTRA_MAKEFILE),)
include $(EXTRA_MAKEFILE)
endif
