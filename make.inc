NCC = /opt/nec/ve/bin/ncc
NAR = /opt/nec/ve/bin/nar
GCC = gcc
AR = ar
DEBUG = -g
OPT = -O0 -DSYNCDMA
GCCFLAGS = --std=c11 -pthread -D_SVID_SOURCE $(DEBUG) $(OPT)
NCCFLAGS = $(FTRACE) -pthread $(DEBUG) $(OPT) -finline -finline-functions

VEDEST ?= $(DEST)

BVE = $(BUILD)/ve
BVE_OMP = $(BUILD)/ve_omp
BVH = $(BUILD)/vh
BB  = $(BUILD)/bin
BVELIB = $(BUILD)/lib
BLIB = $(BUILD)/lib64
BINC = $(BUILD)/include
