#!/bin/bash
#
# This script runs a set of configurations on a set of randomly generated CNF
# formulas.

# Formulas to test per iteration
declare -i FORMULA_PER_CONFIG_RELEASE=1500

# Current directory
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

cd "$SCRIPT_DIR"

declare -i overall_status=0
for RANK in {0..32}; do
    echo ""
    echo "test rank: $RANK"
    echo "../../build/release/bin/mergesat -diversify-rank=$RANK \$1" >check-solver-config.sh
    chmod u+x check-solver-config.sh

    status=0

    # select number of formulas to test based on configuration
    FORMULA_PER_CONFIG="$FORMULA_PER_CONFIG_RELEASE"

    ./fuzzcheck.sh ./check-solver-config.sh "$FORMULA_PER_CONFIG" || status=$?
    if [ "$status" -ne 0 ]; then
        echo "configuration failed!"
        overall_status=$status
    fi
done

exit $overall_status
