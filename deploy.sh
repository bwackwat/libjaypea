#!/bin/bash

# This is pretty epic.
# echo "abc123" | passwd root --stdin

HTTP=10080
HTTPS=10443

cd $1

chmod +x $1/setup-centos7.sh
$1/setup-centos7.sh > $1/setup-centos7.log 2>&1

chmod +x $1/build.sh
$1/build.sh > $1/build.log 2>&1

echo "$3" >> $1/keyfile.deploy

cat <<EOF >> $1/start.sh
#!/bin/bash

$1/bin/comd \
--port $2 \
--keyfile $1/keyfile.deploy \
> $1/comd.log 2>&1 &

$1/bin/modern-web-monad \
--public_directory $1/html \
--ssl_certificate $1/etc/ssl/ssl.crt \
--ssl_private_key $1/etc/ssl/ssl.key \
--http $HTTP \
--https $HTTPS \
> $1/modern-web-monad.log 2>&1 &
EOF

echo -e "\n@reboot $4 $1/start.sh" >> /etc/crontab

firewall-cmd --zone=public --permanent --add-masquerade
firewall-cmd --zone=public --permanent --add-forward-port=port=80:proto=tcp:toport=$HTTP
firewall-cmd --zone=public --permanent --add-forward-port=port=443:proto=tcp:toport=$HTTPS
firewall-cmd --zone=public --permanent --add-port=$2/tcp
firewall-cmd --reload

chown -R $4:$4 $1
