#!/bin/bash

HTTP=10080
HTTPS=10443
COMD=10000

if [ $# -lt 4 ]; then
	echo "Usage: <install dir> <comd key> <username> <hostname> <y or n> <optional password>"
	exit
fi

useradd $3

if ! [ -z $5 ]; then
	echo "$5" | passwd "$3" --stdin
fi

cd $1

chmod +x $1/bin/setup-centos7.sh
$1/bin/setup-centos7.sh 2>&1 | tee $1/setup-centos7.log

chmod +x $1/bin/build-library.sh
$1/bin/build-library.sh 2>&1 | tee $1/build-library.log

chmod +x $1/bin/build-example.sh

echo "$2" >> $1/keyfile.deploy

cat <<EOF >> $1/start.sh 
#!/bin/bash

$1/bin/build-example.sh comd PROD > $1/build-comd.log 2>&1

$1/bin/build-example.sh http-redirecter PROD > $1/build-http-redirecter.log 2>&1

$1/bin/build-example.sh message-api PROD > $1/build-message-api.log 2>&1

$1/build/comd \
--port $COMD \
--keyfile $1/keyfile.deploy \
> $1/comd.log 2>&1 &

$1/build/http-redirecter \
--hostname $4 \
--http $HTTP \
> $1/http-redirecter.log 2>&1 &

$1/build/message-api \
--public_directory $1/html \
--ssl_certificate $1/etc/ssl/ssl.crt \
--ssl_private_key $1/etc/ssl/ssl.key \
--https $HTTPS \
> $1/message-api.log 2>&1 &
EOF

chmod +x $1/start.sh

echo -e "\n@reboot $3 $1/start.sh" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=$HTTP
firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=$HTTPS
firewall-cmd --zone=public --permanent --add-port=$COMD/tcp
firewall-cmd --reload

# certbot certonly --standalone --tls-sni-01-port $HTTPS --domain $4
# cp /etc/letsencrypt/live/$4/fullchain.pem $1/etc/ssl/ssl.crt
# cp /etc/letsencrypt/live/$4/privkey.pem $1/etc/ssl/ssl.key

chown -R $3:$3 $1
