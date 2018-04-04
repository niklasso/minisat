#!/bin/bash
# make-starexec.sh, Norbert Manthey, 2018
#
# build the starexec package from the current git branch

# make sure we notice failures early
set -e -x

# make sure we know where the code is
SOLVERDIR=$(pwd)
BRANCH=$(git rev-parse --abbrev-ref HEAD)

if [ ! -x "$SOLVERDIR"/tools/make-starexec.sh ]
then
	echo "Error: script has to be called from base directory, abort!"
	exit 1
fi

# check for being on a branch
if [ -z "$BRANCH" ]
then
	echo "Error: failed to extract a git branch, abort!"
	exit 1
fi

# make sure we clean up
trap 'rm -rf $TMPD' EXIT
TMPD=$(mktemp -d)

# create the project directory
pushd "$TMPD"

# copy template
cp -r $SOLVERDIR/tools/starexec_template/* .

# copy actual source by using the git tree, only the current branch
git clone "$SOLVERDIR" --branch "$BRANCH" --single-branch minisat
pushd minisat
git checkout $BRANCH
git gc
git prune
popd

# compress
zip -r -9 Minisat.zip *

# jump back and move Minisat.zip here
popd
mv "$TMPD"/Minisat.zip .
