#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../

xterm -e "python scripts/python/watcher.py cpp-source/ scripts/build-library.sh" &

xterm -e "python scripts/python/watcher.py cpp-source/examples,artifacts/libjaypea.so scripts/build-example.sh" &
