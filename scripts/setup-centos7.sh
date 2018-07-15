#!/bin/bash

cd $(dirname "${BASH_SOURCE[0]}")/../

# openssl req -x509 -nodes -sha256 -newkey rsa:4096 -keyout extras/self-signed-ssl/ssl.key -out extras/self-signed-ssl/ssl.crt

yum -y install epel-release

yum -y install git firewalld fail2ban certbot ntp gperftools psmisc git-lfs
yum -y install clang gcc-c++ libpqxx-devel vim python-pip python-devel
yum -y install libstdc++-static libstdc++ cryptopp cryptopp-devel openssl openssl-devel

pip install --upgrade pip
pip install psutil

systemctl enable fail2ban
systemctl restart fail2ban
systemctl enable firewalld
systemctl restart firewalld
systemctl enable ntpd
systemctl restart ntpd

mkdir -p artifacts

if [ -d "artifacts/argon2" ]; then
	rm -rf artifacts/argon2
fi

git clone https://github.com/P-H-C/phc-winner-argon2.git artifacts/argon2
cd artifacts/argon2
make
make install
ldconfig
