#!/bin/bash

killall watcher.py

set -e

sleep 1

xterm -geometry 100x27 \
-T "watcher building libjaypea.so" \
-e scripts/python/watcher.py cpp-source/,scripts/build-library.sh,scripts/build-prefix.sh "scripts/build-library.sh DEBUG" &

sleep 1

xterm -geometry 100x27 \
-T "watcher building examples" \
-e scripts/python/watcher.py cpp-source/examples/,cpp-source/examples/jph2/,artifacts/libjaypea.so,artifacts/libjaypeap.so,scripts/build-example.sh "scripts/build-example.sh DEBUG" &

sleep 1

xterm -geometry 100x5 \
-T "watcher preparing affable-escapade" \
-e scripts/python/watcher.py cpp-source/examples/,scripts/python/site-templater.py,scripts/python/site-indexer.py "scripts/python/site-indexer.py ../affable-escapade y && scripts/python/site-templater.py ../affable-escapade" &

sleep 1

xterm -geometry 100x30 \
-T "watcher running jph2" \
-e scripts/python/watcher.py artifacts/jph2,extras/configuration.json "artifacts/jph2" &

./scripts/python/migration/backup.py

