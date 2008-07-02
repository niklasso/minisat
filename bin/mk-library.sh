#!/bin/sh
LIBNAME=$1
LIBDIR=${MROOT}/lib
LIBSPEC=${LIBDIR}/libs.spec

TMPSPEC=/tmp/mtools-library.spec

if [ $# -ge 2 ]
then
    case $2 in
        d) LIBTARGET=libd;;
        p) LIBTARGET=libp;;
        r) LIBTARGET=libr;;
        *) echo ERROR! Unknown build mode \"$2\".
           exit 10;;
    esac
else
    LIBTARGET=libr
fi

# remove comments
sed s/#.*// $LIBSPEC >$TMPSPEC

# check for matching libraries
grep $LIBNAME $TMPSPEC >/dev/null

if [ $? -eq 0 ]
then
    cd $LIBDIR

    # build all matching libraries with the specified target modifier
    grep $LIBNAME $TMPSPEC | while read lib
    do
        LIB=$(echo $lib | sed 's+\(.*\): *\(.*\)+\1+')
        DEP=$(echo $lib | sed 's+\(.*\): *\(.*\)+\2+')
        gmake -f $MROOT/mtl/template.mk -m LIB="$LIB" -m DEPDIR="$DEP" $LIBTARGET
    done
    
else
    echo ERROR! Could not find specification for libraries matching \"$LIBNAME\".
fi

rm -rf $TMPSPEC