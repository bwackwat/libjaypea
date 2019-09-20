#!/bin/bash

killall watcher.py
killall python
killall artifacts/jph2

set -e
set -x

if [ ! -f "artifacts/configuration.json" ]; then
	cp extras/configuration.json artifacts/configuration.json
fi

scripts/python/watcher.py cpp-source/,cpp-source/examples/,scripts/build-example.sh,scripts/build-library.sh,scripts/build-prefix.sh "scripts/build-library.sh DEBUG && scripts/build-example.sh DEBUG" > logs/develop-build-watcher.log 2>&1 &

scripts/python/watcher.py scripts/python/site-templater.py,scripts/python/site-indexer.py "scripts/python/site-indexer.py ../affable-escapade y && scripts/python/site-templater.py ../affable-escapade" > logs/site-scripts-watcher.log 2>&1 &

scripts/python/watcher.py artifacts/jph2,artifacts/configuration.json "artifacts/jph2" > logs/jph2-watcher.log 2>&1 &

scripts/python/migration/backup.py

echo "DEVELOPMENT RESTARTED"

tail -f logs/develop-build-watcher.log

