MTL       = ../mtl
CORE      = ../core
CHDRS     = $(wildcard *.h) $(wildcard $(MTL)/*.h)
CSRCS     = $(wildcard *.C) $(filter-out $(CORE)/Main.C, $(wildcard $(CORE)/*.C))

EXEC      = minisat
CFLAGS    = -I$(MTL) -I$(CORE) -Wall -ffloat-store
LFLAGS    = -lz

include ../mtl/template.mk
