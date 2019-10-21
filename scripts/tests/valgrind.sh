#!/bin/bash

yum -y install valgrind

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose artifacts/jph2

valgrind --tool=memcheck --leak-check=yes artifacts/jph2

valgrind --tool=callgrind --logfile="artifacts/callgrind.out" artifacts/jph2
callgrind_annotate --inclusive=yes --tree=both --auto=yes artifacts/callgrind.out

valgrind --tool=cachegrind  --logfile="artifacts/cachegrind.out" artifacts/jph2
cg_annotate artifacts/cachegrind.out

