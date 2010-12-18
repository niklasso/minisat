## TODO ###########################################################################################
#
# - Install shared library with:
#    > strip --strip-unneeded ...
#

.PHONY:	r d p sh cr cd cp csh lr ld lp lsh config all
all:	r lr lsh

## Load Previous Configuration ####################################################################

-include config.mk

## Configurable options ###########################################################################

# C++ Compiler:
MINISAT_CXX    ?= $(CXX)

# Directory to store object files, libraries, executables, and dependencies:
BUILD_DIR      ?= build

# Include debug-symbols in release builds
MINISAT_RELSYM ?= -g

# Sets of compile flags for different build types
MINISAT_REL    ?= -O3 -D NDEBUG
MINISAT_DEB    ?= -O0 -D DEBUG 
MINISAT_PRF    ?= -O3 -D NDEBUG
MINISAT_FPIC   ?= -fpic

# Target file names
MINISAT        ?= minisat#       Name of MiniSat main executable.
MINISAT_CORE   ?= minisat_core#  Name of simplified MiniSat executable (only core solver support).
MINISAT_SLIB   ?= libminisat.a#  Name of MiniSat static library.
MINISAT_DLIB   ?= libminisat.so# Name of MiniSat shared library.

## Write Configuration  ###########################################################################

config:
	rm -rf config.mk
	echo 'MINISAT_CXX?=$(MINISAT_CXX)'       >> config.mk
	echo 'BUILD_DIR?=$(BUILD_DIR)'           >> config.mk
	echo 'MINISAT_RELSYM?=$(MINISAT_RELSYM)' >> config.mk
	echo 'MINISAT_REL?=$(MINISAT_REL)'       >> config.mk
	echo 'MINISAT_DEB?=$(MINISAT_DEB)'       >> config.mk
	echo 'MINISAT_PRF?=$(MINISAT_PRF)'       >> config.mk
	echo 'MINISAT_FPIC?=$(MINISAT_FPIC)'     >> config.mk
	echo 'MINISAT?=$(MINISAT)'               >> config.mk
	echo 'MINISAT_CORE?=$(MINISAT_CORE)'     >> config.mk
	echo 'MINISAT_SLIB?=$(MINISAT_SLIB)'     >> config.mk
	echo 'MINISAT_DLIB?=$(MINISAT_DLIB)'     >> config.mk

## Configurable options end #######################################################################

SOMAJOR=2
SOMINOR=0
SORELEASE=0

MINISAT_CXXFLAGS = -I. -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -Wall -Wno-parentheses
MINISAT_LDFLAGS  = -Wall -lz


