#!/bin/bash

yum -y install valgrind

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./binaries/jph2

valgrind --tool=memcheck --leak-check=yes binaries/jph2

valgrind --tool=callgrind --logfile="artifacts/callgrind.out" ./binaries/jph2
callgrind_annotate --inclusive=yes --tree=both --auto=yes artifacts/callgrind.out

valgrind --tool=cachegrind  --logfile="artifacts/cachegrind.out" binaries/jph2
cg_annotate artifacts/cachegrind.out
