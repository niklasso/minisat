#-----------------------------------------------------------------------#
#- GLOBAL DEFS ---------------------------------------------------------#
#-----------------------------------------------------------------------#

# Keep this as generic as possible.

NAME=mergesat

#-----------------------------------------------------------------------#
# Solver signatures have to be both valid file names and C symbols.
# Since Picosat uses a dash '-' for the package name, we have to
# differentiate between with (directory / package name) and without
# dash (the signature).

SIG=$(NAME)
DIR=$(NAME)
TARGET=libipasir$(SIG).a

#-----------------------------------------------------------------------#

CXX=g++
CXXFLAGS=-Wall -DNDEBUG -O3

#-----------------------------------------------------------------------#
#- REQUIRED TOP RULES --------------------------------------------------#
#-----------------------------------------------------------------------#

all: $(TARGET)

clean:
	rm -f *.o *.a
	make -C $(DIR) clean
	rm -rf $(DIR)/build

#-----------------------------------------------------------------------#
#- INVISIBLE INTERNAL SUB RULES ----------------------------------------#
#-----------------------------------------------------------------------#

libipasir$(SIG).a: .FORCE
	@#
	@# configure and build library
	@#
	make -C $(DIR) lr
	@#
	@# copy library
	@#
	cp $(DIR)/build/release/lib/lib$(NAME).a $(TARGET)

#-----------------------------------------------------------------------#

.FORCE:
.PHONY: all clean
