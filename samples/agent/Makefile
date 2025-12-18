#
#	Samples Makefile
#

SAMPLES		:= \
	aws-s3 \
	link-agent-main \
	own-main \
	mqtt \
	thread \
	url-fetch \
	url-get \
	web-static \
	web-dynamic \
	web-upload


ifndef SHOW
.SILENT:
endif

all: build

top:
	echo $(TOP)

build:
	@for sample in $(SAMPLES) ; do \
		echo $$sample ; \
		make -C $$sample ; \
	done

clean:
	@for sample in $(SAMPLES) ; do \
		make -C $$sample clean ; \
	done
