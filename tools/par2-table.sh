# Generate par2 table for files in 2 directories, for files that exist in both
# directories. The script targets gzipped log files. Furthermore, the script
# assumes each basename of a log (and hence formula) exists exactly once.
# Furthermore, the output formas of the competition (^s SAT|UNSAT) is considered,
# and runtime is extracted from the fifth parameter of the line "^c CPU".
#
# For formula <f>, the log with name <f>.log.gz is assumed, i.e. the suffix .log.gz
# will be dropped when reporting names.
#
# Note, this script can be used to generate data to be used for plotting XY plots
# or cactus plots.
#
# Example call, to run with timeout=1800 seconds:
# TIMEOUT=1800 ./par2-table.sh <log-dir-1> <log-dir-2>

DIR1="$1"
DIR2="$2"

# check briefly whether directories exist
[ -d "$DIR1" ] || exit 1
[ -d "$DIR2" ] || exit 1

# for now, let's use 900s as a timeout, if no other TIMEOUT=... variable is specified
declare -r TIMEOUT="${TIMEOUT:-900}"

# other metrics (# solved, par2)
declare -i SOLVED1=0
declare -i SOLVED2=0
declare -i TOTAL=0
PAR2_1=0.0
PAR2_2=0.0

# print header
D1="$(basename "$(readlink -e "$DIR1")")"
D2="$(basename "$(readlink -e "$DIR2")")"

echo "formula $D1 $D2"

MISMATCHED=""
# consider all files in first directory
for log in $(ls $DIR1/*.log.gz)
do
	name=$(basename $log)

	# skip, if file is not present in second directory
	[ -r "$DIR2/$name" ] || continue

	formula=$(basename $name .log.gz)

	TOTAL=$((TOTAL+1))

	# get first info, extract CPU time, or use twice the timeout in case there is no SAT/UNSAT
	INFO1="$(zgrep -e "^s SAT" -e "^s UNSAT" -e "^c CPU" $log)"
	TIME1=$(echo "$INFO1" | awk -v t=$TIMEOUT '/c CPU/ {if($5 < t) {print $5} else {print 2*t}}')
	echo "$INFO1" | grep -q -e "^s SAT" -e "^s UNSAT" || TIME1=$((2*TIMEOUT))
	[ -n "$TIME1" ] || TIME1=$((2*TIMEOUT))
	RC1=0
	echo "$INFO1" | grep -q -e "^s SAT" && RC1=10
	echo "$INFO1" | grep -q -e "^s UNSAT" && RC1=20

	[ -z "$(echo $TIME1 | awk -v t=$TIMEOUT '{ if ($1 <= t) print $1}')" ] || SOLVED1=$((SOLVED1 + 1))

	log2="$DIR2/$name"
	INFO2="$(zgrep -e "^s SAT" -e "^s UNSAT" -e "^c CPU" "$log2")"
	TIME2=$(echo "$INFO2" | awk -v t=$TIMEOUT '/c CPU/ {if($5 < t) {print $5} else {print 2*t}}')
	echo "$INFO2" | grep -q -e "^s SAT" -e "^s UNSAT" || TIME2=$((2*TIMEOUT))
	[ -n "$TIME2" ] || TIME2=$((2*TIMEOUT))
	[ -z "$(echo $TIME2 | awk -v t=$TIMEOUT '{ if ($1 <= t) print $1}')" ] || SOLVED2=$((SOLVED2 + 1))
	RC2=0
	echo "$INFO2" | grep -q -e "^s SAT" && RC2=10
	echo "$INFO2" | grep -q -e "^s UNSAT" && RC2=20

	# print csv line
	echo "$formula $TIME1 $TIME2"
	PAR2_1="$(echo "$PAR2_1 + $TIME1" | bc)"
	PAR2_2="$(echo "$PAR2_2 + $TIME2" | bc)"

	if [ $RC1 -eq 10 -a $RC2 -eq 20 ] || [ $RC1 -eq 20 -a $RC2 -eq 10 ]
	then
		echo "SAT/UNSAT mismatch on $formula ($RC1 vs $RC2)" 1>&2
		MISMATCHED+=" $formula"
	fi
done

# print other stats
echo -e "\ntotal files: $TOTAL"
echo -e "\nsolved $SOLVED1 $SOLVED2" 1>&2
echo -e "\npar2 $PAR2_1 $PAR2_2" 1>&2
[ -z "$MISMATCHED" ] || echo -e "\nmismatch formulas: $MISMATCHED" 1>&2
