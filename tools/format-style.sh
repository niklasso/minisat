#!/bin/bash
#
# This script helps to apply the style of this project to the code base.
# The command can be run on the current tip of the git history, or based on
# the git history after changes have already been merged -- fixing the style
# of all involved commits.
#
# Usage:
# format-style.sh [BASE_COMMIT_ID]
#

# Get to repository base
cd $(dirname "${BASH_SOURCE[0]}")/..

declare -i STATUS=0

declare -a TOOL_CANDIDATES=("clang-format-6.0" "clang-format")

for TOOL in "${TOOL_CANDIDATES[@]}"
do
    if ! command -v "$TOOL" &> /dev/null
    then
        echo "error: could not find clang-format, abort"
        exit 1
    else
        break
    fi
done

echo "info: use clang format, version: $("$TOOL" --version)"

if [ -n "${1:-}" ]; then
    BASE_COMMIT="$1"
    TIP_COMMIT="$(git rev-parse --short HEAD)"
    echo "run formating for git history starting with base commit: $BASE_COMMIT"
    echo "started process with git commit $TIP_COMMIT"
    # TODO: check whether commit $BASE_COMMIT exists in the current branch
    
    # apply format command for each commit in the series
    git rebase -i --autosquash --exec 'tools/format-style.sh' $BASE_COMMIT || STATUS=$?
    if [ "$STATUS" -ne 0 ]; then
        echo "WARNING: make sure to fix the git history after this failed 'git rebase' command, executed on commit '$TIP_COMMIT' !"
        echo ""
        echo "Fix each commit, and run 'git rebase --continue' to fix the whole series. Take care of merge-conflicts, too!"
    fi
else
    # run the actual command on the commit itself
    find minisat -type f -name "*.cc" -o -name "*.h" | xargs clang-format -i || STATUS=$?
fi

exit $STATUS
