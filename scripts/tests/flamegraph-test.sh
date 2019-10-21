#!/bin/bash

# http://www.brendangregg.com/perf.html

set -e

port=4001

HEAPPROFILE=artifacts/heap_profile

./artifacts/libjaypea-api -p $port &
ljpid=$!

echo "ljpid: $ljpid"
ps

perf record -F 99 -a -g -p $ljpid &
perfpid=$!

sleep 2

curl https://localhost:$port --insecure

sleep 2

echo "kill -2 $perfid"
kill -2 $perfpid

sleep 2

echo "perf script > perf.out"
perf script > perf.out

sleep 2

../FlameGraph/stackcollapse-perf.pl perf.out > perf.folded

echo "FOLDED!"

sleep 2

../FlameGraph/flamegraph.pl perf.folded > public-html/media/flamegraph.svg

echo "kill -9 $ljpid"
pkill -9 -P $BASHPID
#kill -9 $ljpid

echo "DONE!"
