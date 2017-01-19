#!/bin/bash

yum makecache fast
yum -y upgrade

yum -y install epel-release clang fail2ban rkhunter certbot
yum -y install libstdc++-static libstdc++ cryptopp cryptopp-devel openssl openssl-devel
yum -y install firewalld

firewall-cmd --zone=public --add-service=http --permanent
firewall-cmd --zone=public --add-service=https --permanent
firewall-cmd --reload
systemctl enable firewall-cmd
