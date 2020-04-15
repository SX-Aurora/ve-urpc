DEST ?= ../install
BUILD ?= ../build

include ../make.inc


VHLIB_OBJ := init_hook.o vh_shm.o vh_urpc.o urpc_common.o memory.o
VELIB_OBJ := init_hook.o ve_urpc.o urpc_common.o memory.o

LIBS := $(addprefix $(BLIB)/,liburpcVH.so liburpcVE.so liburpcVE_omp.so \
  liburpcVH.a liburpcVE.a liburpcVE_omp.a)
INCLUDES := $(addprefix $(BINC)/,urpc.h urpc_debug.h urpc_time.h)

VHLIB_OBJS := $(addprefix $(BVH)/,$(VHLIB_OBJ))
VELIB_OBJS := $(addprefix $(BVE)/,$(VELIB_OBJ))
VELIB_OBJS_OMP := $(addprefix $(BVE_OMP)/,$(VELIB_OBJ))


ALL: $(LIBS) $(INCLUDES)


.PRECIOUS: $(BUILD)/ $(BUILD)%/ $(DEST)/ $(DEST)%/

%/:
	mkdir -p $@

# VH objects below

%/vh_shm.o: vh_shm.c vh_shm.h
%/vh_urpc.o: vh_urpc.c urpc_common.h urpc.h vh_shm.h
%/urpc_common_vh.o: urpc_common.c urpc_common.h urpc.h urpc_time.h
%/init_hook_vh.o: init_hook.c urpc_common.h urpc.h


#  VE objects below

%/ve_urpc.o: ve_urpc.c urpc_common.h urpc.h urpc_time.h ve_inst.h
%/ve_urpc_omp.o: ve_urpc.c urpc_common.h urpc.h urpc_time.h ve_inst.h
%/urpc_common_ve.o: urpc_common.c urpc_common.h urpc.h urpc_time.h
%/init_hook_ve.o: init_hook.c urpc_common.h urpc.h


install: $(LIBS) $(INCLUDES) | $(PREF)$(DEST)/lib/ $(PREF)$(DEST)/include/
	/usr/bin/install -t $(PREF)$(DEST)/lib $(LIBS)
	/usr/bin/install -t $(PREF)$(DEST)/include $(INCLUDES)


.SECONDEXPANSION:

$(BINC)/%.h: %.h | $$(@D)/
	/usr/bin/install -t $(BINC) $<

$(BLIB)/liburpcVH.so: $(VHLIB_OBJS) | $$(@D)/
	$(GCC) $(GCCFLAGS) -fpic -shared -o $@ $^
#	$(GCC) $(GCCFLAGS) -fpic -Wl,--version-script=liburpc_vh.map -shared -o $@ $^

$(BLIB)/liburpcVE.so: $(VELIB_OBJS) | $$(@D)/
	$(NCC) -v -Wl,-zdefs $(NCCFLAGS) -fpic -shared -o $@ $^ -lveio -lveftrace
#	$(NCC) -v -Wl,-zdefs -Wl,--version-script=liburpc_ve.map $(NCCFLAGS) -fpic -shared -o $@ $^ -lveio

$(BLIB)/liburpcVE_omp.so: $(VELIB_OBJS_OMP) | $$(@D)/
	$(NCC) -Wl,-zdefs $(NCCFLAGS) -fpic -shared -fopenmp -o $@ $^ -lveio -lveftrace
#	$(NCC) -Wl,-zdefs -Wl,--version-script=liburpc_ve.map $(NCCFLAGS) -fpic -shared -fopenmp -o $@ $< -lveio

$(BLIB)/liburpcVH.a: $(VHLIB_OBJS) | $$(@D)/
	$(AR) rv $@ $^

$(BLIB)/liburpcVE.a: $(VELIB_OBJS) | $$(@D)/
	$(NAR) rv $@ $^

$(BLIB)/liburpcVE_omp.a: $(VELIB_OBJS_OMP) | $$(@D)/
	$(NAR) rv $@ $^

$(BVH)/%.o: %.c | $$(@D)/
	$(GCC) $(GCCFLAGS) -fpic -o $@ -c $<

$(BVE_OMP)/%.o: %.c | $$(@D)/
	$(NCC) $(NCCFLAGS) -fpic -fopenmp -o $@ -c $<

$(BVE)/%.o: %.c | $$(@D)/
	$(NCC) $(NCCFLAGS) -fpic -o $@ -c $<


clean:
	rm -f $(VHLIB_OBJS) $(VELIB_OBJS) $(VELIB_OBJS_OMP) $(LIBS)

