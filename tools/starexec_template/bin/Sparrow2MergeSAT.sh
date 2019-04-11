#!/bin/bash
# SparrowToMergeSAT, Norbert Manthey, Adrian Balint 2019
#
# solve CNF formula $1 by simplifying first with coprocessor,
#                                            then run Sparrow
#                      and in case the formula was not solved then run MergeSAT on $1
#
# USAGE: ./SparrowToMergeSAT.sh <input.cnf> <seed> <tmpDir> [DRAT]
#

SOLVERDIR="$(dirname "${BASH_SOURCE[0]}" )"

#
# usage
#
if [ "x$1" = "x" -o "x$2" = "x"  -o "x$3" = "x" ]; then
  echo "USAGE: ./SparrowToMergeSAT.sh <input.cnf> <seed> <tmpDir> [DRAT]"
  exit 1
fi

echo "c"
echo "c SparrowToMergeSAT 2019"
echo "c Adrian Balint, Norbert Manthey"
echo "c"

#
# check if the file in the first parameter exists
#
if [ ! -f $1 ]
then
  # if the input file does not exists, then abort nicely with a message
  echo "c the file does not exist: $1"
  echo "s UNKNOWN"
  exit 0
fi

#
# variables for the script
#

file=$(readlink -e $1)            # first argument is CNF instance to be solved
shift                             # reduce the parameters, removed the very first one. remaining $@ parameters are arguments
seed=$1 #seed
shift
tmpDir=$(readlink -e $1)          # directory for temporary files
shift
doDRAT=$1                         # produce a DRAT proof for UNSAT instances
shift

# binary of the used SAT solver
SLSsolver=sparrow                 # name of the binary (if not in this directory, give relative path as well)

# parameters for coprocessor
slsCP3params="-enabled_cp3 -cp3_stats -up -subsimp -bve -no-bve_gates -no-bve_strength -bve_red_lits=1 -cp3_bve_heap=1 -bve_heap_updates=1 -bve_totalG -bve_cgrow_t=1000 -bve_cgrow=10 -ee -cp3_ee_it -unhide -cp3_uhdIters=5 -cp3_uhdEE -cp3_uhdTrans -cp3_uhdProbe=4 -cp3_uhdPrSize=3 -cp3_uhdUHLE=0"

# check whether all tools we need are available
for tool in coprocessor $SLSsolver mergesat
do
	if [ ! -x "$SOLVERDIR"/"$tool" ]
	then
		echo "c Cannot execute tool $tool via $SOLVERDIR/$tool, abort"
		exit 127
    fi
done

# some temporary files
undo=$tmpDir/cp_undo_$$            # path to temporary file that stores cp3 undo information
touch $undo
undo=$(readlink -e $undo)
tmpCNF=$tmpDir/cp_tmpCNF_$$        # path to temporary file that stores cp3 simplified formula
touch $tmpCNF
tmpCNF=$(readlink -e $tmpCNF)
model=$tmpDir/cp_model_$$          # path to temporary file that model of the preprocessor (stdout)
touch $model
model=$(readlink -e $model)
realModel=$tmpDir/model_$$         # path to temporary file that model of the SAT solver (stdout)
touch $realModel
realModel=$(readlink -e $realModel)
echo "c undo: $undo tmpCNF: $tmpCNF model: $model realModel: $realModel"

ppStart=0
ppEnd=0
solveStart=0
solveEnd=0
MergeSATStart=0
MergeSATEnd=0

# make sure we operate in the directory where this script resides
SOLVERDIR=$(dirname "${BASH_SOURCE[0]}")
FILE=$(readlink -e $file)
cd "$SOLVERDIR"

#
# If DRAT is not used, start the usual solver
# Otherwise, skip Coprocessor and Sparrow, and start with MergeSAT directly
#

#
# handle DRAT case
#
drup=""
if [ "x$doDRAT" != "x" ]
then
    # disable fm and laHack for level 1, because they do not support DRAT proofs
    drup=" -drup-file=$tmpDir/proof.out"
fi

#
# run coprocessor with parameters added to this script
# and output to stdout of the preprocessor is redirected to stderr
#

exitCode=0

