#!/bin/sh

MAKE=make

if gmake -v >/dev/null
then
  echo "Using gmake..."
  MAKE=gmake
fi

for d in utils core simp circ tip
do
    cd $MROOT/$d 
    $MAKE clean 
    $MAKE depend.mk
done
