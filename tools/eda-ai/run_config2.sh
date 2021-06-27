#!/bin/bash
#
# Tuned EDA AI Configuration:


# locate the script to be able to call related scripts
SCRIPT=$(readlink -e "$0")
SCRIPTDIR=$(dirname "$SCRIPT")

# fail on error
set -e

INPUT_FILE="$1"

if [ ! -r "$INPUT_FILE" ]; then
    echo "Cannot read input file $INPUT_FILE, abort"
    exit 1
fi

exec "$SCRIPTDIR"/mergesat  \
      -VSIDS-init-lim=10000\
     -VSIDS-lim=30000000\
     -no-almost-pure\
     -ccmin-mode=2\
     -ccnr-change-time=2000\
     -ccnr-change-time-inc=1\
     -ccnr-change-time-inc-inc=0.2\
     -ccnr-conflict-ratio=0.4\
     -no-ccnr-initial\
     -ccnr-ls-mems=50000000\
     -no-ccnr-mediation\
     -ccnr-percent-ratio=0.9\
     -ccnr-restart-gap=300\
     -ccnr-switch-heuristic=500\
     -ccnr-up-time-ratio=0.2\
     -chrono=100\
     -cl-lim=20\
     -cla-decay=0.9990000000000001\
     -confl-to-chrono=4000\
     -core-size-lim=50000\
     -core-size-lim-inc=0.1\
     -elim\
     -gc-frac=0.20000000000000023\
     -grow=0\
     -inprocess-delay=2.0\
     -inprocess-init-delay=-1\
     -inprocess-learnt-level=2\
     -inprocess-penalty=2\
     -lbd-avg-compare-limit=0.8000000000000004\
     -lcm\
     -lcm-core\
     -lcm-delay=1000\
     -lcm-delay-inc=1000\
     -lcm-dup-buffer=16\
     -lcm-reverse\
     -max-act-bump=100\
     -max-lbd-calc=100\
     -max-simp-cls=2147483647\
     -max-simp-steps=40000000000\
     -min-step-size=0.06000000000000003\
     -phase-saving=2\
     -pre\
     -pref-assumpts\
     -rfirst=100\
     -rinc=2.0\
     -rnd-freq=0.0\
     -rnd-init=0\
     -rnd-seed=9.16482529999E7\
     -rtype=2\
     -simp-gc-frac=0.49999999999999994\
     -sls-clause-lim=-1\
     -sls-var-lim=-1\
     -step-size=0.40000000000000013\
     -step-size-dec=1.0E-4\
     -sub-lim=1000\
     -use-backup-trail\
     -use-ccnr\
     -use-rephasing\
     -var-decay=0.8000000000000004\
     -var-decay-conflicts=5000\
     -vsids-c=12000000\
     -vsids-p=3000000000 \
     "$INPUT_FILE"
