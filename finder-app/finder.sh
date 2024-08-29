#!/bin/sh
# Searches for string in files
# Author: Heiko Schmidt

set -e
set -u

# check if sufficient parameters have been provided
if [ $# -lt 2 ]; then
	echo "Please provide path and string to write"
	exit 1
fi

# use names for parameters
FILESDIR=$1
SEARCHSTR=$2

if [ ! -d "$FILESDIR" ]; then
	echo "${FILESDIR} is not a directory"
	exit 1
fi

# get the number of files recursively using find and wc to count lines
FILECOUNT=$(find "$FILESDIR" -type f | wc -l)
if [ $? -ne 0 ]; then
	echo "Failed to call find"
	exit 1
fi

# recursively get files with matching string and count as above
STRINGCOUNT=$(grep -rF "${SEARCHSTR}" "${FILESDIR}" | wc -l)
if [ $? -ne 0 ]; then
	echo "Failed to grep files"
	exit 1
fi

echo "The number of files are ${FILECOUNT} and the number of matching lines are ${STRINGCOUNT}"
exit 0
