#!/bin/bash
#
# Run steps to check integrity of the solver

FUZZ_ROUNDS=${CIFUZZROUNDS:-100}
STAREXEC_FUZZ_ROUNDS=${CISTAREXECFUZZROUNDS:-50}
CLEANUP=${CICLEANUP:-1}

TESTFUZZ=${RUNFUZZ:-1}
TESTSTAREXEC=${RUNSTAREXEC:-1}
TESTOPENWBO=${RUNOPENWBO:-1}

# Check whether this script is called from the repository root
[ -x tools/ci.sh ] || exit 1

# Check whether MiniSat is built
[ ! -x build/release/bin/mergesat ] || make -j $(nproc)

TOOLSDIR=$(readlink -e tools)
CHECKERDIR=$(readlink -e tools/checker)

if [ $TESTFUZZ -eq 1 ]
then
	# Enter checker repository
	pushd tools/checker

	# Checkout and build required tools
	./prepare.sh

	# Perform a basic run, and forward its exit code
	echo "Solve and check 100 formulas with a 5s timeout"
	STATUS=0
	./fuzz-drat.sh $FUZZ_ROUNDS 5 || STATUS=$?

	popd
fi

# check starexec build
# build starexec package, also for Sparrow2MergeSAT, currently based on local code
./tools/make-starexec.sh -r satrace-2019 -s satrace-2019

# build starexec build
MERGEZIP=$(ls MergeSAT*.zip | sort -V | tail -n 1)
MERGEZIP=$(readlink -e $MERGEZIP)

if [ $TESTSTAREXEC -eq 1 ]
then
	[ $CLEANUP -eq 0 ] || trap 'rm -rf $TMPD' EXIT
	TMPD=$(mktemp -d)
	cp "$MERGEZIP" "$TMPD"
	pushd $TMPD

	mkdir -p ci_starexec_tmpdir
	STAREXEC_TMPDIR=$(readlink -e ci_starexec_tmpdir)

	unzip "$MERGEZIP"
	./starexec_build
	TARGETS=$(ls ./bin/starexec_run_*_proof)

	# fuzz each of the starexec targets
	for target in $TARGETS
	do
		FULL_TARGET=$(readlink -e $target)
		pushd $CHECKERDIR
		FUZZ_SOLVER="$FULL_TARGET" DEFAULT_MERGESAT_TMPDIR="$STAREXEC_TMPDIR" ./fuzz-drat.sh $STAREXEC_FUZZ_ROUNDS 5 || STATUS=$?
		popd
	done
	# finish starexec fuzzing
	popd
fi

# test openwbo with mergesat backend
if [ $TESTOPENWBO -eq 1 ]
then
	OPENWBO_DIR=open-wbo-$$
	git clone https://github.com/sat-group/open-wbo.git "$OPENWBO_DIR"
	pushd "$OPENWBO_DIR"
	cp "$MERGEZIP" solvers/
	cd solvers
	# unzip, and create links required for build
	unzip MergeSAT.zip
	cd mergesat
	for f in minisat/*; do echo $f; ln -s $f; done
	cd ..
	# construct required makefile
	echo "VERSION = core" > mergesat.mk
	echo "SOLVERNAME = \"mergesat\"" >> mergesat.mk
	echo "SOLVERDIR = mergesat" >> mergesat.mk
	echo "NSPACE = Minisat" >> mergesat.mk
	cd ..

	make d SOLVER=mergesat -j $(nproc) || STATUS=1
	MAXSATSTATUS=0
	./open-wbo_debug -cpu-lim=30 $TOOLSDIR/ci/unsat.cnf.gz || MAXSATSTATUS=$?
	if [ $MAXSATSTATUS -ne 30 ]
	then
		echo "c failed to solve unsat formula"
		STATUS=1
	fi
	popd
	[ $CLEANUP -eq 0 ] || rm -rf "$OPENWBO_DIR" # try to clean up
fi

# Forward exit status from fuzzing
exit $STATUS
