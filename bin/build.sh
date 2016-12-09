#!/bin/bash

argc=$#
argv=$*

compiler="clang++ -std=c++11 -g -O0 \
-Weverything -Wpedantic \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors"

function build {
	if [ $argc -eq 0 ] || [[ $@ == *$argv* ]]; then 
		echo -e "\n\n"
		echo -e "-------------------------BUILDING-------------------------\n"
		echo "$compiler $2 $3 $4 $5 $6 $7 $8 $9 -o ./bin/$1"
		echo -e "\n\n"
		eval "$compiler $2 $3 $4 $5 $6 $7 $8 $9 -o ./bin/$1"
	fi
}

build tcp-poll-server "./src/tcp-poll-server.cpp ./src/util.cpp"
build tcp-client "-lpthread ./src/tcp-client.cpp ./src/util.cpp ./src/simple-tcp-client.cpp"
build json-test "./src/json.cpp ./src/json-test.cpp ./src/util.cpp"
build modern-web-monad "-lssl -lcrypto ./src/modern-web-monad.cpp ./src/util.cpp ./src/simple-tcp-server.cpp ./src/json.cpp"
build ponal "./src/ponal-client.cpp ./src/util.cpp ./src/simple-tcp-client.cpp"
build ponald "./src/ponal-server.cpp ./src/util.cpp ./src/simple-tcp-server.cpp"
build read-stdin-tty "./src/read-stdin-tty.cpp ./src/util.cpp"
build comd "-lcryptopp ./src/comd.cpp ./src/comd-util.cpp ./src/simple-tcp-server.cpp ./src/util.cpp"
build com "-lcryptopp ./src/com.cpp ./src/comd-util.cpp ./src/simple-tcp-client.cpp ./src/util.cpp"
build keyfile-gen "-lcryptopp ./src/keyfile-gen.cpp"
build pgsql-model-test "-lpqxx ./src/pgsql-model.cpp ./src/json.cpp ./src/pgsql-model-test.cpp"
