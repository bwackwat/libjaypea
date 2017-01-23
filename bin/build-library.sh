#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../

source bin/build-prefix.sh

echo "building libjaypea.so"
eval $libcompiler

ldconfig
