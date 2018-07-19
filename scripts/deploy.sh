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

killall -q python
killall -q binaries/jph2
killall -q binaries/http-redirecter

# Watch for changes to affable-escapade, and update.
python -u scripts/python/git-commit-checker.py bwackwat affable-escapade > logs/affable-escapade-git-commit-checker.log 2>&1 &

python -u scripts/python/watcher.py artifacts/affable-escapade.master.latest.commit "cd /opt/affable-escapade && git reset --hard HEAD && git clean -f && cd /opt/libjaypea && scripts/extras/update-branch-from-git.sh /opt/affable-escapade master && cd /opt/affable-escapade && git lfs pull && cd /opt/libjaypea && scripts/python/site-indexer.py ../affable-escapade y && scripts/python/site-templater.py ../affable-escapade" > logs/affable-escapade-master-commit-watcher.log 2>&1 &

# Watch for changes to libjaypea, and update.
python -u scripts/python/git-commit-checker.py bwackwat libjaypea > logs/libjaypea-git-commit-checker.log 2>&1 &

python -u scripts/python/watcher.py artifacts/libjaypea.master.latest.commit "scripts/extras/update-branch-from-git.sh $1 master && scripts/build-library.sh PROD && scripts/build-example.sh PROD" > logs/libjaypea-master-commit-watcher.log 2>&1 &

# Start three servers.

python -u scripts/python/watcher.py binaries/http-redirecter "binaries/http-redirecter --hostname $4 --port 10080" forever > logs/http-redirecter-watcher.log 2>&1 &

python -u scripts/python/watcher.py binaries/$3 "binaries/$3 -key artifacts/ssl.key -crt artifacts/ssl.crt -pcs \"dbname=webservice user=$2 password=$5\"" forever > logs/$3-watcher.log 2>&1 &

python -u scripts/python/watcher.py binaries/chat "binaries/chat --port 11000" forever > logs/chat-watcher.log 2>&1 &

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
RemainAfterExit=true
ExecStart=/bin/sh -c '/opt/libjaypea/artifacts/start.sh 2>&1 > $1/logs/start.log'
ExecStop=/bin/sh -c 'killall -q python && killall -q binaries/http-redirecter && -q killall -q binaries/$3'

[Install]
WantedBy=default.target
EOF

systemctl enable libjaypea

firewall-offline-cmd --zone=public -add-masquerade
firewall-offline-cmd --zone=public --add-forward-port=port=80:proto=tcp:toport=10080
firewall-offline-cmd --zone=public --add-forward-port=port=443:proto=tcp:toport=10443
firewall-offline-cmd --zone=public --add-forward-port=port=8000:proto=tcp:toport=11000

# Must stop http-redirecter.
# certbot certonly --standalone --http-01-port 10080 --domain $4 -n --agree-tos --email $6
# cp /etc/letsencrypt/live/$4/fullchain.pem artifacts/ssl.crt
# cp /etc/letsencrypt/live/$4/privkey.pem artifacts/ssl.key
cp extras/self-signed-ssl/ssl.crt artifacts/
cp extras/self-signed-ssl/ssl.key artifacts/

scripts/extras/pgsql-centos7.sh $5 > logs/pgsql-centos7.log 2>&1

chown -R $2:$2 $1
chown -R $2:$2 $1/../affable-escapade

