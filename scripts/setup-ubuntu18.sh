#!/bin/bash

if [ "$(whoami)" != "root" ]; then
	echo "This must be run as root or using sudo."
	exit 1
fi

set -eux

cd $(dirname "${BASH_SOURCE[0]}")/../

apt-get install libcrypto++-dev libpqxx-dev libssl1.1 libssl1.0.0 clang libargon2.0-dev
