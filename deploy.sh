#!/bin/bash

# This is pretty epic.
# echo "abc123" | passwd root --stdin

chmod +x /opt/setup-centos7.sh
/opt/setup-centos7.sh > /opt/setup-centos7.log 2>&1

chmod +x /opt/build.sh
/opt/build.sh > /opt/build.log 2>&1

echo "$2" >> /opt/deploy-keyfile

echo "/opt/bin/comd -p $1 -k /opt/keyfile.deploy > /opt/comd.log 2>&1 &" >> /etc/rc.d/rc.local
echo "/opt/bin/modern-web-monad -pd /opt/html > /opt/modern-web-monad.log 2>&1 &" >> /etc/rc.d/rc.local
chmod +x /etc/rc.d/rc.local

firewall-cmd --zone=public --add-port=80/tcp --permanent
firewall-cmd --zone=public --add-port=443/tcp --permanent
firewall-cmd --zone=public --add-port=$1/tcp --permanent
firewall-cmd --reload
