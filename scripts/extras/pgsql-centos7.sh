#!/bin/bash

# Used for development locally.

# yum makecache fast
# yum -y upgrade

if [ $# -lt 1 ]; then
	echo "Usage: pgsql-centos7.sh <password>"
	exit
fi

yum -y install postgresql-server postgresql-contrib postgresql-libs postgis

export PGDATA=/data
if [ ! -d "/data" ]; then
	mkdir -p /data
	postgresql-setup initdb
fi

systemctl start postgresql
systemctl enable postgresql

dir=$(dirname $BASH_SOURCE)

cp $dir/pg_hba.conf /var/lib/pgsql/data/pg_hba.conf
chown postgres:postgres /var/lib/pgsql/data/pg_hba.conf
chmod 600 /var/lib/pgsql/data/pg_hba.conf
systemctl restart postgresql

cp $dir/tables.sql /tables.sql
chmod 666 /tables.sql
chown postgres:postgres /tables.sql

psql -U postgres -c "CREATE DATABASE webservice OWNER postgres;"
psql -U postgres -d webservice -a -f /tables.sql
psql -U postgres -d webservice -c "ALTER ROLE bwackwat PASSWORD '$1';"

rm /tables.sql

