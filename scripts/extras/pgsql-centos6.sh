#!/bin/bash

# Used for deployment.

yum makecache fast
yum -y upgrade

yum -y install postgresql-server postgresql-contrib postgresql-libs postgis vim

service postgresql initdb
service postgresql restart
chkconfig postgresql on

echo $1 | passwd postgres --stdin

cp ./scripts/extras/tables.sql /tables.sql
chmod 666 /tables.sql
chown postgres:postgres /tables.sql

psql -U postgres -c "CREATE DATABASE webservice OWNER postgres;"
psql -U postgres -d webservice -a -f /tables.sql

rm /tables.sql

mv /var/lib/pgsql/data/pg_hba.conf /var/lib/pgsql/data/pg_hba.conf.orig
cp ./scripts/extras/pg_hba.conf /var/lib/pgsql/data/pg_hba.conf
chown postgres:postgres /var/lib/pgsql/data/pg_hba.conf
chmod 600 /var/lib/pgsql/data/pg_hba.conf
service postgresql restart
