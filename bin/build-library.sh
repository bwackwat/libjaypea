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

libs="-lpthread -lssl -lpqxx -lcryptopp"

warn="-Wformat -Wformat-security -Werror=format-security \
-Weverything -Wpedantic -Wconversion \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors "

compiler="clang++ -std=c++11 -fPIC -shared -I$dir/src \
$libs $warn $extra \
$dir/src/*.cpp -o $dir/build/libjaypea.so"

echo "building libjaypea"
eval $compiler
