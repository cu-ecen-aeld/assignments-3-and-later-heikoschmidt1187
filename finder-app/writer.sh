#!/bin/sh
# Writes the given string to a given file
# Author: Heiko Schmidt

set -e
set -u

# check if sufficient parameters have been provided
if [ $# -lt 2 ]; then
	echo "Please provide path and string to write"
	exit 1
fi

# use names for parameters
WRITEFILE=$1
WRITESTR=$2

# get directory name from path to create it if not present
WRITEDIR="$(dirname "${WRITEFILE}")"

if [ ! -d "$WRITEDIR" ]; then
	mkdir -p "$WRITEDIR"
	if [ $? -ne 0 ]; then
	    echo "Failed to create target directory"
	    exit 1
	fi
fi

# write to the file, overwriting if needed
echo "$WRITESTR" > "$WRITEFILE"

if [ $? -ne 0 ]; then
	echo "Failed to write file"
	exit 1
fi

exit 0

