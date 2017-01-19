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
