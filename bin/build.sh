#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../src

argc=$#
argv=$*

compiler="clang++ -std=c++11 -g -O0 \
-Weverything -Wpedantic \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors"

function build {
	if [ $argc -eq 0 ] || [[ $@ == *$argv* ]]; then
		echo -e "\n-------------------------building $1-------------------------\n"
		eval "$compiler $2 $3 $4 $5 $6 $7 $8 $9 -o ../bin/$1"
	fi
}

build tcp-poll-server "tcp-poll-server.cpp util.cpp"
build tcp-poll-client "tcp-poll-client.cpp util.cpp"
build tcp-client "-lpthread tcp-client.cpp util.cpp simple-tcp-client.cpp"
build json-test "json.cpp json-test.cpp util.cpp"
build modern-web-monad "-lssl -lcrypto modern-web-monad.cpp util.cpp simple-tcp-server.cpp json.cpp"
build ponal "ponal-client.cpp util.cpp simple-tcp-client.cpp"
build ponald "ponal-server.cpp util.cpp simple-tcp-server.cpp"
build read-stdin-tty "read-stdin-tty.cpp util.cpp"
build comd "-lcryptopp comd.cpp comd-util.cpp simple-tcp-server.cpp util.cpp"
build com "-lcryptopp com.cpp comd-util.cpp simple-tcp-client.cpp util.cpp"
build keyfile-gen "-lcryptopp keyfile-gen.cpp"
build pgsql-model-test "-lpqxx pgsql-model.cpp json.cpp pgsql-model-test.cpp"
build tcp-event-server "tcp-event-server.cpp util.cpp"
