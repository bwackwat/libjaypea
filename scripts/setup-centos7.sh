#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../


yum -y install epel-release

yum -y install git firewalld fail2ban certbot
yum -y install clang gcc-c++ libpqxx-devel
yum -y install libstdc++-static libstdc++ cryptopp cryptopp-devel openssl openssl-devel

systemctl enable fail2ban
systemctl restart fail2ban
systemctl enable firewalld
systemctl restart firewalld

mkdir -p artifacts

if [ -d "artifacts/argon2" ]; then
	rm -rf artifacts/argon2
fi

git clone https://github.com/P-H-C/phc-winner-argon2.git artifacts/argon2
cd artifacts/argon2
make
make install
