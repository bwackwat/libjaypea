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

newname = raw_input("Enter a name for the droplet [" + newuser + "]: ") or newuser

cloud_config = """
#cloud-config

runcmd:
 - yum -y install git
 - mkdir -p /opt/libjaypea/logs
 - git clone https://github.com/bwackwat/libjaypea /opt/libjaypea
 - chmod +x /opt/libjaypea/scripts/deploy.sh
 - /opt/libjaypea/scripts/deploy.sh /opt/libjaypea {0} > /opt/libjaypea/logs/deploy.log 2>&1
"""

minimal_config = """
#cloud-config

runcmd:
 - yum -y install wget
 - mkdir -p /opt/libjaypea/logs
 - cd /opt/libjaypea
 - wget https://raw.githubusercontent.com/bwackwat/libjaypea/master/scripts/deploy-minimal.sh
 - chmod +x deploy-minimal.sh
 - /opt/libjaypea/deploy-minimal.sh /opt/libjaypea {0} > /opt/libjaypea/logs/deploy-minimal.log 2>&1
"""

if(raw_input("Do you want to deploy a full server? (includes development tools) [n]: ") == "y"):
	cloud_config = cloud_config.format(newuser)
else:
	cloud_config = minimal_config.format(newuser)

print '-----------------------------------------------------------------'
print cloud_config
print '-----------------------------------------------------------------'

if(raw_input("Cloud init good to go? [y]: ") or "y" != "y"):
	print "Exiting."
	sys.exit(0)

newestimageid = 0
images = json.loads(requests.get("https://api.digitalocean.com/v2/images?per_page=999&type=distribution", headers=headers).content)
for image in images["images"]:
	if image["distribution"] == "CentOS":
		print "\nCentOS Image"
		print "id: " + str(image["id"])
		print "name: " + image["name"]
		print "created_at: " + image["created_at"]
		if image["id"] > newestimageid:
			newestimageid = image["id"]

print '-----------------------------------------------------------------'

newdropletid = raw_input("Enter an image id (newest is recommended) [" + str(newestimageid) + "]: ") or str(newestimageid)
newdroplet = rprint(requests.post("https://api.digitalocean.com/v2/droplets", headers=headers, json={
	"name":newname,
	"region":"sfo1",
	"size":"512mb",
	"image":newdropletid,
	"ssh_keys":None,
	"backups":False,
	"ipv6":False,
	"private_networking":False,
	"user_data":cloud_config
}))

print '-----------------------------------------------------------------'
