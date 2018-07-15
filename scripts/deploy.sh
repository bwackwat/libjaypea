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

killall python
killall /opt/libjaypea/binaries/jph2
killall /opt/libjaypea/binaries/http-redirecter

# Watch for changes to affable-escapade, and update.
python -u scripts/python/git-commit-checker.py bwackwat affable-escapade > logs/libjaypea-git-commit-checker.log 2>&1 &

python -u scripts/python/watcher.py artifacts/affable-escapade.master.latest.commit "scripts/extras/update-branch-from-git.sh /opt/affable-escapade master" > logs/affable-escapade-master-commit-watcher.log 2>&1 &

# Watch for changes to libjaypea, and update.
python -u scripts/python/git-commit-checker.py bwackwat libjaypea > logs/libjaypea-git-commit-checker.log 2>&1 &

python -u scripts/python/watcher.py artifacts/libjaypea.master.latest.commit "scripts/extras/update-branch-from-git.sh $1 master && scripts/build-library.sh PROD && scripts/build-example.sh PROD" > logs/libjaypea-master-commit-watcher.log 2>&1 &

python -u scripts/python/watcher.py binaries/$3 "binaries/$3 -pcs \"dbname=webservice user=$2 password=$5\" -p 10443 -pd ../affable-escapade" > logs/$3-watcher.log 2>&1 &

python -u scripts/python/watcher.py binaries/http-redirecter "binaries/http-redirecter --hostname $4 --port 10080" > logs/http-redirecter-watcher.log 2>&1 &

EOF

chmod +x artifacts/start.sh

touch $1/logs/start.log

cat <<EOF >> /etc/systemd/system/libjaypea.service
[Unit]
Description=libjaypea startup unit
After=postgresql.service

[Service]
User=bwackwat
Type=oneshot
KillMode=process
WorkingDirectory=/opt/libjaypea
ExecStart=/bin/sh -c '/opt/libjaypea/artifacts/start.sh 2>&1 > $1/logs/start.log'

[Install]
WantedBy=default.target
EOF

systemctl enable libjaypea

firewall-offline-cmd --zone=public -add-masquerade
firewall-offline-cmd --zone=public --add-forward-port=port=80:proto=tcp:toport=10080
firewall-offline-cmd --zone=public --add-forward-port=port=443:proto=tcp:toport=10443

# Run BEFORE http redirecter.
# certbot certonly --standalone --http-01-port 10080 --domain jph2.net -n

# certbot certonly --standalone --tls-sni-01-port 10443 --domain build.bwackwat.com
# cp /etc/letsencrypt/live/build.bwackwat.com/fullchain.pem artifacts/ssl.crt
# cp /etc/letsencrypt/live/build.bwackwat.com/privkey.pem artifacts/ssl.key

chown -R $2:$2 $1
chown -R $2:$2 $1/../affable-escapade

scripts/extras/pgsql-centos7.sh $5 > logs/pgsql-centos7.log 2>&1

