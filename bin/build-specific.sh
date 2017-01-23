#!/bin/bash

# Allows listen backlog to be much larger.
# sysctl -w net.core.somaxconn=65535
# echo -e "\nnet.core.somaxconn=65535" >> /etc/sysctl.conf

echo -e "$#: $@"

cd $(dirname "${BASH_SOURCE[0]}")/../src
srcdir=$(pwd)
mkdir -p $srcdir/../build
echo -e "$srcdir"
argc=$#
argv=($@)

compiler="clang++ -std=c++11 -rdynamic -g -O0 -I$srcdir/ \
-Weverything -Wpedantic -Wconversion \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors"

case "${argv[@]}" in *"PROD"*)
	compiler="clang++ -std=c++11 -O3 -I$srcdir/ \
	-fsanitize=thread -fsanitize=undefined \
	-D_FORTIFY_SOURCE=2 -fstack-protector-all \
	-Wformat -Wformat-security -Werror=format-security \
	-Weverything -Wpedantic -Wconversion \
	-Wno-c++98-compat -Wno-padded \
	-Wno-exit-time-destructors -Wno-global-constructors"
	
	((argc-=1))
	argv=( "${argv[@]/"PROD"}" )
esac

echo $compiler

function build {
	if [ $argc -eq 0 ] || [[ "$1" = *"$argv"* ]]; then
		echo -e "\n-------------------------building $1-------------------------\n"
		eval "$compiler $2 $3 $4 $5 $6 $7 $8 $9 util.cpp json.cpp -o $srcdir/../build/$1"
	fi
}

build json-test "examples/json-test.cpp"
build queue-test "examples/queue-test.cpp"
build pgsql-provider "-lpqxx -lcryptopp -largon2 pgsql-model.cpp symmetric-encryptor.cpp symmetric-tcp-server.cpp tcp-server.cpp examples/pgsql-provider.cpp"

build read-stdin-tty "examples/read-stdin-tty.cpp"
build keyfile-gen "-lcryptopp examples/keyfile-gen.cpp"

build tcp-poll-server "examples/tcp-poll-server.cpp"
build ponald "-lpthread examples/ponal-server.cpp tcp-server.cpp"
build comd "-lcryptopp -lpthread symmetric-encryptor.cpp comd-util.cpp examples/comd.cpp tcp-server.cpp symmetric-tcp-server.cpp"
build chat-server "-lpthread tcp-server.cpp examples/chat-server.cpp"

build tcp-poll-client "examples/tcp-poll-client.cpp"
build ponal "examples/ponal-client.cpp simple-tcp-client.cpp"
build com "-lcryptopp symmetric-encryptor.cpp comd-util.cpp examples/com.cpp tcp-client.cpp symmetric-event-client.cpp"
build chat-client "simple-tcp-client.cpp examples/chat-client.cpp"

build modern-web-monad "-lssl -lcryptopp -lcrypto -lpthread examples/modern-web-monad.cpp web-monad.cpp tcp-server.cpp private-tcp-server.cpp"
build pgsql-web-monad "-lssl -lcryptopp -lcrypto -lpthread examples/pgsql-web-monad.cpp symmetric-encryptor.cpp symmetric-tcp-client.cpp simple-tcp-client.cpp web-monad.cpp tcp-server.cpp private-tcp-server.cpp"
build echo-server "-lpthread tcp-server.cpp examples/echo-server.cpp"

build tcp-client "-lpthread examples/tcp-client.cpp simple-tcp-client.cpp"
build tcp-event-client "examples/tcp-event-client.cpp tcp-client.cpp"
