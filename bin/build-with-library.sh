#!/bin/bash

# Allows listen backlog to be much larger.
# sysctl -w net.core.somaxconn=65535
# echo -e "\nnet.core.somaxconn=65535" >> /etc/sysctl.conf

cd $(dirname "${BASH_SOURCE[0]}")/../
dir=$(pwd)
mkdir -p $dir/build

argc=$#
argv=($@)

extra="-O0"
case "${argv[@]}" in *"PROD"*)
	extra="-O3 -fsanitize=thread -fsanitize=undefined \
	-D_FORTIFY_SOURCE=2 -fstack-protector-all "
	
	((argc-=1))
	argv=( "${argv[@]/"PROD"}" )
esac

warn="-Wformat -Wformat-security -Werror=format-security \
-Weverything -Wpedantic -Wconversion \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors "

compiler="clang++ -std=c++11 -I$dir/src -L$dir/build -ljaypea $warn $extra" 

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

build modern-web-monad
build pgsql-web-monad "-lcryptopp"
build echo-server
build tcp-client
build tcp-event-client
