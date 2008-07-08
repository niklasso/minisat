#!/bin/sh
REMOTE=$1
BRANCH=$(git branch | sed -n '/\*/s/\*//p')

git remote show $REMOTE >/dev/null 2>&1 
case $? in
    0) ;;
    *) echo "ERROR! Unknown remote <$REMOTE>";
       exit 10;;
esac

git fetch $REMOTE
git checkout $BRANCH
git reset --hard
