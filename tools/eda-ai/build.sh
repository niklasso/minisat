#!/bin/bash
#

SELECTED_CONFIGURATION=1 # could be 1, 2 or 3

# locate the script to be able to call related scripts
SCRIPT=$(readlink -e "$0")
SCRIPTDIR=$(dirname "$SCRIPT")

# fail on error
set -e

mkdir -p "$SCRIPTDIR"/binary
OUTPUT_DIR=$(readlink -e "$SCRIPTDIR"/binary)

rsync -avz "$SCRIPTDIR"/code/mergesat/ "$OUTPUT_DIR"/mergesat-src/

pushd "$SCRIPTDIR"/binary/mergesat-src
make r BUILD_TYPE=simp VERB= -j $(nproc)
cp build/release/bin/mergesat "$OUTPUT_DIR"/
cd "$OUTPUT_DIR"
rm -rf mergesat-src
popd

cp "$SCRIPTDIR"/code/mergesat/tools/eda-ai/run_config"$SELECTED_CONFIGURATION".sh "$OUTPUT_DIR"/

ls binary
