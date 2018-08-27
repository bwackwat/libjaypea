#!/bin/bash

git fetch
git pull origin master

scripts/build-library.sh PROD
scripts/build-example.sh PROD

# This is for the build server.

mkdir -p build/

cp -f artifacts/libjaypeap.so build/
cp -f binaries/* build/
cp -f artifacts/libjaypea.master.latest.commit build/

touch artifacts/ready.lock
