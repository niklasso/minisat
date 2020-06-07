#!/bin/bash
#
# This script sets up the checker environment (install drat-trim as well as cnfuzz)

# exits the script immediately if a command exits with a non-zero status
set -e

# directory of this script (in repo/scripts)
script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# get drat-trim
if [ -d drat-trim-src ]
then
	pushd drat-trim-src
	git fetch origin
	git checkout origin/master
	popd
else
	git clone https://github.com/marijnheule/drat-trim.git drat-trim-src
fi

# get fuzz tools
curl http://fmv.jku.at/cnfuzzdd/cnfuzzdd2013.zip > fuzz-suite.zip

# build cnfuzz
unzip -j -n fuzz-suite.zip cnfuzzdd2013/cnfuzz.c -d .
gcc -o cnfuzz cnfuzz.c -Wall -O0 -lrt

# build drat-trim
pushd drat-trim-src
make
mv drat-trim ..
popd

# build verify model
g++ verify_model.c -o verify -O3

# clean up
rm -rf drat-trim-src fuzz-suite.zip
