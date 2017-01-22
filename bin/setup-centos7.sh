#!/bin/bash

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

if [ ! -d "/opt/argon2" ]; then
	mkdir -p /opt/argon2
        git clone https://github.com/P-H-C/phc-winner-argon2.git /opt/argon2
	cp -n /opt/argon2/include/argon2.h /usr/include/
	cd /opt/argon2
	make
	cp -n ./libargon2.so /usr/local/lib/
	cp /usr/local/lib/libargon2.so /lib64/
	cp /lib64/libargon2.so /lib64/libargon2.so.0
	ldconfig
	cd ../
fi
