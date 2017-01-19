#!/bin/python

import requests, json, random, string, sys

headers = {
	"Content-Type":"application/json",
	"Authorization":"Bearer " + raw_input("Enter your authorization token: ")
}

def rprint(response):
	print '-----------------------------------------------------------------'
	print response.status_code
	for key, value in response.headers.iteritems():
		print key + ": " + value
	response_json = json.loads(response.content)
	print json.dumps(response_json, indent=4)
	return response_json

# rprint(requests.get("https://api.digitalocean.com/v2/droplets", headers=headers))
# rprint(requests.get("https://api.digitalocean.com/v2/actions", headers=headers))

images = rprint(requests.get("https://api.digitalocean.com/v2/images?per_page=999&type=distribution", headers=headers))

print '-----------------------------------------------------------------'

for image in images["images"]:
	if image["distribution"] == "CentOS":
		print "\nCentOS Image"
		print "id: " + str(image["id"])
		print "name: " + image["name"]
		print "created_at: " + image["created_at"]

print '-----------------------------------------------------------------'

newport = raw_input("Enter a port for comd [40000]: ") or "40000"
newkey = raw_input("Enter a new 96 byte key for comd (enjoy writing a sentence): ")
newkeylen = len(newkey)
if newkeylen < 96:
	newkey += ''.join(random.choice(string.ascii_letters) for x in range(96 - newkeylen))
	print "Appended " + str(96 - newkeylen) + " characters to the key."
with open("keyfile.deploy", "w") as f:
	f.write(newkey)
	print "Key is written to /opt/keyfile.deploy and will be used with cloud-init."

print '-----------------------------------------------------------------'

with open("deploy.sh") as f:
	print f.read()

print '-----------------------------------------------------------------'

if(raw_input("\nDeploy script good to go (y?): ") != "y"):
	print "Exiting."
	sys.exit(0)

newdir = raw_input("Enter a directory for the deployment (be absolute) [/opt]: ") or "/opt"

cloud_config = """
#cloud-config

runcmd:
 - yum -y install git
 - mkdir -p {0}
 - git clone https://github.com/bwackwat/libjaypea {0}
 - chmod + x {0}/deploy.sh
 - {0}/deploy.sh {0} {1} "{2}"

power_state:
   mode: reboot
"""

cloud_config = cloud_config.format(newdir, newport, newkey)
print cloud_config

print '-----------------------------------------------------------------'

if(raw_input("\nCloud init good to go (y?): ") != "y"):
	print "Exiting."
	sys.exit(0)

rprint(requests.post("https://api.digitalocean.com/v2/droplets", headers=headers, json={
	"name":raw_input("Enter a name for the droplet: "),
	"region":"sfo1",
	"size":"512mb",
	"image":raw_input("Enter an image id from above (newest is recommended): "),
	"ssh_keys":None,
	"backups":False,
	"ipv6":False,
	"private_networking":False,
	"user_data":cloud_config
}))

print "DONE!"
