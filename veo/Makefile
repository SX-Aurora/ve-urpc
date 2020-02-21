
CWD = $(shell pwd)

NCPP = /opt/nec/ve/bin/nc++
NCC = /opt/nec/ve/bin/ncc
GCC = gcc
GPP = g++
DEBUG = -g
GPPFLAGS = -std=gnu++11 -fpic -pthread -O0 $(DEBUG) -I.. -I.
GCCFLAGS = -std=c11 -fpic --no-pie -pthread -O0 $(DEBUG) -I.. -I.
NCPPFLAGS = -std=gnu++11 -pthread -fpic -O3 $(DEBUG) -I.. -I.
NCCFLAGS = -pthread -fpic -O3 $(DEBUG) -I.. -I.


VHLIB_OBJS = ProcHandle.o CallArgs.o veo_api.o veo_urpc_vh.o log.o
VELIB_OBJS = veo_urpc_ve.o
TESTS = test_veo test_callargs test_callsync test_stackargs libvehello.so libvestackargs.so


ALL: libveoVE.so libveoVH.so test_ve $(TESTS)


libveoVE.so: $(VELIB_OBJS)
	$(NCC) -Wl,-zdefs $(NCPPFLAGS) -shared -o $@ $^ \
		-L.. -lurpc_ve -lveio -ldl

libveoVH.so: $(VHLIB_OBJS)
	$(GPP) -Wl,-zdefs $(GPPFLAGS) -shared -o $@ $^ -L.. -lurpc_vh -llog4c -ldl

# VH

ProcHandle.o: ProcHandle.cpp ProcHandle.hpp VEOException.hpp
	$(GPP) $(GPPFLAGS) -o $@ -c $<

CallArgs.o: CallArgs.cpp CallArgs.hpp VEOException.hpp
	$(GPP) $(GPPFLAGS) -o $@ -c $<

veo_urpc_vh.o: veo_urpc.cpp veo_urpc.h
	$(GPP) $(GPPFLAGS) -o $@ -c $<

veo_api.o: veo_api.cpp
	$(GPP) $(GPPFLAGS) -o $@ -c $<

log.o: log.cpp log.hpp
	$(GPP) $(GPPFLAGS) -o $@ -c $<

test_veo: test_veo.c veo_urpc.h
	$(GCC) $(GCCFLAGS) -o $@ $< -L. -lveoVH -L.. -lurpc_vh

test_callargs: test_callargs.c ve_offload.h
	$(GCC) $(GCCFLAGS) -o $@ $< -L. -lveoVH -L.. -lurpc_vh

test_callsync: test_callsync.c
	$(GCC) $(GCCFLAGS) -o $@ $< -L. -lveoVH -L.. -lurpc_vh

test_stackargs: test_stackargs.c
	$(GCC) $(GCCFLAGS) -o $@ $< -L. -lveoVH -L.. -lurpc_vh

# VE

veo_urpc_ve.o: veo_urpc.cpp veo_urpc.h
	$(NCPP) $(NCPPFLAGS) -o $@ -c $<

test_ve.o: test_ve.c veo_urpc.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

test_ve: test_ve.o
	$(NCPP) $(NCPPFLAGS) -o $@ $^ -L. -lveoVE -L.. -lurpc_ve -lveio -lpthread

libvehello.so: libvehello.c
	$(NCC) $(NCCFLAGS) -shared -o $@ $<

libvestackargs.so: libvestackargs.c
	$(NCC) $(NCCFLAGS) -shared -o $@ $<


clean:
	rm -f *.o *.so test_*.o
