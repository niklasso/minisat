#!/bin/bash
# make-starexec.sh, Norbert Manthey, 2019
#
# build the starexec package from the current git branch

# make sure we notice failures early
set -e -x

# make sure we know where the code is
SOLVERDIR=$(pwd)

if [ ! -x "$SOLVERDIR"/tools/make-starexec.sh ]
then
	echo "Error: script has to be called from base directory, abort!"
	exit 1
fi

# make sure we clean up
trap 'rm -rf $TMPD' EXIT
TMPD=$(mktemp -d)

# create the project directory
pushd "$TMPD"

mkdir mergesat
cd mergesat

# copy template
cp -r $SOLVERDIR/tools/ipasir_template/* .

# copy actual source by using the git tree, only the current branch
git clone "$SOLVERDIR" --single-branch mergesat
pushd mergesat
git checkout $BRANCH
git gc
git prune
git remote remove origin || true
git remote add origin https://github.com/conp-solutions/mergesat.git
popd

# Generate a license stub
echo "Note, sub-packages might come with different licenses!" > LICENSE

cd ..

# compress
zip -r -y -9 mergesat-ipasir.zip *

# jump back and move mergesat-ipasir.zip here
popd
mv "$TMPD"/mergesat-ipasir.zip .
