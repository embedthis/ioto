#
#	defines.mk -- Definitions to build Ioto samples with GCC make
#

PROFILE = dev
OS      = $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')

#
#	Change this to point to your Ioto install if building outside of the Ioto repository
#
TOP		= $(realpath $(shell pwd)/../..)
LIBDIR 	= $(TOP)/build/bin
INCDIR  = $(TOP)/include

#
#	The TLS stack libioto.a was built with (OpenSSL or MbedTLS), as its generated projects record it
#
TLS		= $(shell cat $(TOP)/projects/gmake2/tls-$(OS).rsp 2>/dev/null)
LIBS	= -lioto -ldl -lpthread -lm $(filter-out -D% -I%,$(TLS))
CFLAGS	= -g -I$(INCDIR) $(filter -D% -I%,$(TLS)) -DSERVICES_WEB -DSERVICES_MQTT

#
#	Libraries go in LDLIBS so make's link rule places them after the objects, which the GNU linker requires
#
LDLIBS	= $(LIBS)

ifeq ($(OS),macosx)
	LDFLAGS	:= -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -Wl,-rpath,$(LIBDIR)/ -L$(LIBDIR)
	MODFLAGS := -dynamiclib -install_name @rpath/libmod_simple.dylib -compatibility_version 1.0 -current_version 1.0
	MODEXT := .dylib
endif
ifeq ($(OS),linux)
	LDFLAGS	:= -L$(LIBDIR)
	MODEXT := .so
endif
