#!/bin/bash

# Allows listen backlog to be much larger.
# sysctl -w net.core.somaxconn=65535
# echo -e "\nnet.core.somaxconn=65535" >> /etc/sysctl.conf

echo -e "$#: $@"

cd $(dirname "${BASH_SOURCE[0]}")/../src
srcdir=$(pwd)
argc=$#
argv=($@)
optimize="-O0"

case "${argv[@]}" in *"OPT"*)
	optimize="-O3"
	((argc-=1))
	argv=( "${argv[@]/"OPT"}" )
esac

compiler="clang++ -std=c++11 -g $optimize -I$srcdir \
-Weverything -Wpedantic -Wconversion \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors"

fsanitize="-fsanitize=safe-stack"
fsanitize="-fsanitize=cfi"
hardened="-D_FORTIFY_SOURCE=2 -fstack-protector-all $fsanitize -Wformat -Wformat-security -Werror=format-security"

echo $compiler

function build {
	if [ $argc -eq 0 ] || [[ "$1" = *"$argv"* ]]; then
		echo -e "\n-------------------------building $1-------------------------\n"
		eval "$compiler $2 $3 $4 $5 $6 $7 $8 $9 util.cpp json.cpp -o ../bin/$1"
	fi
}

build json-test "examples/json-test.cpp"
build queue-test "examples/queue-test.cpp"
build pgsql-provider "-lpqxx -lcryptopp pgsql-model.cpp symmetric-encryptor.cpp symmetric-event-server.cpp tcp-event-server.cpp tcp-epoll-server.cpp examples/pgsql-provider.cpp"

build read-stdin-tty "examples/read-stdin-tty.cpp"
build keyfile-gen "-lcryptopp examples/keyfile-gen.cpp"

build tcp-poll-server "examples/tcp-poll-server.cpp"
build ponald "-lpthread examples/ponal-server.cpp tcp-event-server.cpp"
build comd "-lcryptopp -lpthread symmetric-encryptor.cpp examples/comd/comd-util.cpp examples/comd/comd.cpp tcp-epoll-server.cpp tcp-event-server.cpp symmetric-event-server.cpp"
build chat-server "-lpthread tcp-epoll-server.cpp tcp-event-server.cpp examples/chat-server.cpp"

build tcp-poll-client "examples/tcp-poll-client.cpp"
build ponal "examples/ponal-client.cpp simple-tcp-client.cpp"
build com "-lcryptopp symmetric-encryptor.cpp examples/comd/comd-util.cpp examples/comd/com.cpp tcp-event-client.cpp symmetric-event-client.cpp"
build chat-client "simple-tcp-client.cpp examples/chat-client.cpp"

build modern-web-monad "-lssl -lcryptopp -lcrypto -lpthread examples/modern-web-monad.cpp web-monad.cpp tcp-event-server.cpp tcp-epoll-server.cpp private-event-server.cpp"
build pgsql-web-monad "-lssl -lcryptopp -lcrypto -lpthread examples/pgsql-web-monad.cpp symmetric-encryptor.cpp symmetric-tcp-client.cpp simple-tcp-client.cpp web-monad.cpp tcp-event-server.cpp tcp-epoll-server.cpp private-event-server.cpp"
build echo-server "-lpthread tcp-event-server.cpp tcp-epoll-server.cpp examples/echo-server.cpp"

build tcp-client "-lpthread examples/tcp-client.cpp simple-tcp-client.cpp"
build tcp-event-client "examples/tcp-event-client.cpp tcp-event-client.cpp"
