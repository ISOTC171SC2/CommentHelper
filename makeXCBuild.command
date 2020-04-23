#!/bin/bash
set -e

# Note: This file can be stored in and run from the document section of the OS X dock.

# Change to the directory where this script lives.
cd "$(dirname $0)" >/dev/null

# create a new directory for the build
# if new, then cd into it
# if not, then cd into it and make sure it's empty
if mkdir -p xcBuild; then
	cd xcBuild
else
	cd xcBuild
	rm -R *
fi

# create the XCode project for the CMake
cmake -G "Xcode" ..
