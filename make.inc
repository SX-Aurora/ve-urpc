ifeq ($(VE_ARCH),ve3)
	OPT_ARCH ?= -march=ve3
	OPT_ABI ?= -mabi=2
else
#	OPT_ARCH ?= -march=ve1
#	OPT_ABI ?= -mabi=1
endif

VEBIN ?= /opt/nec/ve/bin
NCC ?= $(VEBIN)/ncc
NAR = $(VEBIN)/nar

GCC = gcc
AR = ar
DEBUG = -g
OPT = -O3 -DSYNCDMA
GCCFLAGS = --std=c11 -pthread -D_SVID_SOURCE $(DEBUG) $(OPT)
NCCFLAGS = $(FTRACE) -pthread $(DEBUG) $(OPT) -finline -finline-functions \
	$(OPT_ARCH) $(OPT_ABI)

VEDEST ?= $(DEST)

BVE = $(BUILD)/ve
BVE_OMP = $(BUILD)/ve_omp
BVH = $(BUILD)/vh
BB  = $(BUILD)/bin
BVELIB = $(BUILD)/lib
BLIB = $(BUILD)/lib64
BINC = $(BUILD)/include
