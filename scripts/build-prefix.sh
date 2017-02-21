#!/bin/bash

dir=$(pwd)

mkdir -p $dir/binaries

argc=$#
argv=($@)

warn="-Wformat -Wformat-security -Werror=format-security \
-Weverything -Wpedantic -Wconversion \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors "

case "${argv[@]}" in
	 *"PROD"*)
		echo "PRODUCTION MODE"
		extra="-O3 -fsanitize=thread -fsanitize=undefined \
		-D_FORTIFY_SOURCE=2 -fstack-protector-all "
	
		((argc-=1))
		argv=( "${argv[@]/"PROD"}" )
		;;
	*"DEBUG"*)
		echo "DEBUG MODE"
		extra="-O0 -D_DO_DEBUG"
	
		((argc-=1))
		argv=( "${argv[@]/"DEBUG"}" )
		;;
	*)
		extra="-O0"
		warn="$warn -Wno-unused-parameter -Wno-unused-exception-parameter \
		-Wno-unused-variable "
esac

libs="-lpthread -lssl -lcryptopp"

libcompiler="clang++ -std=c++11 -fPIC -shared -I$dir/cpp-source \
$libs $warn $extra \
$dir/cpp-source/*.cpp -o $dir/artifacts/libjaypea.so"

compiler="clang++ -std=c++11 -I$dir/cpp-source $dir/artifacts/libjaypea.so $dir/artifacts/argon2/libargon2.so.0 -lcryptopp -lpqxx $warn $extra"
