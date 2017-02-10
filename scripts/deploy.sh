#!/bin/bash

#HTTP=10080
#HTTPS=10443
#COMD=10000
DIST=10001

if [ $# -lt 4 ]; then
	echo "Usage: <install dir> <comd key> <username> <hostname> <y or n> <optional password>"
	exit
fi

useradd $3
if ! [ -z $5 ]; then
	echo "$5" | passwd "$3" --stdin
fi

cd $1

mkdir -p $1/logs
mkdir -p $1/artifacts

chmod +x $1/scripts/setup-centos7.sh
$1/scripts/setup-centos7.sh 2>&1 | tee $1/logs/setup-centos7.log

chmod +x $1/scripts/build-library.sh
$1/scripts/build-library.sh 2>&1 | tee $1/logs/build-library.log

chmod +x $1/scripts/build-example.sh

echo "$2" >> $1/artifacts/deploy.keyfile

touch $1/artifacts/start-services.sh
chmod +x $1/artifacts/start-services.sh

cat <<EOF >> $1/artifacts/start.sh 
#!/bin/bash

$1/scripts/build-example.sh distributed-server PROD > $1/logs/build-distributed-server.log 2>&1

$1/binaries/distributed-server \
--port $DIST \
--keyfile $1/artifacts/deploy.keyfile

$1/artifacts/start-services.sh

EOF

chmod +x $1/artifacts/start.sh

echo -e "\n@reboot $3 $1/artifacts/start.sh" >> /etc/crontab

#firewall-cmd --zone=public --permanent --add-masquerade
#firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=$HTTP
#firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=$HTTPS
#firewall-cmd --zone=public --permanent --add-port=$COMD/tcp
firewall-cmd --zone=public --permanent --add-port=$DIST/tcp
firewall-cmd --reload

# certbot certonly --standalone --tls-sni-01-port $HTTPS --domain $4
# cp /etc/letsencrypt/live/$4/fullchain.pem $1/artifacts/ssl.crt
# cp /etc/letsencrypt/live/$4/privkey.pem $1/artifacts/ssl.key

chown -R $3:$3 $1
