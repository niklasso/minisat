#!/bin/sh

TMPFILE=/tmp/tmp.cc

git ls-files | grep "\.C" | while read f
do
    BASE=${f%.C}
    echo renaming: $f "->" $BASE.cc
    git mv $f $BASE.cc
    sed "s+\*\[\(.*\)\.C+[\1.cc+" $BASE.cc >$TMPFILE
    mv $TMPFILE $BASE.cc
    git add $BASE.cc
done

rm -f $TMPFILE
