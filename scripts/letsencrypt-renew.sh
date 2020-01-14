#!/bin/bash

if [ $# -lt 4 ]; then
	echo "Usage: $0 </opt/libjaypea> <domain> <port> <key owner username/group>"
	exit
fi

set -eux

systemctl stop libjaypea

certbot certonly --standalone --http-01-port $3 --domain $2

cp /etc/letsencrypt/live/$2/fullchain.pem $1/artifacts/ssl.crt
cp /etc/letsencrypt/live/$2/privkey.pem $1/artifacts/ssl.key

chown $4.$4 $1/artifacts/ssl.crt
chown $4.$4 $1/artifacts/ssl.key

chmod 644 $1/artifacts/ssl.crt
chmod 644 $1/artifacts/ssl.key

systemctl start libjaypea

