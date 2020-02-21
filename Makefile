CWD = $(shell pwd)
DEST ?= $(CWD)/install

ALL: urpc test

urpc:
	make -C src

test:
	make -C test

install:
	make -C src install DEST=$(DEST)
	make -C test install DEST=$(DEST)

clean:
	make -C src clean
	make -C test clean

.PHONY: urpc test install clean
