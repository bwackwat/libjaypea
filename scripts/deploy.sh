#!/bin/bash

if [ $# -lt 5 ]; then
	echo "Usage: deploy.sh <install directory> <username> <application name> <hostname> <db password>"
	exit
fi

cd $1

mkdir -p logs
mkdir -p artifacts
mkdir -p public-html/build

scripts/setup-centos7.sh > logs/setup-centos7.log 2>&1

cat <<EOF >> artifacts/start.sh
#!/bin/bash

python -u scripts/python/git-commit-checker.py bwackwat libjaypea > logs/git-commit-checker.log 2>&1 &

python -u scripts/python/watcher.py artifacts/libjaypea.master.latest.commit "scripts/extras/update-build.sh" > logs/master-commit-watcher.log 2>&1 &

python -u scripts/python/watcher.py binaries/$3 "binaries/$3 -pcs \"dbname=webservice user=$2 password=$5\" -p 10443 -pd ../affable-escapade" > logs/$3-watcher.log 2>&1 &

python -u scripts/python/watcher.py binaries/http-redirecter "binaries/http-redirecter --hostname $4 --port 10080" > logs/http-redirecter-watcher.log 2>&1 &

EOF

chmod +x artifacts/start.sh

cat <<EOF >> /etc/systemd/system/libjaypea.service
[Unit]
Description=libjaypea startup unit
After=postgresql.service

[Service]
User=bwackwat
Type=oneshot
WorkingDirectory=/opt/libjaypea
ExecStart=/opt/libjaypea/artifacts/start.sh

[Install]
WantedBy=default.target
EOF

systemctl enable libjaypea

echo -e "\n@reboot $2 $1/artifacts/start.sh > $1/logs/start.log 2>&1\n" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=10080
firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=10443
firewall-cmd --reload

# certbot certonly --standalone --tls-sni-01-port 10443 --domain build.bwackwat.com
# cp /etc/letsencrypt/live/build.bwackwat.com/fullchain.pem artifacts/ssl.crt
# cp /etc/letsencrypt/live/build.bwackwat.com/privkey.pem artifacts/ssl.key

chown -R $2:$2 $1
chown -R $2:$2 $1/../affable-escapade

scripts/pgsql-centos7.sh $5 > logs/pgsql-centos7.log 2>&1

