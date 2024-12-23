#
#	aws-s3 -- Makefile for S3 sample
#

include ../defines.mk

all: compile

compile: sample

sample: sample.o

sample.o: sample.c

run:
	./sample

clean:
	rm -f sample.o sample
