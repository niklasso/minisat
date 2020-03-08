#!/bin/bash
#
# This file checks whether clang-format would modify the style of the repository
#
# This command can be used to get the style up and running:
#
#   find minisat -type f | xargs clang-format -i

# get to repository base
cd "${BASH_SOURCE[0]}"/..

declare -i STATUS=0

# iterate over all files and check style
for file in $(find minisat -name "*.cc" -o -name "*.h" -type f)
do
	echo "check $file"
	REPLACEMENT=$(clang-format -i -output-replacements-xml "$file")

	REPLACEMENT_LINES=$(echo "$REPLACEMENT" | wc -l)

	if [ "$REPLACEMENT_LINES" -ne 3 ]
	then
		echo "style fails for $file"
		echo "$REPLACEMENT"
		STATUS=1
	fi
done

exit $STATUS
