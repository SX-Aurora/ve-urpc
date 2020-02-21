
CWD = $(shell pwd)

NCPP = /opt/nec/ve/bin/nc++
NCC = /opt/nec/ve/bin/ncc
NAR = /opt/nec/ve/bin/nar
GCC = gcc
GPP = g++
AR = ar
#DEBUG = -g
OPT = -O3
GPPFLAGS = -std=gnu++11 -fpic -pthread $(OPT) $(DEBUG) -I.. -I.
GCCFLAGS = -std=c11 -fpic -pthread  $(OPT) $(DEBUG) -I.. -I.
NCPPFLAGS = -std=gnu++11 -pthread -fpic  $(OPT) $(DEBUG) -I.. -I.
NCCFLAGS = -pthread -fpic  $(OPT) $(DEBUG) -I.. -I.

VEORUN_BIN ?= $(CWD)/veorun


VHLIB_OBJS = ProcHandle.o CallArgs.o veo_api.o veo_urpc_vh.o log.o
VELIB_OBJS = veo_urpc_ve.o
LIBS = libveoVE.so libveoVH.so libveoVE.a libveoVH.a
TESTS = test_callsync test_stackargs libvehello.so libvestackargs.so


ALL: $(LIBS) veorun $(TESTS)


libveoVE.so: $(VELIB_OBJS)
	$(NCC) -Wl,-zdefs $(NCPPFLAGS) -shared -o $@ $^ \
		-L.. -lurpcVE -lveio -ldl

libveoVH.so: $(VHLIB_OBJS)
	$(GPP) -Wl,-zdefs $(GPPFLAGS) -shared -o $@ $^ -L.. -lurpcVH -llog4c -ldl

libveoVE.a: $(VELIB_OBJS)
	$(NAR) rv $@ $^

libveoVH.a: $(VHLIB_OBJS)
	$(AR) rv $@ $^


# VH

ProcHandle.o: ProcHandle.cpp ProcHandle.hpp VEOException.hpp
	$(GPP) $(GPPFLAGS) -o $@ -c $<

CallArgs.o: CallArgs.cpp CallArgs.hpp VEOException.hpp
	$(GPP) $(GPPFLAGS) -o $@ -c $<

veo_urpc_vh.o: veo_urpc.c veo_urpc.h
	$(GPP) $(GPPFLAGS) -o $@ -c $<

veo_api.o: veo_api.cpp
	$(GPP) $(GPPFLAGS) -DVEORUN_BIN=\"$(VEORUN_BIN)\" -o $@ -c $<

log.o: log.cpp log.hpp
	$(GPP) $(GPPFLAGS) -o $@ -c $<

test_callsync: test_callsync.c
	$(GCC) $(GCCFLAGS) -o $@ $< -L. -lveoVH -L.. -lurpcVH

test_stackargs: test_stackargs.c
	$(GCC) $(GCCFLAGS) -o $@ $< -L. -lveoVH -L.. -lurpcVH

# VE

veo_urpc_ve.o: veo_urpc.c veo_urpc.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

veorun.o: veorun.c veo_urpc.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

veorun: veorun.o
	$(NCPP) $(NCPPFLAGS) -o $@ $^ -L. -lveoVE -L.. -lurpcVE -lveio -lpthread

libvehello.so: libvehello.c
	$(NCC) $(NCCFLAGS) -shared -o $@ $<

libvestackargs.so: libvestackargs.c
	$(NCC) $(NCCFLAGS) -shared -o $@ $<


clean:
	rm -f *.o *.so test_*.o $(TESTS)
