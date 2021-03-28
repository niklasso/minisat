#!/usr/bin/env bash
#
# Copyright Norbert Manthey, 2019

# This script is executed as default in the container

# Default location is defined by environment variable INPUT_CNF
INPUT_CNF=${INPUT_CNF:-}

# In case that variable is not given, we might have a CNF stored in S3
if [ -z "$INPUT_CNF" ]
then
    if ! command -v aws &> /dev/null
    then
        echo "error: cannot find tool 'aws', abort"
        exit 1
    fi
    
    # Get file basename
    PROBLEM=$(basename ${COMP_S3_PROBLEM_PATH})
    
    # Get file from S3
    aws s3 cp s3://"${S3_BKT}"/"${COMP_S3_PROBLEM_PATH}" /tmp/"$PROBLEM"
    
    INPUT_CNF=/tmp/"$PROBLEM"
fi


# Final check wrt input file
if [ ! -r "$INPUT_CNF" ]
then
    echo "error: cannot read input cnf '$INPUT_CNF'"
    exit 1
fi

# Actually run the solver on the resulting input CNF
/opt/mergesat/build/release/bin/mergesat "$INPUT_CNF"
