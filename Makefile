NCC = /opt/nec/ve/bin/ncc
NAR = /opt/nec/ve/bin/nar
GCC = gcc
AR = ar
#DEBUG = -g
GCCFLAGS = --std=c11 -fpic -pthread -D_SVID_SOURCE -O3 $(DEBUG)
NCCFLAGS = -pthread -fpic -O3 $(DEBUG)

VHLIB_OBJS = init_hook_vh.o vh_shm.o vh_urpc.o urpc_common_vh.o
VELIB_OBJS = init_hook_ve.o ve_urpc.o urpc_common_ve.o
VELIB_OBJS_OMP =  init_hook_ve.o ve_urpc_omp.o urpc_common_ve.o
LIBS = liburpcVH.so liburpcVE.so liburpcVE_omp.so
TESTS = ping_vh pong_ve


ALL: $(LIBS) $(TESTS)


liburpcVH.so: $(VHLIB_OBJS)
	$(GCC) $(GCCFLAGS) -shared -o $@ $^
#	$(GCC) $(GCCFLAGS) -Wl,--version-script=liburpc_vh.map -shared -o $@ $^

liburpcVE.so: $(VELIB_OBJS)
	$(NCC) -v -Wl,-zdefs $(NCCFLAGS) -shared -o $@ $^ -lveio
#	$(NCC) -v -Wl,-zdefs -Wl,--version-script=liburpc_ve.map $(NCCFLAGS) -shared -o $@ $^ -lveio

liburpcVE_omp.so: $(VELIB_OBJS_OMP)
	$(NCC) -Wl,-zdefs $(NCCFLAGS) -shared -fopenmp -o $@ $^ -lveio
#	$(NCC) -Wl,-zdefs -Wl,--version-script=liburpc_ve.map $(NCCFLAGS) -shared -fopenmp -o $@ $< -lveio


# VH objects below

vh_shm.o: vh_shm.c vh_shm.h
	$(GCC) $(GCCFLAGS) -o $@ -c $<

vh_urpc.o: vh_urpc.c urpc_common.h vh_shm.h 
	$(GCC) $(GCCFLAGS) -o $@ -c $<

urpc_common_vh.o: urpc_common.c urpc_common.h urpc_time.h
	$(GCC) $(GCCFLAGS) -o $@ -c $<

init_hook_vh.o: init_hook.c urpc_common.h
	$(GCC) $(GCCFLAGS) -o $@ -c $<

pingpong_vh.o: pingpong.c urpc_common.h
	$(GCC) $(GCCFLAGS) -o $@ -c $<

ping_vh.o: ping_vh.c
	$(GCC) $(GCCFLAGS) -o $@ -c $<

ping_vh: ping_vh.o pingpong_vh.o
	$(GCC) $(GCCFLAGS) -o $@ $^ -L. -lurpcVH

#  VE objects below

ve_urpc.o: ve_urpc.c urpc_common.h urpc_time.h ve_inst.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

ve_urpc_omp.o: ve_urpc.c urpc_common.h urpc_time.h ve_inst.h
	$(NCC) $(NCCFLAGS) -fopenmp -o $@ -c $<

urpc_common_ve.o: urpc_common.c urpc_common.h urpc_time.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

init_hook_ve.o: init_hook.c urpc_common.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

pingpong_ve.o: pingpong.c urpc_common.h
	$(NCC) $(NCCFLAGS) -o $@ -c $<

pong_ve.o: pong_ve.c
	$(NCC) $(NCCFLAGS) -o $@ -c $<

pong_ve: pong_ve.o pingpong_ve.o
	$(NCC) $(NCCFLAGS) -o $@ $^ -L. -lurpcVE -lveio -lpthread

clean:
	rm -f *.o *.so ping_v? $(TESTS) $(LIBS)
