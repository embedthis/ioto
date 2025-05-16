#
#	Build the Auth App
#
TOP			:= $(realpath ../..)
SITE		:= $(TOP)/state/site
DB			:= $(TOP)/state/db

ifndef SHOW
.SILENT:
endif

.PHONY: build config

all: config build

config:
	@$(TOP)/bin/export-app-config $(TOP)

build:
	mkdir -p $(SITE)
	cp -r site/* $(SITE)
	rm -f $(DB)/*.jnl $(DB)/*.db
	echo "  [Exported] Web site"
	echo "      [Info] Built with the Auth App"

clean:
	rm -fr $(TOP)/site
