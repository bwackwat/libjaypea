#!/bin/bash

if [ $# -lt 3 ]; then
	echo "Usage: <install directory> <distribution keyfile> <username> <optional password>"
	exit
fi

useradd $3
if ! [ -z $4 ]; then
	echo "$4" | passwd "$3" --stdin
fi

chmod +x $1/scripts/setup-centos7.sh
$1/scripts/setup-centos7.sh 2>&1 | tee $1/logs/setup-centos7.log

chmod +x $1/scripts/build-library.sh
chmod +x $1/scripts/build-example.sh

echo "$2" >> $1/artifacts/deploy.keyfile

touch $1/artifacts/start-services.sh
chmod +x $1/artifacts/start-services.sh

cat <<EOF >> $1/artifacts/start.sh
#!/bin/bash

cd $1

git fetch
git pull origin master

scripts/build-library.sh PROD > logs/build-library.log 2>&1
scripts/build-example.sh PROD > logs/build-example.log 2>&1

binaries/distributed-server \
--port 10000 \
--keyfile artifacts/deploy.keyfile \
> logs/distributed-server.log 2>&1 &

artifacts/start-services.sh

EOF

chmod +x $1/artifacts/start.sh

echo -e "\n@reboot $3 $1/artifacts/start.sh\n" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=10080
firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=10443
firewall-cmd --zone=public --permanent --add-port=10000/tcp
firewall-cmd --reload

# certbot certonly --standalone --tls-sni-01-port 10443 --domain test.bwackwat.com
# cp /etc/letsencrypt/live/test.bwackwat.com/fullchain.pem artifacts/ssl.crt
# cp /etc/letsencrypt/live/test.bwackwat.com/privkey.pem artifacts/ssl.key

chown -R $3:$3 $1
