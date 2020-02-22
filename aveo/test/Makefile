DEST ?= ../install
BUILD ?= ../build

include ../make_aveo.inc

NCCFLAGS := $(NCCFLAGS) -I../src
GCCFLAGS := $(GCCFLAGS) -I../src
LDFLAGS = -Wl,-zdefs -Wl,-rpath,$(abspath $(DEST)/lib) -Wl,-rpath,$(abspath $(BLIB)) -L$(BLIB)

TESTS = $(addprefix $(BB)/,test_callsync test_stackargs test_nprocs libvehello.so libvestackargs.so)


ALL: $(TESTS)


# install

install: $(TESTS) | $(DEST)/tests/
	cp -p $(TESTS) $(DEST)/tests


.PRECIOUS: $(BUILD)/ $(BUILD)%/ $(DEST)/ $(DEST)%/

%/:
	mkdir -p $@

.SECONDEXPANSION:

$(BB)/%.so: %.c  | $$(@D)/
	$(NCC) $(NCCFLAGS) -shared -fpic $(LDFLAGS) -o $@ $<

$(BB)/%: %.c  | $$(@D)/
	$(GCC) $(GCCFLAGS) -fpic $(LDFLAGS) -o $@ $< -L. -lveoVH -L.. -lurpcVH


clean:
	rm -f $(TESTS)
