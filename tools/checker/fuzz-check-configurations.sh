#!/bin/bash
#
# This script runs a set of configurations on a set of randomly generated CNF
# formulas.

# Formulas to test per iteration
declare -i FORMULA_PER_CONFIG=2500

# List of configurations to consider
CONFIGURATIONS=("../../build/release/bin/mergesat"
                "../../build/debug/bin/mergesat -check-sat"
                "../../build/debug/bin/mergesat -check-sat -no-pre"
                "../../build/debug/bin/mergesat -check-sat -rtype=4 -VSIDS-lim=50 -VSIDS-init-lim=50"
                "../../build/debug/bin/mergesat -check-sat -verb=2"
                "../../build/debug/bin/mergesat -check-sat -check-proof=1 -drup-file=proof.drat")

# Current directory
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

cd "$SCRIPT_DIR"

declare -i overall_status=0
for CONFIG in "${CONFIGURATIONS[@]}"
do
    echo "test configuration: $CONFIG"
    
    echo "$CONFIG \$1" > check-solver-config.sh
    chmod u+x check-solver-config.sh
    
    status=0
    ./fuzzcheck.sh ./check-solver-config.sh "$FORMULA_PER_CONFIG" || status=$?
    if [ "$status" -ne 0 ]
    then
        echo "configuration failed!"
        overall_status=$status
    fi
done

exit $overall_status
