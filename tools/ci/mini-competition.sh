#!/usr/bin/env bash
#
# Run a small benchmark evaluation and report the PAR2 value for a selected
# set of benchmarks.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

declare -r TIMEOUT=600
declare -r SPACE_MB=4096

get_drattrim() {
    pushd "$SCRIPT_DIR"
    if [ ! -x drat-trim/drat-trim ]; then
        git clone https://github.com/marijnheule/drat-trim.git
        pushd drat-trim
        make
        popd
    fi
    popd
}

get_verify() {
    [ -x verify ] && return
    cp "$SCRIPT_DIR"/../fuzzer/verify_model.c .
    gcc -o verify verify_model.c
}

get_benchmark() {
    pushd "$SCRIPT_DIR"
    mkdir -p benchmarks
    cd benchmarks

    # some SAT 2020 benchmarks
    [ -r "3bitadd_32.cnf.gz.CP3-cnfmiter.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/3d38ffe08887da6cbe9b17ce50c4b34c
    [ -r "Steiner-15-7-bce.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/5a5fb82a3672ee898465aa8f1103147f
    [ -r "ps_200_317_70.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/5d65c8ec63b606c54ed75f0f3e0529e6
    [ -r "fclqcolor-18-14-11.cnf.gz.CP3-cnfmiter.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/72a1b82b2c1f17b0e5e0d4a44937d848
    [ -r "baseballcover12with24_and2positions.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/792b33e1963e294682d2188f36554008
    [ -r "w19-20.0.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/7a13ffe9546cc7dfabdedebb64700e8a
    [ -r "schur-triples-7-60.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/7a955f7b50c9e274fdd477006a02d609
    [ -r "dislog_a11_x11_n21.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/85071adde6e3118aef323066387a4aac
    [ -r "Timetable_C_437_E_62_Cl_29_S_28.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/8e905dfa09f45f7f50099f70cc38714c
    [ -r "combined-crypto1-wff-seed-110-wffvars-500-cryptocplx-31-overlap-2.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/940aeb30818a6b0656c9906a32bcd7bc
    [ -r "newpol4-6.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/b37dd84a1ca56a24876590bd60c5cf57
    [ -r "53-131220.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/b4457e1a0929d236a86db88d22e5a31c
    [ -r "sted2_0x0_n219-342.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/cdd89d1b9259dcf26d4a53ba94041e93
    [ -r "Steiner-45-16-bce.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/dac6f7f51d4aad660422a31ed0ee2456
    [ -r "ls15-normalized.cnf.gz.CP3-cnfmiter.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/e4c32dcfef4f80078111484c8eed84fd
    [ -r "Kakuro-easy-041-ext.xml.hg_4.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/e7b868b93a16feaa7c21bf075cda87f7
    [ -r "vlsat2_24450_2770239.dimacs.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/e85b6cb3e2751d5c80559433ba1adf06
    [ -r "Kittell-k7.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/172ecb98a80b859e62612ff192a53729
    [ -r "sv-comp19_prop-reachsafety.newton_2_2_true-unreach-call_true-termination.i-witness.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/27f890d3aa346aaa55a31b63ebd4952c
    [ -r "tseitingrid7x185_shuffled.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/4a3bee9d892a695c3d63191f4d1fbdff
    [ -r "beempgsol5b1.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/4dd165fbd1678001e130c89cfa2969a2
    [ -r "hid-uns-enc-6-1-0-0-0-0-14492.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/6147e666b75f603a4c4490d21ab654cd
    [ -r "simon03:sat02bis:k2fix_gr_2pinvar_w8.used-as.sat04-349.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/b400f2362d15334aa6d05ef99e315fc1
    [ -r "sqrt_ineq_3.c.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/b51583e32432c5778c7e3e996c3bfeba
    [ -r "6s153.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/b55b472a43a2daa68378ac8cbc4c2fb4
    [ -r "mrpp_8x8#16_12.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/cec5d86e0eae8234ca7c29b31c4674a6
    [ -r "contest03-SGI_30_50_30_20_3-dir.sat05-440.reshuffled-07.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/e47586f86868e83a5ebe7a3ecae204f1
    [ -r "jkkk-one-one-10-30-unsat.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/f3bda5fcae82cfa27460df877965eeca
    [ -r "prime2209-84.cnf.xz" ] || wget --content-disposition https://gbd.iti.kit.edu/file/af66c2b2ff8a9cd900d9f2f79e53f6a7

    popd
}

get_drattrim
get_verify
get_benchmark

VERIFY="$SCRIPT_DIR"/verify
DRATTRIM="$SCRIPT_DIR"/drat-trim/drat-trim
BENCHMARKDIR="$SCRIPT_DIR"/benchmarks

for tool in runlim bc awk; do
    if ! command -v "$tool" &>/dev/null; then
        echo "error: failed to find tool $tool"
    fi
done

trap '[ -r "$LOG_FILE" ] && rm -f "$LOG_FILE"' EXIT
LOG_FILE=$(mktemp)

trap '[ -r "$TMP_OUTFILE" ] && rm -f "$TMP_OUTFILE"' EXIT
TMP_OUTFILE=$(mktemp)

# initialize arrays
declare -A PAR2
declare -A MAXTIME
declare -A ERRORS
declare -A SOLVED
declare -A SAT
declare -A UNSAT
declare -i ERROR=0
for solver in "$@"; do
    PAR2["$solver"]=0
    MAXTIME["$solver"]=0
    ERRORS["$solver"]=0
    SOLVED["$solver"]=0
    SAT["$solver"]=0
    UNSAT["$solver"]=0
done

# run benchmarks
for benchmark in $(ls $BENCHMARKDIR/* | head -n 2); do
    for solver in "$@"; do
        echo "Run solver: $solver $benchmark"
        RUNLIM_STATUS=0
        runlim -o "$LOG_FILE" -k -r "$TIMEOUT" -s "$SPACE_MB" $solver "$benchmark" &>"$TMP_OUTFILE" || RUNLIM_STATUS=1

        RESULT="$(awk -F':' '/\[runlim\] result:/ {print $2}' "$LOG_FILE" | xargs)"
        RUNTIME="$(awk -F':' '/\[runlim\] real:/ {print $2}' "$LOG_FILE" | sed 's:seconds::g' | xargs)"
        STATUS="$(awk -F':' '/\[runlim\] status:/ {print $2}' "$LOG_FILE" | xargs)"

        if [ "$STATUS" != "out of time" ] && [ "$STATUS" != "out of memory" ] && [ "$STATUS" != "ok" ]; then
            echo "error: failed to run $solver $benchmark (results $RESULTS, time $RUNTIME, status $STATUS)"
            echo "== RUNLIM LOG =="
            cat "$LOG_FILE"
            echo "== SOLVER OUTPUT =="
            cat "$TMP_OUTFILE"
            ERRORS["$solver"]=$((ERRORS["$solver"] + 1))
            ERROR=$((ERROR + 1))

            # skip further evaluation
            continue
        fi

        S_LINE="$(grep "^s " "$TMP_OUTFILE")"

        # if sat, verify model
        if [ "$S_LINE" == "s SATISFIABLE" ]; then
            echo "verify sat"
        fi

        # if unsat, verify proof
        if [ "$S_LINE" == "s UNSATISFIABLE" ]; then
            echo "verify unsat"
        fi

        # calculate par2, and track stats
        if [ "$STATUS" == "out of time" ] || [ "$STATUS" == "out of memory" ]; then
            PAR2["$solver"]=$(echo "${PAR2["$solver"]} + $TIMEOUT + $TIMEOUT" | bc)
        else
            PAR2["$solver"]=$(echo "${PAR2["$solver"]} + $RUNTIME" | bc)
            SOLVED["$solver"]=$((${SOLVED["$solver"]} + 1))
            if [ "$S_LINE" == "s SATISFIABLE" ]; then
                SAT["$solver"]=$((${SAT["$solver"]} + 1))
            elif [ "$S_LINE" == "s UNSATISFIABLE" ]; then
                UNSAT["$solver"]=$((${UNSAT["$solver"]} + 1))
            fi

            if (($(echo "$RUNTIME > ${MAXTIME["$solver"]}" | bc -l))); then
                MAXTIME["$solver"]="$RUNTIME"
            fi
        fi

    done
done

echo "Summary"
for solver in "$@"; do
    echo "$solver: par2: ${PAR2["$solver"]} maxtime: ${MAXTIME["$solver"]} solved: ${SOLVED["$solver"]} (sat: ${SAT["$solver"]} unsat: ${UNSAT["$solver"]} ) errors: ${ERRORS["$solver"]}"
done

if [ "$ERROR" -ne 0 ]; then
    exit "$ERROR"
fi
exit "$ERROR"
