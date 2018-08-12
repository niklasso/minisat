#!/bin/bash
#
# Run steps to check integrity of the solver

# Check whether this script is called from the repository root
[ -x tools/ci.sh ] || exit 1

# Check whether MiniSat is built
[ ! -x build/release/bin/minisat ] || make -j $(nproc)

# Enter checker repository
pushd tools/checker

# Checkout and build required tools
./prepare.sh

# Perform a basic run, and forward its exit code
echo "Solve and check 100 formulas with a 5s timeout"
STATUS=0
./fuzz-drat.sh 100 5 || STATUS=$?

# Forward exit status from fuzzing
exit $STATUS
