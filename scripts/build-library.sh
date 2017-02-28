#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../

source scripts/build-prefix.sh

echo "compiling artifacts/libjaypea.so"
eval $libcompiler

rm artifacts/build.lock
