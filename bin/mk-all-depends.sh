#!/bin/sh

MAKE=gmake

for d in utils core simp circ tip
do
    cd $MROOT/$d 
    $MAKE clean 
    $MAKE depend.mk
done
