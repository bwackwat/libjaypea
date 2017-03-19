#!/bin/bash

set -e

echo "--------------------------------------------------"

xterm -geometry 100x5 \
-T "watcher building libjaypea.so" \
-e scripts/python/watcher.py cpp-source/,scripts/build-library.sh,scripts/build-prefix.sh "scripts/build-library.sh $1" &

echo "--------------------------------------------------"

xterm -geometry 100x25 \
-T "watcher building examples" \
-e scripts/python/watcher.py cpp-source/examples/,artifacts/libjaypea.so,artifacts/libjaypeap.so,scripts/build-example.sh "scripts/build-example.sh $1" &

echo "--------------------------------------------------"
