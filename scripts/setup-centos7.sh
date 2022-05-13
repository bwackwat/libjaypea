#!/bin/bash

if [ "$(whoami)" != "root" ]; then
	echo "This must be run as root"
	exit 1
fi

set -ux

cd $(dirname "${BASH_SOURCE[0]}")/../

# openssl req -x509 -nodes -sha256 -newkey rsa:4096 -keyout extras/self-signed-ssl/ssl.key -out extras/self-signed-ssl/ssl.crt

yum -y update

yum -y install epel-release

yum -y install git firewalld fail2ban certbot ntp gperftools psmisc git-lfs
yum -y install clang gcc-c++ libpqxx-devel vim python3 python3-devel
yum -y install libstdc++-static libstdc++ cryptopp cryptopp-devel openssl openssl-devel argon2 libargon2-devel

pip3 install --upgrade pip3
pip3 install psutil

systemctl enable fail2ban
systemctl restart fail2ban
systemctl enable firewalld
systemctl restart firewalld
systemctl enable ntpd
systemctl restart ntpd
