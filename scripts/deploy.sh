#!/bin/bash

if [ $# -lt 3 ]; then
	echo "Usage: <install directory> <distribution keyfile> <username> <optional password>"
	exit
fi

useradd $3
if ! [ -z $4 ]; then
	echo "$4" | passwd "$3" --stdin
fi

cd $1

mkdir -p $1/logs
mkdir -p $1/artifacts

chmod +x $1/scripts/setup-centos7.sh
$1/scripts/setup-centos7.sh 2>&1 | tee $1/logs/setup-centos7.log

chmod +x $1/scripts/build-library.sh
$1/scripts/build-library.sh PROD 2>&1 | tee $1/logs/build-library.log

chmod +x $1/scripts/build-example.sh

echo "$2" >> $1/artifacts/deploy.keyfile

touch $1/artifacts/start-services.sh
chmod +x $1/artifacts/start-services.sh
touch $1/artifacts/setup-firewall.sh
chmod +x $1/artifacts/setup-firewall.sh

cat <<EOF >> $1/artifacts/start.sh 
#!/bin/bash

$1/scripts/build-example.sh distributed-server PROD > $1/logs/build-distributed-server.log 2>&1

$1/binaries/distributed-server \
--port 10000 \
--keyfile $1/artifacts/deploy.keyfile \
> $1/logs/distributed-server.log 2>&1 &

$1/artifacts/start-services.sh

EOF

chmod +x $1/artifacts/start.sh

echo -e "\n@reboot root $1/artifacts/setup-firewall.sh" >> /etc/crontab
echo -e "@reboot $3 $1/artifacts/start.sh\n" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-port=10000/tcp
firewall-cmd --permanent --new-service-from-file=$1/artifacts/libjaypea-firewalld-service.xml
firewall-cmd --reload

# certbot certonly --standalone --tls-sni-01-port $HTTPS --domain $4
# cp /etc/letsencrypt/live/$4/fullchain.pem $1/artifacts/ssl.crt
# cp /etc/letsencrypt/live/$4/privkey.pem $1/artifacts/ssl.key

chown -R $3:$3 $1
