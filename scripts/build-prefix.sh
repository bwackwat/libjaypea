#!/bin/bash

dir=$(pwd)

mkdir -p $dir/binaries
mkdir -p $dir/artifacts

argc=$#
argv=($@)

warn="-Wformat -Wall -Wformat-security -Werror=format-security \
-Weverything -Wpedantic -Wconversion \
-Wno-c++98-compat -Wno-padded \
-Wno-exit-time-destructors -Wno-global-constructors \
-Wno-unused-parameter -Wno-unused-exception-parameter -Wno-unused-variable "

library="$dir/artifacts/libjaypea.so"

case "${argv[@]}" in
	 *"PROD"*)
		echo "PRODUCTION MODE"
		library="$dir/artifacts/libjaypeap.so"
		extra="-O3 -fsanitize=undefined -D_FORTIFY_SOURCE=2 -fstack-protector-all "
	
		((argc-=1))
		argv=( "${argv[@]/"PROD"}" )
		;;
	*"DEBUG"*)
		echo "DEBUG MODE"
		extra="-O0 -DDO_DEBUG -g -lefence"
		# Flags for gperftools:
		# -pg -lprofile -ltcmalloc -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
	
		((argc-=1))
		argv=( "${argv[@]/"DEBUG"}" )
		;;
	*)
		extra="-O0"
esac

libs="-lpthread -lssl -lcryptopp -largon2"

libcompiler="clang++ -std=c++11 -fPIC -shared -I$dir/cpp-source \
$libs $warn $extra \
$dir/cpp-source/*.cpp -o $library"

compiler="clang++ -std=c++11 -I$dir/cpp-source $library  -lcryptopp -lpqxx $warn $extra"