# try Coprocessor+SLS first, jump to MergeSAT in case of UNSAT+DRAT, or hitting resource limits
ppStart=`date +%s`
"$SOLVERDIR"/coprocessor $file $realModel -enabled_cp3 -undo=$undo -dimacs=$tmpCNF $slsCP3params 1>&2
exitCode=$?
ppEnd=`date +%s`
echo "c preprocessed $(( $ppEnd - $ppStart)) seconds" 1>&2
echo "c preprocessed $(( $ppEnd - $ppStart)) seconds with exit code $exitCode"

# solved by preprocessing
if [ "$exitCode" -eq "10" -o "$exitCode" -eq "20" ]
then
    echo "c solved by preprocessor"
    # check whether we need a witness, in which case we would have to run with the DRAT-enabled solver below
    if [ "x$doDRAT" != "x" ]
    then
        echo "c"
        echo "c rerun, due to proof generation ..."
        echo "c"
        exitCode=0
    else
        winningSolver="Coprocessor"
    fi
else
    echo "c not solved by preprocessor -- do search"
    if [ "$exitCode" -eq "0" ]
    then
        #
        # exit code == 0 -> could not solve the instance
        # dimacs file will be printed always
        # exit code could be 10 or 20, depending on whether coprocessor could solve the instance already
        #

        #
        # run your favorite solver (output is expected to look like in the SAT competition, s line and v line(s) )
        # and output to stdout of the sat solver is redirected to stderr
        #
        echo "c starting sparrow solver" 1>&2
        solveStart=`date +%s`
        "$SOLVERDIR"/$SLSsolver -a -l -k -r1 --timeout 900 --maxflips=500000000 $tmpCNF $seed > $model
        exitCode=$?
        solveEnd=`date +%s`
        echo "c solved $(( $solveEnd - $solveStart )) seconds" 1>&2

        #
        # undo the model
        # coprocessor can also handle "s UNSATISFIABLE"
        #
        echo "c post-process with coprocessor"
        "$SOLVERDIR"/coprocessor -post -undo=$undo -model=$model > $realModel

        #
        # verify final output if SAT?
        #
        if [ "$exitCode" -eq "10" ]
        then
            echo "c verify model ..."
            winningSolver="sparrow"
        fi
    else
        #
        # preprocessor returned some unwanted exit code
        #
        echo "c preprocessor has been unable to solve the instance"
        #
        # run sat solver on initial instance
        # and output to stdout of the sat solver is redirected to stderr
        #
        solveStart=`date +%s`
        "$SOLVERDIR"/$SLSsolver -a -l -k -r1 --timeout 900 --maxflips=500000000 $file $seed > $realModel
        exitCode=$?
        solveEnd=`date +%s`
        echo "c solved $(( $solveEnd - $solveStart )) seconds" 1>&2
    fi
fi

# use MergeSAT, in case we do not have a solution yet
if [ "$exitCode" -ne "10" -a "$exitCode" -ne "20" ]
then
    echo "c use MergeSAT" 1>&2
    # lets use MergeSAT for everything that could not be solved within the limits
    MergeSATStart=`date +%s`
    #
    # use MergeSAT
    # If DRAT should be used, the variable $drup contains the location to the proof, and disables FM in the solver (all other techniques work with DRAT)
    #
    "$SOLVERDIR"/mergesat $file $drup > $realModel
    exitCode=$?
    MergeSATEnd=`date +%s`
    echo "c MergeSAT used $(( $MergeSATEnd - $MergeSATStart)) seconds with exit code $exitCode" 1>&2
    if [ "$exitCode" -eq "10" -o "$exitCode" -eq "20" ]
    then
        winningSolver="MergeSAT"
    fi
fi


#
# print times
#
echo "c pp-time: $(( $ppEnd - $ppStart)) SLS-time: $(( $solveEnd - $solveStart ))  CDCL-time: $(( $MergeSATEnd - $MergeSATStart))" 1>&2
echo "c solved with: $winningSolver" 1>&2

#
# print solution
#
cat $realModel

#
# remove tmp files
#
rm -f $undo $undo.map $tmpCNF $model $realModel $ageFile $actFile

#
# return with correct exit code
#
exit $exitCode

