#!/bin/bash
#
# Tuned EDA AI Configuration:


# locate the script to be able to call related scripts
SCRIPT=$(readlink -e "$0")
SCRIPTDIR=$(dirname "$SCRIPT")

# fail on error
set -e

INPUT_FILE="$1"

if [ ! -r "$INPUT_FILE" ]; then
    echo "Cannot read input file $INPUT_FILE, abort"
    exit 1
fi

exec "$SCRIPTDIR"/mergesat  \
     "$INPUT_FILE"
