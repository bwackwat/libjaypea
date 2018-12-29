#!/bin/bash

if [ $# -lt 1 ]; then
	echo "Usage: $0 <domain>"
	exit
fi

certbot certonly --standalone --tls-sni-01-port 10443 --domain $1

cp /etc/letsencrypt/live/$1/fullchain.pem /opt/libjaypea/artifacts/

cp /etc/letsencrypt/live/$1/privkey.pem /opt/libjaypea/artifacts/

