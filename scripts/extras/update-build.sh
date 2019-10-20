#!/bin/bash

git fetch
git pull origin master

scripts/build-library.sh PROD
scripts/build-example.sh PROD

# \ Run the command without aliases.
\cp -f artifacts/libjaypeap.so ../affable-escapade/build/
\cp -f artifacts/libjaypea.so ../affable-escapade/build/
\cp -f artifacts/jph2 ../affable-escapade/build/

touch artifacts/ready.lock
