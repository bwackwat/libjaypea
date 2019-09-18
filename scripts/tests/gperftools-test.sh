#!/bin/bash

# http://www.brendangregg.com/perf.html

set -e

port=4001

HEAPPROFILE=artifacts/heap_profile artifacts/libjaypea-api -p $port &
ljpid=$!

sleep 2

curl https://localhost:$port --insecure

sleep 2

pprof --gv artifacts/libjaypea artifacts/heap_profile

pkill -9 -P $ljpid

echo "DONE!"

