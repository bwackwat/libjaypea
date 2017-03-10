#!/bin/bash

if [ $# -lt 2 ]; then
	echo "Usage: <install directory> <username>"
	exit
fi

yum -y install epel-release git
yum -y install libpqxx cryptopp certbot
yum -y install firewalld fail2ban

systemctl enable fail2ban
systemctl restart fail2ban
systemctl enable firewalld
systemctl restart firewalld

cd $1

git clone https://github.com/phc-hash-winner/argon2 argon2
cd argon2
make
make install

cd $1

mkdir -p artifacts/
mkdir -p public-html/

wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/extras/self-signed-ssl/ssl.crt -O ssl.crt
wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/extras/self-signed-ssl/ssl.key -O ssl.key

wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/scripts/python/build-server-checker.py
chmod +x build-server-checker.py

wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/scripts/python/watcher.py
chmod +x watcher.py

echo "<h1>Hello, World!</h1>" >> public-html/index.html

touch libjaypea-api.configuration.json
touch http-redirecter.configuration.json
touch master.latest.commit

cat <<EOF >> start.sh
#!/bin/bash

cd $1

python -u watcher.py master.latest.commit "wget -N https://build.bwackwat.com/build/libjaypeap.so -O artifacts/libjaypeap.so && wget -N https://build.bwackwat.com/api/host-service?token=abc123%26host=dev.bwackwat.com%26service=libjaypea-api -O libjaypea-api.configuration.json && wget -N https://build.bwackwat.com/build/libjaypea-api && chmod +x libjaypea-api && wget -N https://build.bwackwat.com/api/host-service?token=abc123%26host=dev.bwackwat.com%26service=http-redirecter -O http-redirecter.configuration.json && wget -N https://build.bwackwat.com/build/http-redirecter && chmod +x http-redirecter" > master-commit-watcher.log 2>&1 &

python -u watcher.py libjaypea-api "./libjaypea-api --configuration_file libjaypea-api.configuration.json" > libjaypea-api-watcher.log 2>&1 &
	
python -u watcher.py http-redirecter "./http-redirecter --configuration_file http-redirecter.configuration.json" > http-redirecter-watcher.log 2>&1 &

python -u build-server-checker.py master > build-server-checker.log 2>&1 &

EOF

chmod +x start.sh

echo -e "\n@reboot $2 $1/start.sh > $1/start.log 2>&1\n" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=10080
firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=10443
firewall-cmd --reload

# certbot certonly --standalone --tls-sni-01-port 10443 --domain dev.bwackwat.com
# cp /etc/letsencrypt/live/dev.bwackwat.com/fullchain.pem ssl.crt
# cp /etc/letsencrypt/live/dev.bwackwat.com/privkey.pem ssl.key

useradd $2
chown -R $2.$2 $1
