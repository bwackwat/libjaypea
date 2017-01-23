#!/bin/bash

thisdir=$(pwd)

cd $(dirname "${BASH_SOURCE[0]}")/../

source bin/build-prefix.sh

echo "building $1 as build/$2"

eval "$compiler $3 $thisdir/$1 -o $dir/build/$2"
