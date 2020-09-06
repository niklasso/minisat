#!/usr/bin/env bash
#
# Copyright Norbert Manthey, 2020
#
# Note: this script should only be run in a docker container!
# You might make your system unusable, as your glibc is replaced
#
# Build glibc, add THP patches on top. Finally, install glibc

# in case something goes wrong, notify!
set -ex

INSTALL=false
if [ "$1" == "install" ]
then
	INSTALL=true
fi

build_glibc()
{
	# build mergesat
	pushd glibc
	mkdir -p build
	cd build
	../configure --host=x86_64-linux-gnu --prefix=/usr/lib/x86_64-linux-gnu --libdir=/usr/lib/x86_64-linux-gnu --enable-add-ons=libidn,"" --without-selinux --enable-stackguard-randomization --enable-obsolete-rpc --with-pkgversion="Ubuntu GLIBC 2.27-0ubuntu11-thp" --enable-kernel=2.6.32 --enable-systemtap --enable-multi-arch
#	../configure --host=x86_64-linux-gnu --prefix=/usr/lib/x86_64-linux-gnu --enable-add-ons=libidn,"" --without-selinux --enable-stackguard-randomization --enable-obsolete-rpc --with-pkgversion="Ubuntu GLIBC 2.27-0ubuntu11-thp" --enable-kernel=2.6.32 --enable-systemtap --enable-multi-arch
	make -j $(nproc)
	popd
}

install_glibc()
{
	# install glibc
	pushd glibc
	mkdir -p build
	cd build
	make install
	popd
}

get_glibc_227()
{
	[ -d glibc ] || git clone git://sourceware.org/git/glibc.git glibc
	pushd glibc
	git checkout glibc-2.27
	popd
}

patch_glibc_227_thp()
{
	[ -d thp ] || git clone https://github.com/conp-solutions/thp.git thp
	
	pushd glibc
	# apply patches in order
	for p in $(ls ../thp/ubuntu18.04-thp-2.27/*.patch | sort -V)
	do
		echo $p
		git apply "$p"
	done
	popd
}

get_glibc_227
patch_glibc_227_thp

build_glibc

if [ "$INSTALL" = "true" ]
then
    install_glibc
else
    echo "Will not install glibc"
fi
