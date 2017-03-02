#!/bin/bash

yum makecache fast
yum -y update
yum -y install fail2ban certbot

systemctl enable fail2ban
systemctl start fail2ban

# certbot certonly --standalone --tls-sni-01-port 10443 --domain test.bwackwat.com
# cp /etc/letsencrypt/live/$4/fullchain.pem $1/artifacts/ssl.crt
# cp /etc/letsencrypt/live/$4/privkey.pem $1/artifacts/ssl.key
