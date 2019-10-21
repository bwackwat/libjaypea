#!/bin/python

import requests, json, getpass, os, sys, datetime, urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

if len(sys.argv) > 1 and sys.argv[1] == "reset":
	print "Resetting."
	if os.path.exists("artifacts/backup.json"):
		os.remove("artifacts/backup.json")
	exit()

config = {};

if os.path.exists("artifacts/backup.json"):
	with open("artifacts/backup.json", "r") as f:
		config = json.loads(f.read())

if "url" not in config:
	config["url"] = raw_input("Enter a url for JPH2: ")
if "username" not in config:
	config["username"] = raw_input("Enter your username for JPH2: ")
if "password" not in config:
	config["password"] = getpass.getpass("Enter your password for JPH2: ")

with open("artifacts/backup.json", "w") as f:
	f.write(json.dumps(config))

if not os.path.exists("artifacts/backups"):
	os.makedirs("artifacts/backups")

backup_verify = True
if "localhost" in config["url"]:
	backup_verify = False

count = 1
filename = "backup-" + datetime.datetime.now().strftime("%b%d%Y") + "-"
while os.path.exists("artifacts/backups/" + filename + str(count) + ".sql"):
	count += 1
filename = "artifacts/backups/" + filename + str(count) + ".sql"

print "Saving backup SQL..."

with open(filename, "w") as f:
	token_request = requests.post(config["url"] + "api/login", json={"username": config["username"], "password": config["password"]}, verify=backup_verify).json()
	try:
		token = token_request["token"]
	except KeyError as e:
		print token_request
		exit()
	data = requests.post(config["url"] + "api/backup", json={"token": token}, verify=backup_verify).content
	f.write(data)


