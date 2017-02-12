#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../

source scripts/build-prefix.sh

echo "building libjaypea.so"
eval $libcompiler

#ldconfig
