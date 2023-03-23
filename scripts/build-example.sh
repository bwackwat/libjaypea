#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../

source scripts/build-prefix.sh

ls -lah $dir
ls -lah $dir/artifacts

function build {
	if [ $argc -eq 0 ] || [[ "$1" = *"$argv"* ]]; then
		echo "compiling artifacts/$1"
		eval "$compiler $3 $dir/cpp-source/examples/$1.cpp -o $dir/artifacts/$1"
	fi
}

# Some libs need to be linked again because they are static?

if [ ! -z "$2" ]; then
	build $2
fi

build jph2

exit

build chat
build pqxxtest
build pgsql-provider
build bwackwat

build libjaypea-api
build http-redirecter

build music

build json-test
build queue-test
build read-stdin-tty
build keyfile-gen

build tcp-poll-server
build ponal-server
build comd
build chat-server

build tcp-poll-client
build ponal-client
build com
build chat-client

build echo-server
build tcp-event-client
