#!/bin/bash

set -e

if [ -z $1 ]; then
	echo "Please provide a directory to 'build' a site."
	exit
fi

echo "Using $1"

start=$(pwd)
echo "Started in $start"

cd $(dirname "${BASH_SOURCE[0]}")/
scriptdir=$(pwd)
echo "Script in $scriptdir"

cd $start
cd $1
sitedir=$(pwd)
echo "Site in $sitedir"

cd $scriptdir

python ../python/site-templater.py $sitedir/
python ../python/site-indexer.py $sitedir/
