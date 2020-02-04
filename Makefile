NCC = /opt/nec/ve/bin/ncc
GCC = gcc
DEBUG = -g
GCCFLAGS = --std=c11 -pthread -D_SVID_SOURCE -O0 $(DEBUG)
NCCFLAGS = -pthread -O0 $(DEBUG)

ALL: ping_vh pong_ve

ping_vh: ping_vh.o pingpong_vh.o vh_urpc.o vh_shm.o urpc_common_vh.o
	$(GCC) $(GCCFLAGS) -o $@ $^

vh_shm.o: vh_shm.c vh_shm.h
	$(GCC) $(GCCFLAGS) -o $@ -c $<

vh_urpc.o: vh_urpc.c urpc_common.h vh_shm.h 
	$(GCC) $(GCCFLAGS) -o $@ -c $<

urpc_common_vh.o: urpc_common.c urpc_common.h urpc_time.h
	$(GCC) $(GCCFLAGS) -o $@ -c $<

pingpong_vh.o: pingpong.c urpc_common.h
	$(GCC) $(GCCFLAGS) -o $@ -c $<

ping_vh.o: ping_vh.c
	$(GCC) $(GCCFLAGS) -o $@ -c $<

#  VE objects below

pong_ve: pong_ve.o pingpong_ve.o ve_urpc.o urpc_common_ve.o
	$(NCC) $(NCCFLAGS) -o $@ $^ -lveio -lpthread

ve_urpc.o: ve_urpc.c urpc_common.h urpc_time.h ve_inst.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

urpc_common_ve.o: urpc_common.c urpc_common.h urpc_time.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

pingpong_ve.o: pingpong.c urpc_common.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

pong_ve.o: pong_ve.c
	$(NCC) $(NCCFLAGS) -o $@ -c $<

clean:
	rm -f *.o ping_v? test_*_urpc
