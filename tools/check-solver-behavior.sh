#!/bin/bash
#
# This script checks whether 2 solvers behave the same on a given CNF. The
# criteria is the number of decisions and conflicts performed, as well as the
# exit code.
#

# get to repository base
INPUT="$1"
SOLVER1="$2"
SOLVER2="$3"

usage ()
{
    $0 input 'solver1 + params' 'solver2 + parames'
}


echo "Use input $INPUT"
echo "Run solver1: '$SOLVER1'"
echo "Run solver2: '$SOLVER2'"

# make sure we clean up
trap 'rm -rf $TMPD' EXIT
TMPD=$(mktemp -d)


echo "Run solver 1 ..."
$SOLVER1 "$INPUT" > "$TMPD"/out1.log 2> "$TMPD"/err1.log
STATUS1=$?

echo "Run solver 2 ..."
$SOLVER2 "$INPUT" > "$TMPD"/out2.log 2> "$TMPD"/err2.log
STATUS2=$?

echo "Evaluate ..."
CON1=$(awk '/c conflicts/ {print $4}' "$TMPD"/out1.log)
CON2=$(awk '/c conflicts/ {print $4}' "$TMPD"/out2.log)

DEC1=$(awk '/c decisions/ {print $4}' "$TMPD"/out1.log)
DEC2=$(awk '/c decisions/ {print $4}' "$TMPD"/out2.log)


STATUS=0

if [ "$CON1" != "$CON2" ]
then
    echo "Conflicts do not match, $CON1 vs $CON2"
    STATUS=1
else
    echo "Conflicts match: $CON1"
fi


if [ "$DEC1" != "$DEC2" ]
then
    echo "Decisions do not match, $DEC1 vs $DEC2"
    STATUS=1
else
    echo "Decisions match: $DEC1"
fi

if [ "$STATUS1" != "$STATUS2" ]
then
    echo "Exit codes do not match, $STATUS1 vs $STATUS2"
    STATUS=1
else
    echo "Exit code match: $STATUS1"
fi

exit $STATUS
