#!/bin/bash

if [ $# -lt 8 ]; then
	echo "Usage: deploy.sh <install directory> <username> <application name> <hostname> <db password> <http port> <https port> <tanks port>"
	exit
fi

cd $1

scripts/setup-centos7.sh > logs/setup-centos7.log 2>&1

rm artifacts/start.sh
cat <<EOF >> artifacts/start.sh
#!/bin/bash

killall -q python
killall -q $1/artifacts/$3

# Watch for changes to affable-escapade, and update.
python -u scripts/python/git-commit-checker.py bwackwat affable-escapade > logs/affable-escapade-git-commit-checker.log 2>&1 &

python -u scripts/python/watcher.py artifacts/affable-escapade.master.latest.commit "cd /opt/affable-escapade && git reset --hard HEAD && git clean -xdnf && cd $1 && scripts/extras/update-branch-from-git.sh /opt/affable-escapade master && cd /opt/affable-escapade && git lfs pull && cd $1 && scripts/python/site-indexer.py ../affable-escapade y && scripts/python/site-templater.py ../affable-escapade" > logs/affable-escapade-master-commit-watcher.log 2>&1 &

# Watch for changes to libjaypea, and update.
python -u scripts/python/git-commit-checker.py bwackwat libjaypea > logs/libjaypea-git-commit-checker.log 2>&1 &

python -u scripts/python/watcher.py artifacts/libjaypea.master.latest.commit "scripts/extras/update-build.sh" > logs/libjaypea-master-commit-watcher.log 2>&1 &

# Start the application server.

python -u scripts/python/watcher.py artifacts/$3 "artifacts/$3" forever > logs/$3-watcher.log 2>&1 &

./scripts/dead-site-checker.sh http://localhost:$6 artifacts/jph2 > logs/dead-site-checker.log 2>&1 &

EOF

chmod +x artifacts/start.sh

touch $1/logs/start.log

rm /etc/systemd/system/libjaypea.service
cat <<EOF >> /etc/systemd/system/libjaypea.service
[Unit]
Description=libjaypea startup unit
After=postgresql.service

[Service]
User=$2
Type=oneshot
KillMode=process
WorkingDirectory=$1
RemainAfterExit=true
ExecStart=/bin/sh -c '$1/artifacts/start.sh 2>&1 > $1/logs/start.log'
ExecStop=/bin/sh -c 'killall -q dead-site-checker.sh && killall -q python && killall -q $1/artifacts/$3'

[Install]
WantedBy=default.target
EOF

systemctl enable libjaypea

firewall-offline-cmd --zone=public --add-masquerade
firewall-offline-cmd --zone=public --add-forward-port=port=80:proto=tcp:toport=$6
firewall-offline-cmd --zone=public --add-forward-port=port=443:proto=tcp:toport=$7
firewall-offline-cmd --zone=public --add-forward-port=port=8000:proto=tcp:toport=$8

#firewall-cmd --permanent --zone=public --add-masquerade
#firewall-cmd --permanent --zone=public --add-forward-port=port=80:proto=tcp:toport=20080
#firewall-cmd --permanent --zone=public --add-forward-port=port=443:proto=tcp:toport=20443
#firewall-cmd --permanent --zone=public --add-forward-port=port=8000:proto=tcp:toport=28000
#firewall-cmd --set-default-zone=public
#firewall-cmd --reload

# Must stop http-redirecter.
# certbot certonly --standalone --http-01-port 10080 --domain $4 -n --agree-tos --email $6
# cp /etc/letsencrypt/live/$4/fullchain.pem artifacts/ssl.crt
# cp /etc/letsencrypt/live/$4/privkey.pem artifacts/ssl.key
cp extras/self-signed-ssl/ssl.crt artifacts/
cp extras/self-signed-ssl/ssl.key artifacts/

scripts/extras/pgsql-centos7.sh $5 > logs/pgsql-centos7.log 2>&1

chown -R $2:$2 $1
chown -R $2:$2 $1/../affable-escapade

