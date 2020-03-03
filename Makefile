CWD = $(shell pwd)
DEST ?= $(CWD)/install
BUILD ?= $(CWD)/build

ALL: urpc test

urpc:
	make -C src DEST=$(DEST) BUILD=$(BUILD)

test:
	make -C test DEST=$(DEST) BUILD=$(BUILD)

install:
	make -C src install DEST=$(DEST) BUILD=$(BUILD)
	make -C test install DEST=$(DEST) BUILD=$(BUILD)

clean:
	make -C src clean
	make -C test clean

.PHONY: urpc test install clean
