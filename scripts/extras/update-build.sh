#!/bin/bash

git fetch
git pull origin master

scripts/build-library.sh PROD
scripts/build-example.sh PROD

cp -f artifacts/libjaypeap.so public-html/build/
cp -f binaries/* public-html/build/
cp -f artifacts/libjaypea.master.latest.commit public-html/build/

touch artifacts/ready.lock
