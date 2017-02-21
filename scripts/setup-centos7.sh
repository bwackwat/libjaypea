#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../

mkdir -p logs
mkdir -p artifacts

yum makecache fast
yum -y upgrade

yum -y install epel-release

yum -y install clang fail2ban rkhunter certbot gcc-c++ libpqxx-devel
yum -y install libstdc++-static libstdc++ cryptopp cryptopp-devel openssl openssl-devel
yum -y install firewalld

systemctl enable fail2ban
systemctl restart fail2ban
systemctl enable firewalld
systemctl restart firewalld

rkhunter --update
rkhunter --propupd

if [ -d "artifacts/argon2"]; then
	rm -rf artifacts/argon2
fi

git clone https://github.com/P-H-C/phc-winner-argon2.git artifacts/argon2
cd artifacts/argon2
make
