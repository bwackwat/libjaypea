#!/bin/bash

# Allows listen backlog to be much larger.
# sysctl -w net.core.somaxconn=65535
# echo -e "\nnet.core.somaxconn=65535" >> /etc/sysctl.conf

dir=$(pwd)

mkdir -p $dir/build

argc=$#
argv=($@)

warn="-Wformat -Wformat-security -Werror=format-security \
-Weverything -Wpedantic -Wconversion \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors "

case "${argv[@]}" in
	 *"PROD"*)
		extra="-O3 -fsanitize=thread -fsanitize=undefined \
		-D_FORTIFY_SOURCE=2 -fstack-protector-all "
	
		((argc-=1))
		argv=( "${argv[@]/"PROD"}" )
		;;
	*"DEBUG"*)
		extra="-O0 -D DO_DEBUG"
	
		((argc-=1))
		argv=( "${argv[@]/"DEBUG"}" )
		;;
	*)
		extra="-O0"
		warn="$warn -Wno-unused-parameter -Wno-unused-exception-parameter \
		-Wno-unused-variable "
esac

libs="-lpthread -lssl -lcryptopp"

compiler="clang++ -std=c++11 -I$dir/src -ljaypea -lcryptopp -lpqxx $warn $extra" 

libcompiler="clang++ -std=c++11 -fPIC -shared -I$dir/src \
$libs $warn $extra \
$dir/src/*.cpp -o /lib64/libjaypea.so"
