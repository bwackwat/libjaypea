#!/bin/bash

yum -y install epel-release
yum -y install libpqxx cryptopp
# yum -y install libssl firewalld fail2ban

dist="/opt/libjaypea"
user="deploy"
mkdir -p $dist/artifacts/
useradd $user

git clone https://github.com/phc-hash-winner/argon2 $dist/argon2
cd $dist/argon2
make
make install

cat <<EOF >> $dist/start.sh
#!/bin/bash

cd $dist

wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/scripts/python/build-server-checker.py

python build-server-checker.py > build-server-checker.log 2>&1 &``

wget -N https://raw.githubusercontent.com/bwackwat/libjaypea/master/scripts/python/watcher.py

python watcher.py master.latest.commit "wget -N https://test.bwackwat.com/build/libjaypeap.so && wget -N https://test.bwackwat.com/build/libjaypea-api && libjaypea-api --configuration-file libjaypea-api.configuration.json" > libjaypea-api-watcher.log 2>&1 &

EOF

chmod +x $dist/start.sh

echo -e "\n@reboot deploy $dist/start.sh\n" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=10080
firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=10443
firewall-cmd --zone=public --permanent --add-port=10000/tcp
firewall-cmd --reload

# certbot certonly --standalone --tls-sni-01-port 10443 --domain test.bwackwat.com
# cp /etc/letsencrypt/live/test.bwackwat.com/fullchain.pem artifacts/ssl.crt
# cp /etc/letsencrypt/live/test.bwackwat.com/privkey.pem artifacts/ssl.key

chown -R deploy.deploy $dist
