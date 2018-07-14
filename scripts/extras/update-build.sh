#!/bin/bash

git fetch
git pull origin master

scripts/build-library.sh PROD
scripts/build-example.sh PROD

# This is for the build server.

cp -f artifacts/libjaypeap.so public-html/build/
cp -f binaries/* public-html/build/
cp -f artifacts/libjaypea.master.latest.commit public-html/build/

touch artifacts/ready.lock
