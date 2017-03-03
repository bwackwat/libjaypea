#!/bin/bash

yum makecache fast
yum -y upgrade

yum -y install postgresql-server postgresql-contrib postgresql-libs postgis

export PGDATA=/data
if [ ! -d "/data" ]; then
        mkdir -p /data
        postgresql-setup initdb
fi

systemctl start postgresql
systemctl enable postgresql

cp ./scripts/extras/tables.sql /tables.sql
chmod 666 /tables.sql
chown postgres:postgres /tables.sql

echo "abc123" | passwd postgres --stdin
su postgres

psql -U postgres -c "CREATE DATABASE webservice OWNER postgres;"
psql -U postgres -d webservice -a -f /tables.sql

exit
rm /tables.sql

#Only change is 
#host    all             all             127.0.0.1/32            ident
#To
#host    all             all             127.0.0.1/32            trust
#mv -n /var/lib/pgsql/data/pg_hba.conf /var/lib/pgsql/data/pg_hba.conf.orig
#cp -f ./bin/pg_hba.conf /var/lib/pgsql/data/pg_hba.conf
