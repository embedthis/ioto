#
#	Build the unit test framework
#
TOP			:= $(realpath ../..)
SITE		:= $(TOP)/state/site

ifndef SHOW
.SILENT:
endif

.PHONY: build config

all: config build

config:
	@$(TOP)/bin/export-app-config $(TOP)
	@mkdir -p $(SITE)
	find $(SITE) -type f -print | xargs -r rm

build:
	echo "      [Info] Built with the Unit App"

clean:
