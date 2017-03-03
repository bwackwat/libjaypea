#!/bin/bash

set -e

git fetch
git pull origin master

scripts/build-library.sh PROD

scripts/build-example.sh PROD
