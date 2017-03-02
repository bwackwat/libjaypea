#!/bin/bash

dir=$(pwd)

mkdir -p $dir/binaries
mkdir -p $dir/artifacts

argc=$#
argv=($@)

warn="-Wformat -Wformat-security -Werror=format-security \
-Weverything -Wpedantic -Wconversion \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors "

library="$dir/artifacts/libjaypea.so"

case "${argv[@]}" in
	 *"PROD"*)
		echo "PRODUCTION MODE"
		library="$dir/artifacts/libjaypeap.so"
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
$dir/cpp-source/*.cpp -o $library"

compiler="clang++ -std=c++11 -I$dir/cpp-source $library -lcryptopp -lpqxx $warn $extra"
