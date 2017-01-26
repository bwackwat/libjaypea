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

# rprint(requests.get("https://api.digitalocean.com/v2/droplets", headers=headers))
# rprint(requests.get("https://api.digitalocean.com/v2/actions", headers=headers))

newuser = raw_input("Enter a username for the deployment [admin]: ") or "admin"
newpass = ""
if(raw_input("Want to set password for " + newuser + "? [n]: ") == "y"):
	newpass = raw_input("Password: ")

newdir = raw_input("Enter a directory for the deployment (be absolute) [/opt/web]: ") or "/opt/web"
newhost = raw_input("Enter a hostname [bwackwat.com]: ") or "bwackwat.com"
newkey = raw_input("Enter a new 96 byte key for comd (enjoy writing a sentence) [pads random letters]: \n")

newkeylen = len(newkey)
if newkeylen < 96:
	newkey += ''.join(random.choice(string.ascii_letters) for x in range(96 - newkeylen))
	print "Appended " + str(96 - newkeylen) + " characters to the key."
with open(scriptdir + "/../keyfile.deploy", "w") as f:
	f.write(newkey)
print "Key is written to keyfile.deploy and will be used with cloud-init."

print '-----------------------------------------------------------------'

with open(scriptdir + "/deploy.sh") as f:
	print f.read()

print '-----------------------------------------------------------------'

if(raw_input("Deploy script good to go? [y]: ") or "y" != "y"):
	print "Exiting."
	sys.exit(0)

print '-----------------------------------------------------------------'

cloud_config = """
#cloud-config

runcmd:
 - useradd {2}
 - yum -y install git
 - mkdir -p {0}
 - git clone https://github.com/bwackwat/libjaypea {0}
 - chmod +x {0}/bin/deploy.sh
 - {0}/bin/deploy.sh {0} "{1}" {2} {3} {4}

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

images = json.loads(requests.get("https://api.digitalocean.com/v2/images?per_page=999&type=distribution", headers=headers).content)
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

#newid = newdroplet["droplet"]["id"]

print '-----------------------------------------------------------------'

#fips = json.loads(requests.get("https://api.digitalocean.com/v2/floating_ips?page=1&per_page=20", headers=headers).content)
#for fip in fips["floating_ips"]:
#	print "\nFloating IP:"
#	print fip["ip"]
#	print "Droplet: " + (fip["droplet"]["name"] if fip["droplet"] else "UNASSIGNED")

#print '-----------------------------------------------------------------'

#newfip = raw_input("If you want to assign the new droplet to a floating ip, enter the ip [n]: ") or "n";
#if(newfip != "n"):
#	rprint(requests.post("https://api.digitalocean.com/v2/floating_ips/" + newfip + " /actions", headers=headers, json={"type":"assign","droplet_id":newid}))

#print '-----------------------------------------------------------------'

print "Deployed! Check your DigitalOcean dashboard for the new droplet."
