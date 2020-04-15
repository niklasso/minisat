#!/bin/bash
#
# This file checks whether clang-format would modify the style of the repository
#
# This command can be used to get the style up and running:
#
#   find minisat -type f | xargs clang-format -i

# get to repository base
cd $(dirname "${BASH_SOURCE[0]}")/..

declare -i STATUS=0

if ! command -v clang-format &> /dev/null
then
    echo "error: could not find clang-format, abort"
    exit 1
fi

echo "info: use clang format, version: $(clang-format --version)"

# iterate over all files and check style
for file in $(find minisat -name "*.cc" -o -name "*.h" -type f)
do
	echo "check $file"
	REPLACEMENT=$(clang-format -i -output-replacements-xml "$file")

	REPLACEMENT_LINES=$(echo "$REPLACEMENT" | wc -l)

	if [ "$REPLACEMENT_LINES" -ne 3 ]
	then
		echo "style fails for $file"
		STATUS=1
	fi
done

if [ "$STATUS" -ne 0 ]; then
    echo "error: failed style check with status $STATUS"
fi
exit $STATUS
