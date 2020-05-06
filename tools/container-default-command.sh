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


# In case we cannot unzip the file ourselves, try a tool
if [[ "${INPUT_CNF}" == *".xz" ]];
then
    BASE_INPUT=$(basename "$INPUT_CNF" .xz)
    cp "$INPUT_CNF" /tmp/
    unxz /tmp/"${BASE_INPUT}.xz"
    INPUT_CNF=/tmp/"$BASE_INPUT"
elif [[ "${INPUT_CNF}" == *".bz2" ]];
then
    BASE_INPUT=$(basename "$INPUT_CNF" .bz2)
    cp "$INPUT_CNF" /tmp/
    bunzip2 /tmp/"${BASE_INPUT}.bz2"
    INPUT_CNF=/tmp/"$BASE_INPUT"
fi

# Final check wrt input file
if [ ! -r "$INPUT_CNF" ]
then
    echo "error: cannot read input cnf '$INPUT_CNF'"
    exit 1
fi

# Actually run the solver on the resulting input CNF
/opt/mergesat/build/release/bin/mergesat "$INPUT_CNF"
