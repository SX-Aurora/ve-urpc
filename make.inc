NCC = /opt/nec/ve/bin/ncc
NAR = /opt/nec/ve/bin/nar
GCC = gcc
AR = ar
#DEBUG = -g
GCCFLAGS = --std=c11 -pthread -D_SVID_SOURCE -O3 $(DEBUG)
NCCFLAGS = -pthread -O3 $(DEBUG)


BVE = $(BUILD)/ve
BVE_OMP = $(BUILD)/ve_omp
BVH = $(BUILD)/vh
BB  = $(BUILD)/bin

