#!/bin/bash
#
# Run steps to check integrity of the solver

FUZZ_ROUNDS=${CIFUZZROUNDS:-1000}
STAREXEC_FUZZ_ROUNDS=${CISTAREXECFUZZROUNDS:-50}
CLEANUP=${CICLEANUP:-1}

TESTFUZZ=${RUNFUZZ:-1}
TESTSTAREXEC=${RUNSTAREXEC:-1}
TESTOPENWBO=${RUNOPENWBO:-1}
TESTPIASIR=${RUNIPASIR:-1}

IPASIR_TIMEOUT=${IPASIR_TIMEOUT:-3600}

# Check whether this script is called from the repository root
[ -x tools/ci.sh ] || exit 1

# Check whether MiniSat is built
[ -x build/release/bin/mergesat ] || make -j $(nproc)
[ -x build/debug/bin/mergesat ] || make d -j $(nproc)
[ -r build/release/lib/libmergesat.a ] || make -j $(nproc)

TOOLSDIR=$(readlink -e tools)
CHECKERDIR=$(readlink -e tools/checker)

STATUS=0

if [ $TESTFUZZ -eq 1 ]; then
    # Enter checker repository
    pushd tools/checker

    # Checkout and build required tools
    ./prepare.sh

    # Perform a basic run, and forward its exit code
    echo "Solve and check 100 formulas with a 5s timeout"
    ./fuzz-drat.sh $FUZZ_ROUNDS 5 || STATUS=$?

    echo "Fuzz some pre-defined configurations ..."
    ./fuzz-check-configurations.sh || STATUS=$?

    popd
fi

# locate release library
STATIC_LIB=$(readlink -e build/release/lib/libmergesat.a)

# check starexec build
# build starexec package, also for Sparrow2MergeSAT, currently based on local code
./tools/make-starexec.sh

# build starexec build
MERGEZIP=$(ls MergeSAT*.zip | sort -V | tail -n 1)
MERGEZIP=$(readlink -e $MERGEZIP)

if [ $TESTSTAREXEC -eq 1 ]; then
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
    for target in $TARGETS; do
        FULL_TARGET=$(readlink -e $target)
        pushd $CHECKERDIR
        FUZZ_SOLVER="$FULL_TARGET" DEFAULT_MERGESAT_TMPDIR="$STAREXEC_TMPDIR" ./fuzz-drat.sh $STAREXEC_FUZZ_ROUNDS 5 || STATUS=$?
        popd
    done
    # finish starexec fuzzing
    popd
fi

