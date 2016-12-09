#!/bin/bash

compiler="clang++ -std=c++11 -g -O0 \
-Weverything -Wpedantic \
-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors"

function build {
	echo -e "\n\n"
	echo -e "-------------------------BUILDING-------------------------\n"
	echo "$compiler $1 $2 $3 $4 $5 $6 $7 $8 $9"
	echo -e "\n\n"
	eval "$compiler $1 $2 $3 $4 $5 $6 $7 $8 $9"
}

build "./src/tcp-poll-server.cpp -o ./bin/tcp-poll-server"
build "-lpthread ./src/tcp-client.cpp ./src/util.cpp ./src/simple-tcp-client.cpp -o ./bin/tcp-client"
build "./src/json.cpp ./src/json-test.cpp ./src/util.cpp -o ./bin/json-test"
build "-lssl -lcrypto ./src/modern-web-monad.cpp ./src/simple-tcp-server.cpp ./src/json.cpp -o ./bin/modern-web-monad"
build "./src/ponal-client.cpp ./src/simple-tcp-client.cpp -o ./bin/ponal"
build "./src/ponal-server.cpp ./src/util.cpp ./src/simple-tcp-server.cpp -o ./bin/ponald"
build "./src/read-stdin-tty.cpp -o ./bin/read-stdin-tty"
build "-lcryptopp ./src/comd.cpp ./src/comd-util.cpp ./src/simple-tcp-server.cpp ./src/util.cpp -o ./bin/comd"
build "-lcryptopp ./src/com.cpp ./src/comd-util.cpp ./src/simple-tcp-client.cpp ./src/util.cpp -o ./bin/com"
build "-lcryptopp ./src/keyfile-gen.cpp -o ./bin/keyfile-gen"
build "-lpqxx ./src/pgsql-model.cpp ./src/json.cpp ./src/pgsql-model-test.cpp -o ./bin/pgsql-model-test"
