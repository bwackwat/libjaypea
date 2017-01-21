#!/bin/bash

HTTP=10080
HTTPS=10443
COMD=10000

if [ $# -lt 3 ]; then
	echo "Usage: <install dir> <comd key> <username> <optional password>"
	exit
fi

if ! [ -z $4 ]; then
	echo "$4" | passwd "$3" --stdin
fi

cd $1

chmod +x $1/bin/setup-centos7.sh
$1/bin/setup-centos7.sh 2>&1 | tee $1/setup-centos7.log

chmod +x $1/bin/build.sh

echo "$2" >> $1/keyfile.deploy

cat <<EOF >> $1/start.sh 
#!/bin/bash

$1/bin/build.sh comd OPT > $1/build-comd.log 2>&1

$1/bin/build.sh modern-web-monad OPT > $1/build-comd.log 2>&1

$1/build/comd \
--port $COMD \
--keyfile $1/keyfile.deploy \
> $1/comd.log 2>&1 &

$1/build/modern-web-monad \
--public_directory $1/html \
--ssl_certificate $1/etc/ssl/ssl.crt \
--ssl_private_key $1/etc/ssl/ssl.key \
--http $HTTP \
--https $HTTPS \
> $1/modern-web-monad.log 2>&1 &
EOF

chmod +x $1/start.sh

echo -e "\n@reboot $3 $1/start.sh" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=$HTTP
firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=$HTTPS
firewall-cmd --zone=public --permanent --add-port=$COMD/tcp
firewall-cmd --reload

chown -R $3:$3 $1
