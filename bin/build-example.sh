#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../

source bin/build-prefix.sh

function build {
	if [ $argc -eq 0 ] || [[ "$1" = *"$argv"* ]]; then
		echo "building $1"
		eval "$compiler $2 $dir/src/examples/$1.cpp -o $dir/build/$1"
	fi
}

# Some libs need to be linked again because they are static?

build json-test
build queue-test
build pgsql-provider "-lcryptopp -largon2"
build read-stdin-tty
build keyfile-gen "-lcryptopp"

build tcp-poll-server
build ponal-server
build comd "-lcryptopp"
build chat-server

build tcp-poll-client
build ponal-client
build com "-lcryptopp"
build chat-client

build message-api
build bwackwat "-lcryptopp"
build wss-server "-lcryptopp"
build http-redirecter
build echo-server
build tcp-client
build tcp-event-client
