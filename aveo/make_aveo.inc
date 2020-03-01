NCPP = /opt/nec/ve/bin/nc++
NCC = /opt/nec/ve/bin/ncc
NAR = /opt/nec/ve/bin/nar
GCC = gcc
GPP = g++
AR = ar
#DEBUG = -g
OPT = -O3
INCL = -I$(DEST)/include -I. -I../src -I../../src
GPPFLAGS = -std=gnu++11 -pthread $(OPT) $(DEBUG) $(INCL)
GCCFLAGS = -std=c11 -pthread  $(OPT) $(DEBUG) $(INCL)
NCPPFLAGS = -std=gnu++11 -pthread  $(OPT) $(DEBUG) $(INCL)
NCCFLAGS = -pthread  $(OPT) $(DEBUG) $(INCL)


BVE = $(BUILD)/ve
BVE_OMP = $(BUILD)/ve_omp
BVH = $(BUILD)/vh
BB  = $(BUILD)/bin
BLIB = $(BUILD)/lib

