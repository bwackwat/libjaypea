#!/bin/bash

killall watcher.py

set -e

xterm -geometry 100x5 \
-T "watcher building libjaypea.so" \
-e scripts/python/watcher.py cpp-source/,scripts/build-library.sh,scripts/build-prefix.sh "scripts/build-library.sh $1" &

xterm -geometry 100x5 \
-T "watcher building examples" \
-e scripts/python/watcher.py cpp-source/examples/,artifacts/libjaypea.so,artifacts/libjaypeap.so,scripts/build-example.sh "scripts/build-example.sh $1" &

xterm -geometry 100x5 \
-T "watcher running jph2" \
-e scripts/python/watcher.py binaries/jph2,extras/configuration.json "binaries/jph2" &

xterm -geometry 100x5 \
-T "watcher running http-redirecter" \
-e scripts/python/watcher.py binaries/http-redirecter,extras/configuration.json "binaries/http-redirecter -hn localhost -p 10080 ../affable-escapade" &

xterm -geometry 100x5 \
-T "watcher preparing affable-escapade" \
-e scripts/python/watcher.py cpp-source/examples/,scripts/python/site-templater.py,scripts/python/site-indexer.py "scripts/python/site-indexer.py ../affable-escapade y && scripts/python/site-templater.py ../affable-escapade" &
