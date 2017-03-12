#!/bin/bash

wget -N https://build.bwackwat.com/build/libjaypeap.so -O artifacts/libjaypeap.so

wget -N https://build.bwackwat.com/api/host-service?token=abc123'&'host=dev.bwackwat.com'&'service=libjaypea-api -O libjaypea-api.configuration.json
rm libjaypea-api
wget -N https://build.bwackwat.com/build/libjaypea-api

wget -N https://build.bwackwat.com/api/host-service?token=abc123'&'host=dev.bwackwat.com'&'service=http-redirecter -O http-redirecter.configuration.json
rm http-redirecter
wget -N https://build.bwackwat.com/build/http-redirecter

touch ready.lock