test_openwbo() {
    local OPENWBO_DIR=open-wbo-$$
    git clone https://github.com/sat-group/open-wbo.git "$OPENWBO_DIR"

    if [ ! -d "$OPENWBO_DIR" ]; then
        echo "failed to clone open-wbo"
        STATUS=1
        return
    fi

    pushd "$OPENWBO_DIR"
    cp "$MERGEZIP" solvers/
    cd solvers
    # unzip, and create links required for build
    unzip MergeSAT.zip
    cd mergesat
    for f in minisat/*; do
        echo $f
        ln -s $f
    done
    cd ..
    # construct required makefile
    echo "VERSION = core" >mergesat.mk
    echo "SOLVERNAME = \"mergesat\"" >>mergesat.mk
    echo "SOLVERDIR = mergesat" >>mergesat.mk
    echo "NSPACE = Minisat" >>mergesat.mk
    cd ..

    make d SOLVER=mergesat -j $(nproc) || STATUS=1
    local MAXSATSTATUS=0
    ./open-wbo_debug -cpu-lim=30 $TOOLSDIR/ci/unsat.cnf.gz || MAXSATSTATUS=$?
    if [ $MAXSATSTATUS -ne 30 ]; then
        echo "c failed to solve unsat formula"
        STATUS=1
    fi
    popd
    [ $CLEANUP -eq 0 ] || rm -rf "$OPENWBO_DIR" # try to clean up
}

# test openwbo with mergesat backend
if [ $TESTOPENWBO -eq 1 ]; then
    test_openwbo
fi

test_ipasir() {
    local IPASIR_DIR=ipasir-$$
    git clone https://github.com/conp-solutions/ipasir.git "$IPASIR_DIR"

    if [ ! -d "$IPASIR_DIR" ]; then
        echo "failed to clone ipasir"
        STATUS=1
        return
    fi

    ./tools/make-ipasir.sh
    if [ ! -r "mergesat-ipasir.zip" ]; then
        echo "failed to create ipasir package"
        STATUS=1
        return
    fi

    for round in RELEASE DEBUG; do

        # place mergesat in ipasir directory
        cp mergesat-ipasir.zip "$IPASIR_DIR"/sat
        pushd "$IPASIR_DIR"/sat
        rm -rf mergesat
        unzip mergesat-ipasir.zip

        # rewrite makefile of mergesat to use debug build
        if [ "$round" = "DEBUG" ]; then
            sed -i 's:make -C $(DIR) lr:make -C $(DIR) ld:g' mergesat/makefile
            sed -i 's:cp $(DIR)/build/release/lib/lib$(NAME).a $(TARGET):cp $(DIR)/build/debug/lib/lib$(NAME).a $(TARGET):g' mergesat/makefile
            sed -i 's:-Wall -DNDEBUG -O3:-Wall -DNDEBUG -O0:g' mergesat/makefile
        fi
        popd

        # build ipasir apps
        pushd "$IPASIR_DIR"
        ./scripts/mkcln.sh
        for APP in genipasat genipaessentials ipasir-check-conflict ipasir-check-iterative ipasir-check-satunsat; do
            B_STATUS=0
            ./scripts/mkone.sh "$APP" mergesat || B_STATUS=$?
            if [ $B_STATUS -ne 0 ]; then
                echo "failed building $APP failed with $B_STATUS"
                STATUS=1
            fi
        done
        popd

        # run ipasir checks
        IPASIR_CHECK_STATUS=0
        pushd "$IPASIR_DIR"
        # run check conflict examples
        bin/ipasir-check-conflict-mergesat 1 2 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-conflict-mergesat 10000 10002 || IPASIR_CHECK_STATUS=$?

        # stop here, in case we use debug mode
        if [ "$round" = "DEBUG" ]; then
            continue
        fi

        bin/ipasir-check-conflict-mergesat 1 1 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-conflict-mergesat 2 2 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-conflict-mergesat 2 1 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-conflict-mergesat 10002 10000 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-conflict-mergesat 9999 10002 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-conflict-mergesat 10002 9999 || IPASIR_CHECK_STATUS=$?

        # run check iterative examples
        bin/ipasir-check-iterative-mergesat 30 3 30000 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-iterative-mergesat 300 3 30000 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-iterative-mergesat 3000 3 3000 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-iterative-mergesat 2000 5 3000 || IPASIR_CHECK_STATUS=$?

        # run sat unsat check
        bin/ipasir-check-satunsat-mergesat 20000 2 300 || IPASIR_CHECK_STATUS=$?
        bin/ipasir-check-satunsat-mergesat 300 3 300000 || IPASIR_CHECK_STATUS=$?

        if [ $IPASIR_CHECK_STATUS -ne 0 ]; then
            echo "failed checking ipasir integratipn failed with $IPASIR_CHECK_STATUS"
            STATUS=1
        fi
        popd

        # check ipasir apps
        for APP in genipasat genipaessentials; do
            pushd "$IPASIR_DIR"/app
            cd $APP
            for I in $(ls inputs/*cnf | head -n 2); do
                I_STATUS=0
                timeout -k $((IPASIR_TIMEOUT + 2)) "$IPASIR_TIMEOUT" \
                    ./../../bin/"$APP"-mergesat $I &>"$APP"-mergesat.log || I_STATUS=$?
                if [ "$APP" == genipasat ]; then
                    [ $I_STATUS -ne 10 ] && [ $I_STATUS -ne 20 ] || I_STATUS=0
                fi

                if [ $I_STATUS -ne 0 ]; then
                    echo "failed $APP call with $I_STATUS on input $I"
                    STATUS=1
                    echo "last lines of log:"
                    tail -n 30 "$APP"-mergesat.log || true
                fi
            done

            cd ..
            popd
        done

    done

    [ $CLEANUP -eq 0 ] || rm -rf "$IPASIR_DIR" # try to clean up
}

# test ipasir with mergesat backend
if [ $TESTPIASIR -eq 1 ]; then
    test_ipasir
fi

# Forward exit status from fuzzing
exit $STATUS
