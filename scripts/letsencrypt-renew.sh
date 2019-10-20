#!/bin/bash

if [ $# -lt 3 ]; then
	echo "Usage: $0 </opt/libjaypea> <domain> <port>"
	exit
fi

systemctl stop libjaypea

certbot certonly --standalone --http-01-port $3 --domain $2

systemctl start libjaypea

cp /etc/letsencrypt/live/$2/fullchain.pem $1/artifacts/

cp /etc/letsencrypt/live/$2/privkey.pem $1/artifacts/
