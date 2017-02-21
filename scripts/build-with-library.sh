#!/bin/bash

if [ $# -lt 2 ]; then
	echo "Usage: <relative path to cpp file> <executable name> <optional additional libraries/compiler flags>"
	exit
fi

thisdir=$(pwd)

cd $(dirname "${BASH_SOURCE[0]}")/../

source scripts/build-prefix.sh

echo "compiling $1 as libjaypea/binaries/$2"

eval "$compiler $3 $thisdir/$1 -o $dir/binaries/$2"
