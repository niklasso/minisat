BUILD_TYPE?=release

BUILD_DIR?=build
MAXPRE?=-D MAXPRE
BIGWEIGHTS?=#-D BIG_WEIGHTS
MINISATP_RELSYM?=
MINISATP_REL?=-std=c++11 -O3 -D NDEBUG -Wno-strict-aliasing -D MAPLE    -D MAXPRE  -D MAXPRE  $(MAXPRE) $(BIGWEIGHTS)
MINISATP_DEB?=-std=c++11 -O0 -D DEBUG  -Wno-strict-aliasing -D MAPLE    -D MAXPRE  -D MAXPRE  $(MAXPRE) $(BIGWEIGHTS)
MINISATP_PRF?=-std=c++11 -O3 -D NDEBUG -Wno-strict-aliasing -D MAPLE    -D MAXPRE  -D MAXPRE  $(MAXPRE) $(BIGWEIGHTS)
MINISATP_FPIC?=-fpic
MINISAT_INCLUDE?=-I/usr/include -I../mergesat
MINISAT_LIB?=-L/usr/lib -L../mergesat/build/$(BUILD_TYPE)/lib -lmergesat
ifneq ($(MAXPRE),)
MCL_INCLUDE?=-I../maxpre/src
MCL_LIB?=-L../maxpre/src/lib -lmaxpre
else
MCL_INCLUDE?=
MCL_LIB?=
endif
prefix?=/usr
