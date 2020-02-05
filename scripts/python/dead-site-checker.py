#!/usr/bin/python3

import requests, sys

if len(sys.argv) < 2:
	print("Usage: dead-site-checker.py <hostname>")
	sys.exit(1)

url = "https://" + sys.argv[1]

while True:
	response = None
	restart = False
	try:
		response = requests.get(url, timeout=5)
		if response.status != 200:
			restart = True
	except:
		restart = True

	print(restart)
