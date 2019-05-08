#!/bin/bash

sleep 120

while true
do
	STATUS=$(curl -m 5 -s -o /dev/null -w '%{http_code}' $1 --insecure)
	if [ ! $STATUS -eq 200 ]; then
		echo "$(date) Restarting site."
		touch $2
	else
		echo "$(date) Site is up."
	fi
	sleep 120
done

