DEST ?= ../install
BUILD ?= ../build

include ../make.inc

NCCFLAGS := $(NCCFLAGS) -I../src
GCCFLAGS := $(GCCFLAGS) -I../src
LDFLAGS = -Wl,-rpath,$(DEST)/lib64 -L$(BLIB)
NLDFLAGS = -Wl,-rpath,$(VEDEST)/lib -L$(BVELIB)

TESTS = $(BB)/ping_vh $(BB)/pong_ve $(BB)/send_vh $(BB)/send_vh_e $(BB)/send_vh_t $(BB)/recv_ve

ALL: $(TESTS)


# VH objects below

%/pingpong_vh.o: pingpong.c urpc_common.h
%/ping_vh.o: ping_vh.c pingpong.h
%/sendrecv_vh.o: sendrecv.c urpc_common.h
%/send_vh.o: send_vh.c sendrecv.h
%/send_vh_e.o: send_vh_e.c sendrecv.h
%/send_vh_t.o: send_vh_t.c sendrecv.h

#  VE objects below

%/pingpong_ve.o: pingpong.c urpc_common.h
%/pong_ve.o: pong_ve.c pingpong.h
%/sendrecv_ve.o: sendrecv.c urpc_common.h
%/recv_ve.o: recv_ve.c sendrecv.h

# install

install: $(TESTS) | $(PREF)$(DEST)/tests/
	cp -p $(TESTS) $(PREF)$(DEST)/tests


.PRECIOUS: $(BUILD)/ $(BUILD)%/ $(DEST)/ $(DEST)%/

%/:
	mkdir -p $@

.SECONDEXPANSION:

$(BB)/ping_vh: $(BVH)/ping_vh.o $(BVH)/pingpong.o | $$(@D)/
	$(GCC) $(GCCFLAGS) $(LDFLAGS) -o $@ $^ -lurpcVH

$(BB)/pong_ve: $(BVE)/pong_ve.o $(BVE)/pingpong.o | $$(@D)/
	$(NCC) $(NCCFLAGS) $(NLDFLAGS) -o $@ $^ -lurpcVE -lveio -lpthread -lveftrace

$(BB)/send_vh: $(BVH)/send_vh.o $(BVH)/sendrecv.o | $$(@D)/
	$(GCC) $(GCCFLAGS) $(LDFLAGS) -o $@ $^ -lurpcVH

$(BB)/send_vh_e: $(BVH)/send_vh_e.o $(BVH)/sendrecv.o | $$(@D)/
	$(GCC) $(GCCFLAGS) $(LDFLAGS) -o $@ $^ -lurpcVH

$(BB)/send_vh_t: $(BVH)/send_vh_t.o $(BVH)/sendrecv.o | $$(@D)/
	$(GCC) $(GCCFLAGS) $(LDFLAGS) -o $@ $^ -lurpcVH

$(BB)/recv_ve: $(BVE)/recv_ve.o $(BVE)/sendrecv.o | $$(@D)/
	$(NCC) $(NCCFLAGS) $(NLDFLAGS) -o $@ $^ -lurpcVE -lveio -lpthread -lveftrace


$(BVH)/%.o: %.c pingpong.h | $$(@D)/
	$(GCC) $(GCCFLAGS) -o $@ -c $<

$(BVE)/%.o: %.c pingpong.h | $$(@D)/
	$(NCC) $(NCCFLAGS) -o $@ -c $<


clean:
	rm -f $(TESTS) \
		$(BVH)/ping_vh.o $(BVH)/pingpong.o \
		$(BVE)/pong_ve.o $(BVE)/pingpong.o \
		$(BVH)/send_vh.o $(BVH)/sendrecv.o \
		$(BVH)/send_vh_e.o $(BVH)/sendrecv.o \
		$(BVH)/send_vh_t.o $(BVH)/sendrecv.o \
		$(BVE)/recv_ve.o $(BVE)/sendrecv.o
