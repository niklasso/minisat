#!/bin/bash
#
# Compile for UWrMaxSat

set -e
set -x

SCRIPT=$(readlink -e "$0")
SCRIPTDIR=$(dirname "$SCRIPT")

# enter the ci directory for this script
cd ${SCRIPTDIR}

# get solver
[ ! -d uwrmaxsat ] && git clone https://github.com/marekpiotrow/UWrMaxSat uwrmaxsat
pushd uwrmaxsat
git fetch origin
git checkout origin/master
popd

# compile solver
[ ! -d maxpre ] && git clone https://github.com/Laakeri/maxpre
pushd maxpre
git fetch origin
git checkout origin/master
sed -i 's/-g/-D NDEBUG/' src/Makefile  
make lib -j $(nproc)
popd

# make sure we have mergesat here
ln -s ${SCRIPTDIR}/../../../.. mergesat
pushd mergesat
make r -j $(nproc)
popd

# build the solver
pushd uwrmaxsat  
cp ../config.mk .
make config  
make r -j $(nproc)

