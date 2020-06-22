CWD = $(shell pwd)
DEST ?= $(CWD)/install
BUILD ?= $(CWD)/build

ALL: urpc test

all-ve: urpc-ve

all-vh: urpc-vh

urpc:
	make -C src DEST=$(DEST) BUILD=$(BUILD)

urpc-ve:
	make -C src all-ve DEST=$(DEST) BUILD=$(BUILD)

urpc-vh:
	make -C src all-vh DEST=$(DEST) BUILD=$(BUILD)

test:
	make -C test DEST=$(DEST) BUILD=$(BUILD)

install:
	make -C src install DEST=$(DEST) BUILD=$(BUILD) PREF=$(PREF)
	make -C test install DEST=$(DEST) BUILD=$(BUILD) PREF=$(PREF)

install-ve:
	make -C src install-ve DEST=$(DEST) BUILD=$(BUILD) PREF=$(PREF)

install-vh:
	make -C src install-vh DEST=$(DEST) BUILD=$(BUILD) PREF=$(PREF)

clean:
	make -C src clean
	make -C test clean

.PHONY: urpc test install clean