SRCS           = $(wildcard minisat/core/*.cc) $(wildcard minisat/simp/*.cc) $(wildcard minisat/utils/*.cc)
HDRS           = $(wildcard minisat/mtl/*.h) $(wildcard minisat/core/*.h)$ $(wildcard minisat/simp/*.h) $(wildcard minisat/utils/*.h)
OBJS           = $(filter-out %Main.o, $(SRCS:.cc=.o))

r:	$(BUILD_DIR)/release/bin/$(MINISAT)
d:	$(BUILD_DIR)/debug/bin/$(MINISAT)
p:	$(BUILD_DIR)/profile/bin/$(MINISAT)
sh:	$(BUILD_DIR)/dynamic/bin/$(MINISAT)

cr:	$(BUILD_DIR)/release/bin/$(MINISAT_CORE)
cd:	$(BUILD_DIR)/debug/bin/$(MINISAT_CORE)
cp:	$(BUILD_DIR)/profile/bin/$(MINISAT_CORE)
csh:	$(BUILD_DIR)/dynamic/bin/$(MINISAT_CORE)

lr:	$(BUILD_DIR)/release/lib/$(MINISAT_SLIB)
ld:	$(BUILD_DIR)/debug/lib/$(MINISAT_SLIB)
lp:	$(BUILD_DIR)/profile/lib/$(MINISAT_SLIB)
lsh:	$(BUILD_DIR)/dynamic/lib/$(MINISAT_DLIB)

## Build-type Compile-flags:
$(BUILD_DIR)/release/%.o:			MINISAT_CXXFLAGS +=$(MINISAT_REL) $(MINISAT_RELSYM)
$(BUILD_DIR)/debug/%.o:				MINISAT_CXXFLAGS +=$(MINISAT_DEB) -g
$(BUILD_DIR)/profile/%.o:			MINISAT_CXXFLAGS +=$(MINISAT_PRF) -pg
$(BUILD_DIR)/dynamic/%.o:			MINISAT_CXXFLAGS +=$(MINISAT_REL) $(MINISAT_FPIC)

## Build-type Link-flags:
$(BUILD_DIR)/profile/bin/$(MINISAT):		MINISAT_LDFLAGS += -pg
$(BUILD_DIR)/release/bin/$(MINISAT):		MINISAT_LDFLAGS += --static $(MINISAT_RELSYM)

## Executable dependencies
$(BUILD_DIR)/release/bin/$(MINISAT):	 	$(BUILD_DIR)/release/minisat/simp/Main.o $(BUILD_DIR)/release/lib/$(MINISAT_SLIB)
$(BUILD_DIR)/debug/bin/$(MINISAT):	 	$(BUILD_DIR)/debug/minisat/simp/Main.o $(BUILD_DIR)/debug/lib/$(MINISAT_SLIB)
$(BUILD_DIR)/profile/bin/$(MINISAT):	 	$(BUILD_DIR)/profile/minisat/simp/Main.o $(BUILD_DIR)/profile/lib/$(MINISAT_SLIB)
# need the main-file be compiled with fpic?
$(BUILD_DIR)/dynamic/bin/$(MINISAT):	 	$(BUILD_DIR)/dynamic/minisat/simp/Main.o $(BUILD_DIR)/dynamic/lib/$(MINISAT_DLIB)

## Executable dependencies (core-version)
$(BUILD_DIR)/release/bin/$(MINISAT_CORE):	$(BUILD_DIR)/release/minisat/core/Main.o $(BUILD_DIR)/release/lib/$(MINISAT_SLIB)
$(BUILD_DIR)/debug/bin/$(MINISAT_CORE):	 	$(BUILD_DIR)/debug/minisat/core/Main.o $(BUILD_DIR)/debug/lib/$(MINISAT_SLIB)
$(BUILD_DIR)/profile/bin/$(MINISAT_CORE):	$(BUILD_DIR)/profile/minisat/core/Main.o $(BUILD_DIR)/profile/lib/$(MINISAT_SLIB)
# need the main-file be compiled with fpic?
$(BUILD_DIR)/dynamic/bin/$(MINISAT_CORE): 	$(BUILD_DIR)/dynamic/minisat/core/Main.o $(BUILD_DIR)/dynamic/lib/$(MINISAT_DLIB)

## Library dependencies
$(BUILD_DIR)/release/lib/$(MINISAT_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/release/$(o))
$(BUILD_DIR)/debug/lib/$(MINISAT_SLIB):		$(foreach o,$(OBJS),$(BUILD_DIR)/debug/$(o))
$(BUILD_DIR)/profile/lib/$(MINISAT_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/profile/$(o))
$(BUILD_DIR)/dynamic/lib/$(MINISAT_DLIB).$(SOMAJOR).$(SOMINOR).$(SORELEASE):	$(foreach o,$(OBJS),$(BUILD_DIR)/dynamic/$(o))

## Compile rule
$(BUILD_DIR)/release/%.o $(BUILD_DIR)/debug/%.o $(BUILD_DIR)/profile/%.o $(BUILD_DIR)/dynamic/%.o :	%.cc
	mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(MINISAT_CXX) $(CXXFLAGS) $(MINISAT_CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

## Linking rule
$(BUILD_DIR)/release/bin/$(MINISAT) $(BUILD_DIR)/debug/bin/$(MINISAT) $(BUILD_DIR)/profile/bin/$(MINISAT) $(BUILD_DIR)/dynamic/bin/$(MINISAT)\
$(BUILD_DIR)/release/bin/$(MINISAT_CORE) $(BUILD_DIR)/debug/bin/$(MINISAT_CORE) $(BUILD_DIR)/profile/bin/$(MINISAT_CORE) $(BUILD_DIR)/dynamic/bin/$(MINISAT_CORE):
	mkdir -p $(dir $@)
	$(MINISAT_CXX) $^ $(LDFLAGS) $(MINISAT_LDFLAGS) -o $@

## Static Library rule
%/lib/$(MINISAT_SLIB):
	mkdir -p $(dir $@)
	$(AR) -rcsv $@ $^

## Shared Library rule
$(BUILD_DIR)/dynamic/lib/$(MINISAT_DLIB).$(SOMAJOR).$(SOMINOR).$(SORELEASE):
	mkdir -p $(dir $@)
	$(MINISAT_CXX) $(MINISAT_LDFLAGS) -o $@ -shared -Wl,-soname,$(MINISAT_DLIB).$(SOMAJOR) $^

## Shared Library links
$(BUILD_DIR)/dynamic/lib/$(MINISAT_DLIB).$(SOMAJOR):	$(BUILD_DIR)/dynamic/lib/$(MINISAT_DLIB).$(SOMAJOR).$(SOMINOR).$(SORELEASE)
	ln -sf -T $(notdir $^) $@

$(BUILD_DIR)/dynamic/lib/$(MINISAT_DLIB):	$(BUILD_DIR)/dynamic/lib/$(MINISAT_DLIB).$(SOMAJOR)
	ln -sf -T $(notdir $^) $@

## Include generated dependencies
## NOTE: dependencies are assumed to be the same in all build modes at the moment!
-include $(foreach s, $(SRCS:.cc=.d), $(BUILD_DIR)/dep/$s)
