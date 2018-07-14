#!/bin/bash

cd $1

git fetch
git pull origin $2
