#!/bin/bash

compiler="clang++ -std=c++11 -g -O0 -Weverything -Wextra -Wall -pedantic"
function build {
	eval "$compiler $1 $2 $3 $4"
}

build ./src/tcp_poll_server.cpp -o ./bin/tcp_poll_server
build ./src/tcp_client.cpp -o ./bin/tcp_client
