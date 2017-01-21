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

chmod 666 ./bin/tables.sql
chown postgres:postgres ./bin/tables.sql

psql -U postgres -c "CREATE DATABASE webservice OWNER postgres;"
psql -U postgres -d webservice -a -f ./bin/tables.sql
