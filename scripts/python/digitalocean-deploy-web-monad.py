#!/bin/python

import requests, json, random, string, sys, os

scriptdir = os.path.dirname(os.path.realpath(__file__))

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

newuser = raw_input("Enter a username for the deployment: ")
if not newuser:
	print "Please provide a username."
	sys.exit(1)

newpass = ""
if(raw_input("Want to set password for " + newuser + "? [n]: ") == "y"):
	newpass = raw_input("Password: ")

newdir = raw_input("Enter a directory for the deployment (be absolute) [/opt/libjaypea]: ") or "/opt/libjaypea"
newhost = raw_input("Enter a hostname [bwackwat.com]: ") or "bwackwat.com"
newkey = raw_input("Enter a new 96 byte key for comd (enjoy writing a sentence) [pads random letters]: \n")

newkeylen = len(newkey)
if newkeylen < 96:
	newkey += ''.join(random.choice(string.ascii_letters) for x in range(96 - newkeylen))
	print "Appended " + str(96 - newkeylen) + " characters to the key."
with open(scriptdir + "/../../artifacts/" + newuser + ".deploy.keyfile", "w") as f:
	f.write(newkey)
print "Key is written to " + newuser + ".deploy.keyfile (in this directory) and will be used with cloud-init."

print '-----------------------------------------------------------------'

with open(scriptdir + "/../deploy.sh") as f:
	print f.read()

print '-----------------------------------------------------------------'

if(raw_input("Deploy script good to go? [y]: ") or "y" != "y"):
	print "Exiting."
	sys.exit(0)

print '-----------------------------------------------------------------'

cloud_config = """
#cloud-config

runcmd:
 - yum -y install git
 - mkdir -p {0}
 - git clone -b distribution-work https://github.com/bwackwat/libjaypea {0}
 - chmod +x {0}/scripts/deploy.sh
 - {0}/scripts/deploy.sh {0} "{1}" {2} {3} {4}

power_state:
   mode: reboot
"""

cloud_config = cloud_config.format(newdir, newkey, newuser, newhost, newpass)
print cloud_config

print '-----------------------------------------------------------------'

if(raw_input("Cloud init good to go? [y]: ") or "y" != "y"):
	print "Exiting."
	sys.exit(0)

print '-----------------------------------------------------------------'

images = rprint(requests.get("https://api.digitalocean.com/v2/images?per_page=999&type=distribution", headers=headers))
for image in images["images"]:
	if image["distribution"] == "CentOS":
		print "\nCentOS Image"
		print "id: " + str(image["id"])
		print "name: " + image["name"]
		print "created_at: " + image["created_at"]

print '-----------------------------------------------------------------'

newdroplet = rprint(requests.post("https://api.digitalocean.com/v2/droplets", headers=headers, json={
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

print '-----------------------------------------------------------------'

print "Deployed! Check your DigitalOcean dashboard for the new droplet."
