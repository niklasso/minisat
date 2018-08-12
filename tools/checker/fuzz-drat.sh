#!/bin/bash
#
# Generate solvers and check whether the DRAT proof is valid.
# Solving and verifying is done in the script check-drat.sh

LIMIT=$1
TIMEOUT=$2

[ -n "$TIMEOUT" ] || exit 2

c=$(readlink -e cnfuzz)

I=0
f=input.cnf

FAILED=0
while true
do
	I=$((I+1))
	[ "$LIMIT" -ge "$I" ] || break
	# echo "[$SECONDS] run $I ..."
	$c > $f
	STATUS=0
	timeout "$TIMEOUT" ./check-drat.sh $f &> output.log || STATUS=$?
	# ignore time outs
	[ "$STATUS" -ne 124 ] || continue

	# make failures permanent
	if [ "$STATUS" -ne 0 ] && [ "$STATUS" -ne 10 ] && [ "$STATUS" -ne 20 ]
	then
		echo "[$SECONDS] failed for try $I with status $STATUS and header $(grep "p cnf" input.cnf) seed $(awk '/c seed/ {print $3}' input.cnf)"
		mv input.cnf input.$I.cnf
		mv output.log output.$I.log
                FAILED=$((FAILED+1))
	fi
done
echo "failed $FAILED out of $LIMIT"

[ "$FAILED" -eq 0 ] || exit 1
exit 0
