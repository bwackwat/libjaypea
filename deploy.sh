#!/bin/bash

# This is pretty epic.
# echo "abc123" | passwd root --stdin

cd $1

chmod +x setup-centos7.sh
setup-centos7.sh > setup-centos7.log 2>&1

chmod +x build.sh
build.sh > build.log 2>&1

echo "$3" >> deploy-keyfile

echo -e "$1/bin/comd \
--port $2 \
--keyfile $1/keyfile.deploy \
> $1/comd.log 2>&1 &" >> /etc/rc.d/rc.local
echo -e "$1/bin/modern-web-monad \
--public_directory $1/html \
--ssl_certificate $1/etc/ssl/ssl.crt \
--ssl_private_key $1/etc/ssl/ssl.key \
> $1/modern-web-monad.log 2>&1 &" >> /etc/rc.d/rc.local

chmod +x /etc/rc.d/rc.local

firewall-cmd --zone=public --add-port=80/tcp --permanent
firewall-cmd --zone=public --add-port=443/tcp --permanent
firewall-cmd --zone=public --add-port=$2/tcp --permanent
firewall-cmd --reload
