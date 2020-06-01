#!/bin/bash
# make-starexec.sh, Norbert Manthey, 2019
#
# build the starexec package from the current git branch

# make sure we notice failures early
set -e -x

get_riss ()
{
	local arg="$1"
	local -r CALLDIR=$(pwd)  # we need to store the package here

	# is $arg a directory (to riss)?
	if [ -d "$arg" ]
	then
		pushd "$arg"

		# build the solver and select the latest version
		./scripts/make-starexec.sh
		RISSZIP=$(ls Riss*.zip | sort -V | tail -n 1)

		# move Riss zip file
		mkdir -p "$CALLDIR"/Riss
		mv $RISSZIP "$CALLDIR"/Riss/Riss.zip

		popd
		return 0
	fi

	# is $arg a commit ID to clone from github?
	git ls-remote https://github.com/conp-solutions/riss.git "$arg" || true
	COMMIT=$(git ls-remote https://github.com/conp-solutions/riss.git "$arg" | awk '{print $1}')
	[ -n "$COMMIT" ] || return

	mkdir -p "$CALLDIR"/Riss-clone
	pushd "$CALLDIR"/Riss-clone

	git clone https://github.com/conp-solutions/riss.git
	cd riss
	git checkout -b "mergesat-build" "$COMMIT"

	# build the solver and select the latest version
	./scripts/make-starexec.sh
	RISSZIP=$(ls Riss*.zip | sort -V | tail -n 1)

	# move Riss zip file
	mkdir -p "$CALLDIR"/Riss
	mv $RISSZIP "$CALLDIR"/Riss/Riss.zip

	popd
	# clean up cloned solver
	rm -rf "$CALLDIR"/Riss-clone
}

get_sparrow ()
{
	local arg="$1"
	local SPARROWDIR=
	local -r CALLDIR=$(pwd)  # we need to store the package here

	# is $arg a directory (to sparrow)?
	if [ -d "$arg" ]
	then
		pushd "$arg"

		# clean the solver and zip it
		make clean
		zip -r -y -9 Sparrow.zip *

		# move Sparrow zip file
		mkdir -p "$CALLDIR"/Sparrow
		mv Sparrow.zip "$CALLDIR"/Sparrow/Sparrow.zip

		popd
		return 0
	fi

	# is $arg a commit ID to clone from github?
	git ls-remote https://github.com/adrianopolus/Sparrow.git "$arg" || true
	COMMIT=$(git ls-remote https://github.com/adrianopolus/Sparrow.git "$arg" | awk '{print $1}')
	[ -n "$COMMIT" ] || return

	mkdir -p "$CALLDIR"/Sparrow-clone
	pushd "$CALLDIR"/Sparrow-clone

	git clone https://github.com/adrianopolus/Sparrow.git
	cd Sparrow
	git checkout -b "mergesat-build" "$COMMIT"

	# clean the solver and zip it
	make clean
	zip -r -y -9 Sparrow.zip *

	# move Sparrow zip file
	mkdir -p "$CALLDIR"/Sparrow
	mv Sparrow.zip "$CALLDIR"/Sparrow/Sparrow.zip

	popd
	# clean up cloned solver
	rm -rf "$CALLDIR"/Sparrow-clone
}

# make sure we know where the code is
SOLVERDIR=$(pwd)
BRANCH=$(git rev-parse --short HEAD)

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

RISSOPT=""
SPARROWOPT=""
# do we want to package Riss(for Coprocessor) or Sparrow as well?
while getopts "r:s:" OPTION; do
    case $OPTION in
    r)
        RISSOPT="$OPTARG"
        ;;
    s)
        SPARROWOPT="$OPTARG"
        ;;
    *)
        echo "Unknown options provided"
        ;;
    esac
done


# make sure we clean up
trap 'rm -rf $TMPD' EXIT
TMPD=$(mktemp -d)

# create the project directory
pushd "$TMPD"

# copy template
cp -r $SOLVERDIR/tools/starexec_template/* .

# copy actual source by using the git tree, only the current branch
git clone "$SOLVERDIR" --single-branch mergesat
pushd mergesat
git checkout $BRANCH
git gc
git prune
git remote remove origin || true
git remote add origin https://github.com/conp-solutions/mergesat.git
popd

# get the other packages?
[ -z "$RISSOPT" ] || get_riss "$RISSOPT"
[ -z "$SPARROWOPT" ] || get_sparrow "$SPARROWOPT"

# Generate a license stub
echo "Note, sub-packages might come with different licenses!" > LICENSE

# compress
zip -r -y -9 MergeSAT.zip *

# jump back and move MergeSAT.zip here
popd
mv "$TMPD"/MergeSAT.zip .
