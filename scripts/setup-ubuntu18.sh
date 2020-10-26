#!/bin/bash

if [ "$(whoami)" != "root" ]; then
	echo "This must be run as root or using sudo."
	exit 1
fi

if [ $# -lt 1 ]; then
	echo "Usage: setup-ubuntu18.sh <postgresql password>"
	exit
fi

set -eux

cd $(dirname "${BASH_SOURCE[0]}")/../

apt-get install libcrypto++-dev libpqxx-dev libssl1.1 libssl1.0.0 clang libargon2.0-dev

apt-get install postgresql postgis

apt-get install python3-pip

pip3 install psutil

dir=$(dirname $BASH_SOURCE)

cp $dir/extras/pg_hba.conf /etc/postgresql/10/main/pg_hba.conf
chown postgres:postgres /etc/postgresql/10/main/pg_hba.conf
chmod 600 /etc/postgresql/10/main/pg_hba.conf
systemctl restart postgresql

cp $dir/extras/tables.sql /tables.sql
chmod 666 /tables.sql
chown postgres:postgres /tables.sql

psql -U postgres -c "DROP DATABASE IF EXISTS webservice;"
psql -U postgres -c "CREATE DATABASE webservice OWNER postgres;"
psql -U postgres -d webservice -a -f /tables.sql
psql -U postgres -d webservice -c "ALTER ROLE bwackwat PASSWORD '$1';"

rm /tables.sql
