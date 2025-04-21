#
#	Build the unit test framework
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

.PHONY: config prep schema prep post

all: config

config: prep schema
	json -q --check config/ioto.json5 || echo "Cannot parse ioto.json5"
	json -q --check config/web.json5 || echo "Cannot parse web.json5"
	json -q --check config/test.json5 || echo "Cannot parse test.json5"
	ccp config/web.json5 $(CONFIG)/web.json5
	ccp config/ioto.json5 $(CONFIG)/ioto.json5
	ccp config/test.json5 $(CONFIG)/test.json5

build:

prep:
	mkdir -p $(SITE)
	find $(SITE) -type f -print | xargs -r rm

schema:
	json --blend . config/schema.json5 >$(CONFIG)/schema.json5

clean:

post: