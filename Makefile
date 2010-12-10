## Configurable options ###########################################################################

# Directory to store object files, libraries, executables, and dependencies:
BUILD_DIR    ?= build

# Include debug-symbols in release builds
MINISAT_RELSYMBOLS ?= -g

# Sets of compile flags for different build types
MINISAT_REL  ?= -O3 -D NDEBUG $(MINISAT_RELSYMBOLS) 
MINISAT_DEB  ?= -O0 -D DEBUG 
MINISAT_PRF  ?= -O3 -D NDEBUG

# Target file names
MINISAT      ?= minisat        # Name of MiniSat main executable.
MINISAT_CORE ?= minisat_core   # Name of simplified MiniSat executable (only core solver support).
MINISAT_SLIB ?= libminisat.a   # Name of MiniSat library.

## Configurable options end #######################################################################

MINISAT_CXXFLAGS = -I. -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS -Wall -Wno-parentheses
MINISAT_LDFLAGS  = -Wall $(MINISAT_RELSYMBOLS) -lz

CXX           ?= g++

SRCS           = $(wildcard minisat/core/*.cc) $(wildcard minisat/simp/*.cc) $(wildcard minisat/utils/*.cc)
HDRS           = $(wildcard minisat/mtl/*.h) $(wildcard minisat/core/*.h)$ $(wildcard minisat/simp/*.h) $(wildcard minisat/utils/*.h)
OBJS           = $(filter-out %Main.o, $(SRCS:.cc=.o))

.PHONY:	r d p cr cd cp lr ld lp

r:	$(BUILD_DIR)/release/bin/$(MINISAT)
d:	$(BUILD_DIR)/debug/bin/$(MINISAT)
p:	$(BUILD_DIR)/profile/bin/$(MINISAT)

cr:	$(BUILD_DIR)/release/bin/$(MINISAT_CORE)
cd:	$(BUILD_DIR)/debug/bin/$(MINISAT_CORE)
cp:	$(BUILD_DIR)/profile/bin/$(MINISAT_CORE)

lr:	$(BUILD_DIR)/release/lib/$(MINISAT_SLIB)
ld:	$(BUILD_DIR)/debug/lib/$(MINISAT_SLIB)
lp:	$(BUILD_DIR)/profile/lib/$(MINISAT_SLIB)

## Build-type Compile-flags:
$(BUILD_DIR)/release/%.o:			MINISAT_CXXFLAGS +=$(MINISAT_REL)
$(BUILD_DIR)/debug/%.o:				MINISAT_CXXFLAGS +=$(MINISAT_DEB) -g
$(BUILD_DIR)/profile/%.o:			MINISAT_CXXFLAGS +=$(MINISAT_PRF) -pg

## Build-type Link-flags:
$(BUILD_DIR)/profile/bin/$(MINISAT):		MINISAT_LDFLAGS += -pg
$(BUILD_DIR)/release/bin/$(MINISAT):		MINISAT_LDFLAGS += --static

## Executable dependencies
$(BUILD_DIR)/release/bin/$(MINISAT):	 	$(BUILD_DIR)/release/minisat/simp/Main.o $(BUILD_DIR)/release/lib/$(MINISAT_SLIB)
$(BUILD_DIR)/debug/bin/$(MINISAT):	 	$(BUILD_DIR)/debug/minisat/simp/Main.o $(BUILD_DIR)/debug/lib/$(MINISAT_SLIB)
$(BUILD_DIR)/profile/bin/$(MINISAT):	 	$(BUILD_DIR)/profile/minisat/simp/Main.o $(BUILD_DIR)/profile/lib/$(MINISAT_SLIB)

## Executable dependencies (core-version)
$(BUILD_DIR)/release/bin/$(MINISAT_CORE):	$(BUILD_DIR)/release/minisat/core/Main.o $(BUILD_DIR)/release/lib/$(MINISAT_SLIB)
$(BUILD_DIR)/debug/bin/$(MINISAT_CORE):	 	$(BUILD_DIR)/debug/minisat/core/Main.o $(BUILD_DIR)/debug/lib/$(MINISAT_SLIB)
$(BUILD_DIR)/profile/bin/$(MINISAT_CORE):	$(BUILD_DIR)/profile/minisat/core/Main.o $(BUILD_DIR)/profile/lib/$(MINISAT_SLIB)

## Library dependencies
$(BUILD_DIR)/release/lib/$(MINISAT_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/release/$(o))
$(BUILD_DIR)/debug/lib/$(MINISAT_SLIB):		$(foreach o,$(OBJS),$(BUILD_DIR)/debug/$(o))
$(BUILD_DIR)/profile/lib/$(MINISAT_SLIB):	$(foreach o,$(OBJS),$(BUILD_DIR)/profile/$(o))

## Compile rule
$(BUILD_DIR)/release/%.o $(BUILD_DIR)/debug/%.o $(BUILD_DIR)/profile/%.o:	%.cc
	mkdir -p $(dir $@) $(dir $(BUILD_DIR)/dep/$*.d)
	$(CXX) $(CXXFLAGS) $(MINISAT_CXXFLAGS) -c -o $@ $< -MMD -MF $(BUILD_DIR)/dep/$*.d

## Linking rule (release/debug)
$(BUILD_DIR)/release/bin/$(MINISAT) $(BUILD_DIR)/debug/bin/$(MINISAT) $(BUILD_DIR)/profile/bin/$(MINISAT)\
$(BUILD_DIR)/release/bin/$(MINISAT_CORE) $(BUILD_DIR)/debug/bin/$(MINISAT_CORE) $(BUILD_DIR)/profile/bin/$(MINISAT_CORE):
	mkdir -p $(dir $@)
	$(CXX) $^ $(LDFLAGS) $(MINISAT_LDFLAGS) -o $@

## Library rule (release/debug)
%$(MINISAT_SLIB):
	mkdir -p $(dir $@)
	$(AR) -rcsv $@ $^

## Include generated dependencies
## NOTE: dependencies are assumed to be the same in all build modes at the moment!
-include $(foreach s, $(SRCS:.cc=.d), $(BUILD_DIR)/dep/$s)
