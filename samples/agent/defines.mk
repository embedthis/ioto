#
#	defines.mk -- Definitions to build Ioto samples with GCC make
#

PROFILE = dev
OS      = $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')

ifeq ($(OS),macosx)
	IFLAGS	:= -I /opt/homebrew/include
	LFLAGS	:= -L /opt/homebrew/lib
endif

#
#	Change this to point to your Ioto install if building outside of the Ioto repository
#
TOP		= $(realpath $(shell pwd)/../../..)
LIBDIR 	= $(TOP)/build/bin
INCDIR  = $(TOP)/build/inc
LIBS	= $(LFLAGS) -lioto -ldl -lpthread -lm -lssl -lcrypto 
CFLAGS	= -g -I$(INCDIR) $(IFLAGS) -DSERVICES_CLOUD -DSERVICES_WEB -DSERVICES_MQTT

ifeq ($(OS),macosx)
	LDFLAGS	:= -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -Wl,-rpath,$(LIBDIR)/ -L$(LIBDIR) $(LIBS)
	MODFLAGS := -dynamiclib -install_name @rpath/libmod_simple.dylib -compatibility_version 1.0 -current_version 1.0
	MODEXT := .dylib
endif
ifeq ($(OS),linux)
	LDFLAGS	:= -L$(LIBDIR) $(LIBS)
	MODEXT := .so
endif
