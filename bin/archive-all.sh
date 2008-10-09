#!/bin/sh

DATE=`date +%y%m%d`
ROOT=$PWD
TARBALL=$ROOT/mtools-$DATE.tar
TMPTAR=/tmp/mtools-part.tar
EXCEPTIONS="stats"

rm -rf $TARBALL $TARBALL.gz

#SPECFILE=/tmp/mtools-files.txt


find . -name ".git" | grep -v $EXCEPTIONS | while read dir
do
    DIR=${dir%\.git}
    PREFIX=${DIR/./minisat-$DATE}
    echo Archiving module: $DIR

    cd $DIR

    git archive "--prefix=$PREFIX" HEAD >$TMPTAR

    if [ -f $TARBALL ]
    then
        # echo append
        tar Af $TARBALL $TMPTAR
    else
        # echo create
        mv $TMPTAR $TARBALL
    fi    
    cd $ROOT

done
gzip $TARBALL

rm -rf $TMPTAR