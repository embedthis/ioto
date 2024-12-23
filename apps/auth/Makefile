#
#	Build the Auth App
#
TOP			:= $(realpath ../..)
STATE		:= $(TOP)/state
CONFIG		:= $(STATE)/config
SITE		:= $(STATE)/site
DB			:= $(STATE)/db

export PATH

ifndef SHOW
.SILENT:
endif

.PHONY: api build config post schema 

all: config

config: api export schema 

export:
	json -q --check config/ioto.json5 || echo "Cannot parse ioto.json5"
	json -q --check config/web.json5 || echo "Cannot parse web.json5"
	json -q --check config/db.json5 || echo "Cannot parse db.json5"
	ccp config/web.json5 $(CONFIG)/web.json5
	ccp config/ioto.json5 $(CONFIG)/ioto.json5
	ccp config/db.json5 $(CONFIG)/db.json5

api:

schema:
	json --blend . config/schema.json5 >$(CONFIG)/schema.json5

build: schema
	mkdir -p $(SITE)
	cp -r site/* $(SITE)
	rm -f $(DB)/*.jnl $(DB)/*.db
	echo "  [Exported] Web site"
	echo "      [Info] Built with the Auth App"

clean:
	rm -fr $(TOP)/site
