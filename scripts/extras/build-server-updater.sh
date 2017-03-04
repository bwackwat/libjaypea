#!/bin/bash

master_commit="artifacts/master.latest.commit"

scripts/python/watcher.py $master_commit "scripts/extras/redeploy.sh && cp -r binaries/* public-html/build/ && cp -r $master_commit public-html/build/
