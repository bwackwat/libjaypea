#!/bin/bash

if [ $# -lt 2 ]; then
	echo "Usage: <install directory> <username>"
	exit
fi

yum -y install epel-release
yum -y install libpqxx cryptopp
yum -y install firewalld fail2ban

mkdir -p $1/artifacts/
mkdir -p $1/public-html/

git clone https://github.com/phc-hash-winner/argon2 $1/argon2
cd $1/argon2
make
make install

wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/extras/self-signed-ssl/ssl.crt -O $1/ssl.crt
wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/extras/self-signed-ssl/ssl.key -O $1/ssl.key

echo "<h1>Hello, World!</h1>" >> $1/public-html/index.html

cat <<EOF >> $1/libjaypea-api.configuration.json
{
	"ssl_cerificate":"ssl.crt",
	"ssl_private_key":"ssl.key",
	"port":"10443",
	"public_directory":"$1/public-html"
}
EOF

cat <<EOF >> $1/start.sh
#!/bin/bash

cd $1

wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/scripts/python/build-server-checker.py

python build-server-checker.py > build-server-checker.log 2>&1 &

wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/scripts/python/watcher.py

python watcher.py master.latest.commit "wget -N https://build.bwackwat.com/build/libjaypeap.so -O $1/artifacts/libjaypeap.so && wget -N https://build.bwackwat.com/build/libjaypea-api && libjaypea-api --configuration-file libjaypea-api.configuration.json" > libjaypea-api-watcher.log 2>&1 &

EOF

chmod +x $1/start.sh

echo -e "\n@reboot deploy $1/start.sh\n" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=10080
firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=10443
firewall-cmd --reload

# certbot certonly --standalone --tls-sni-01-port 10443 --domain build.bwackwat.com
# cp /etc/letsencrypt/live/build.bwackwat.com/fullchain.pem $dist/artifacts/ssl.crt
# cp /etc/letsencrypt/live/build.bwackwat.com/privkey.pem $dist/artifacts/ssl.key

useradd $2
chown -R $2.$2 $1
