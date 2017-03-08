#!/bin/bash

if [ $# -lt 2 ]; then
	echo "Usage: deploy.sh <install directory> <username> <optional password>"
	exit
fi

cd $1

mkdir -p logs
mkdir -p artifacts
mkdir -p public-html/build

chmod +x scripts/setup-centos7.sh
scripts/setup-centos7.sh > logs/setup-centos7.log 2>&1

chmod +x scripts/build-library.sh
chmod +x scripts/build-example.sh

touch artifacts/host-services.json

cat <<EOF >> artifacts/start.sh
#!/bin/bash

cd $1

scripts/python/git-commit-checker.py > logs/git-commit-checker.log 2>&1 &

scripts/python/watcher.py artifacts/master.latest.commit "scripts/update-build.sh > logs/update-build.log 2>&1" > logs/master-commit-watcher.log 2>&1 &

scripts/python/watcher.py binaries/libjaypea,artifacts/host-services.json "binaries/libjaypea-api > logs/libjaypea-api.log 2>&1" > logs/libjaypea-api-watcher.log 2>&1 &

EOF

chmod +x artifacts/start.sh

echo -e "\n@reboot $2 $1/artifacts/start.sh\n" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=10080
firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=10443
firewall-cmd --reload

# certbot certonly --standalone --tls-sni-01-port 10443 --domain build.bwackwat.com
# cp /etc/letsencrypt/live/build.bwackwat.com/fullchain.pem artifacts/ssl.crt
# cp /etc/letsencrypt/live/build.bwackwat.com/privkey.pem artifacts/ssl.key

useradd $2
if ! [ -z $3 ]; then
	echo "$3" | passwd "$2" --stdin
fi
chown -R $2:$2 $1
