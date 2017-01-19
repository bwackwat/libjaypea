#!/bin/python

import requests, json, random, string, sys

headers = {
	"Content-Type":"application/json",
	"Authorization":"Bearer " + raw_input("Enter your authorization token: ")
}

def rprint(response):
	print response.status_code
	for key, value in response.headers.iteritems():
		print key + ": " + value
	response_json = json.loads(response.content)
	print json.dumps(response_json, indent=4)
	return response_json

# rprint(requests.get("https://api.digitalocean.com/v2/droplets", headers=headers))

images = rprint(requests.get("https://api.digitalocean.com/v2/images?per_page=999&type=distribution", headers=headers))

print '----------------------------------------------'

for image in images["images"]:
	if image["distribution"] == "CentOS":
		print "\nCentOS Image"
		print "id: " + str(image["id"])
		print "name: " + image["name"]
		print "created_at: " + image["created_at"]

print '----------------------------------------------'

newport = raw_input("Enter a port for comd (40000): ") or "40000"
newkey = raw_input("Enter a new 96 byte key for comd: ")
newkeylen = len(newkey)
if newkeylen < 96:
	newkey += ''.join(random.choice(string.ascii_letters) for x in range(96 - newkeylen))
	print "Appended " + str(96 - newkeylen) + " characters to the key."
with open("deploy-keyfile", "w") as f:
	f.write(newkey)
	print "Key is written to ./deploy-keyfile and will be used with cloud-init."

cloud_config = """
#cloud-config

runcmd:
 - yum -y install git
 - mkdir -p /opt/libjaypea
 - git clone https://github.ocm/bwackwat/libjaypea /opt/libjaypea
 - chmod +x /opt/libjaypea/setup-centos7.sh
 - /opt/libjaypea/bin/setup-centos7.sh > /opt/libjaypea/setup-centos7.log 2>&1
 - chmod +x /opt/libjaypea/build.sh
 - /opt/libjaypea/bin/build.sh > /opt/libjaypea/build.log 2>&1
 - echo "%s" >> /opt/libjaypea/deploy-keyfile
 - echo "/opt/libjaypea/comd -p %s -k /opt/libjaypea/deploy-keyfile > /opt/libjaypea/comd.log 2>&1" >> /etc/rc.d/rc.local
 - echo "/opt/libjaypea/modern-web-monad > /opt/libjaypea/modern-web-monad.log 2>&1" >> /etc/rc.d/rc.local
 - chmod +x /etc/rc.d/rc.local
 - firewall-cmd --zone=public --add-port=80/tcp --permanent
 - firewall-cmd --zone=public --add-port=443/tcp --permanent
 - firewall-cmd --zone=public --add-port=%s/tcp --permanent

power_state:
 mode: reboot
"""
cloud_config = cloud_config % (newkey, newport, newport)
print cloud_config

if(raw_input("\nCloud init good to go? (y): ") != "y"):
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
