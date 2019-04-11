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

popd
CHECKERDIR=$(readlink -e tools/checker)

# check starexec build
# trap 'rm -rf $TMPD' EXIT
TMPD=$(mktemp -d)

# build starexec package, also for Sparrow2MergeSAT, currently based on local code
./tools/make-starexec.sh -r ~/git/riss-public -s ~/git/Sparrow

# build starexec build
MERGEZIP=$(ls MergeSAT*.zip | sort -V | tail -n 1)
mv "$MERGEZIP" "$TMPD"
pushd $TMPD

mkdir -p ci_starexec_tmpdir
STAREXEC_TMPDIR=$(readlink -e ci_starexec_tmpdir)

unzip "$MERGEZIP"
./starexec_build
TARGETS=$(ls ./bin/starexec_run_*)

# fuzz each of the starexec targets
for target in $TARGETS
do
	FULL_TARGET=$(readlink -e $target)
	pushd $CHECKERDIR
	FUZZ_SOLVER="$FULL_TARGET" DEFAULT_MERGESAT_TMPDIR="$STAREXEC_TMPDIR" ./fuzz-drat.sh 10 5
	popd
done

# Forward exit status from fuzzing
exit $STATUS
