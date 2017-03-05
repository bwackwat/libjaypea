#!/bin/bash

master_commit="artifacts/master.latest.commit"

python scripts/python/watcher.py $master_commit "git fetch && git pull origin master && scripts/build-library.sh PROD && scripts/build-example.sh PROD && cp -r binaries/* public-html/build/ && cp -r $master_commit public-html/build/" > build-server-master-updater.log 2>&1
