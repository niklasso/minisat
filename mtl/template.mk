##
##  Template makefile for Standard, Profile, Debug, Release, and Release-static versions
##
##    eg: "make rs" for a statically linked release version.
##        "make d"  for a debug version (no optimizations).
##        "make"    for the standard version (optimized, but with debug information and assertions active)

PWD        = $(shell pwd)
EXEC      ?= $(notdir $(PWD))

CSRCS      = $(wildcard $(PWD)/*.C) 
DSRCS      = $(foreach dir, $(DEPDIR), $(filter-out $(MROOT)/$(dir)/Main.C, $(wildcard $(MROOT)/$(dir)/*.C)))
CHDRS      = $(wildcard $(PWD)/*.h)
COBJS      = $(CSRCS:.C=.o) $(DSRCS:.C=.o)

PCOBJS     = $(addsuffix p,  $(COBJS))
DCOBJS     = $(addsuffix d,  $(COBJS))
RCOBJS     = $(addsuffix r,  $(COBJS))


CXX       ?= g++
CFLAGS    ?= -Wall
LFLAGS    ?= -Wall

COPTIMIZE ?= -O3

CFLAGS    += $(foreach dir, $(DEPDIR), -I$(MROOT)/$(dir))

.PHONY : s p d r rs clean 

s:	$(EXEC)
p:	$(EXEC)_profile
d:	$(EXEC)_debug
r:	$(EXEC)_release
rs:	$(EXEC)_static

## Compile options
%.o:			CFLAGS +=$(COPTIMIZE) -ggdb -D DEBUG
%.op:			CFLAGS +=$(COPTIMIZE) -pg -ggdb -D NDEBUG
%.od:			CFLAGS +=-O0 -ggdb -D DEBUG
%.or:			CFLAGS +=$(COPTIMIZE) -D NDEBUG

## Link options
$(EXEC):		LFLAGS := -ggdb $(LFLAGS)
$(EXEC)_profile:	LFLAGS := -ggdb -pg $(LFLAGS)
$(EXEC)_debug:		LFLAGS := -ggdb $(LFLAGS)
$(EXEC)_release:	LFLAGS := $(LFLAGS)
$(EXEC)_static:		LFLAGS := --static $(LFLAGS)

## Dependencies
$(EXEC):		$(COBJS)
$(EXEC)_profile:	$(PCOBJS)
$(EXEC)_debug:		$(DCOBJS)
$(EXEC)_release:	$(RCOBJS)
$(EXEC)_static:		$(RCOBJS)

## Build rule
%.o %.op %.od %.or:	%.C
	@echo Compiling: $(subst $(MROOT)/,,$@)
	@$(CXX) $(CFLAGS) -c -o $@ $<

## Linking rules (standard/profile/debug/release)
$(EXEC) $(EXEC)_profile $(EXEC)_debug $(EXEC)_release $(EXEC)_static:
	@echo Linking: "$@ ( $(foreach f,$^,$(subst $(MROOT)/,,$f)) )"
	@$(CXX) $^ $(LFLAGS) -o $@

## Clean rule
clean:
	@rm -f $(EXEC) $(EXEC)_profile $(EXEC)_debug $(EXEC)_release $(EXEC)_static \
	  $(COBJS) $(PCOBJS) $(DCOBJS) $(RCOBJS) *.core depend.mk 

## Make dependencies
depend.mk: $(CSRCS) $(CHDRS)
	@echo Making dependencies
	@$(CXX) $(foreach dir, $(DEPDIR), -I$(MROOT)/$(dir)) \
	   $(CSRCS) -MM | sed 's|\(.*\):|$(PWD)/\1 $(PWD)/\1r $(PWD)/\1d $(PWD)/\1p: \\\
|' > depend.mk
	@for dir in $(DEPDIR); do \
	      if [ -r $(MROOT)/$${dir}/depend.mk ]; then \
		  echo Depends on: $${dir}; \
		  cat $(MROOT)/$${dir}/depend.mk >> depend.mk; \
	      fi; \
	  done

-include depend.mk
