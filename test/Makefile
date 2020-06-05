DEST ?= ../install
BUILD ?= ../build

include ../make.inc

NCCFLAGS := $(NCCFLAGS) -I../src
GCCFLAGS := $(GCCFLAGS) -I../src
LDFLAGS = -Wl,-rpath,$(abspath $(DEST)/lib64) -Wl,-rpath,$(abspath $(BLIB)) -Wl,-rpath,$(abspath $(BVELIB)) -Wl,-rpath,.

TESTS = $(BB)/ping_vh $(BB)/pong_ve

ALL: $(TESTS)


# VH objects below

%/pingpong_vh.o: pingpong.c urpc_common.h
%/ping_vh.o: ping_vh.c pingpong.h

#  VE objects below

%/pingpong_ve.o: pingpong.c urpc_common.h
%/pong_ve.o: pong_ve.c pingpong.h

# install

install: $(TESTS) | $(PREF)$(DEST)/tests/
	cp -p $(TESTS) $(PREF)$(DEST)/tests


.PRECIOUS: $(BUILD)/ $(BUILD)%/ $(DEST)/ $(DEST)%/

%/:
	mkdir -p $@

.SECONDEXPANSION:

$(BB)/ping_vh: $(BVH)/ping_vh.o $(BVH)/pingpong.o | $$(@D)/
	$(GCC) $(GCCFLAGS) $(LDFLAGS) -o $@ $^ -L$(BLIB) -L$(BVELIB) -lurpcVH

$(BB)/pong_ve: $(BVE)/pong_ve.o $(BVE)/pingpong.o | $$(@D)/
	$(NCC) $(NCCFLAGS) $(LDFLAGS) -o $@ $^ -L$(BLIB) -L$(BVELIB) -lurpcVE -lveio -lpthread -lveftrace


$(BVH)/%.o: %.c pingpong.h | $$(@D)/
	$(GCC) $(GCCFLAGS) -o $@ -c $<

$(BVE)/%.o: %.c pingpong.h | $$(@D)/
	$(NCC) $(NCCFLAGS) -o $@ -c $<


clean:
	rm -f $(TESTS)
