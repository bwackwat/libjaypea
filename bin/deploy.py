#!/bin/python

import requests, json

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

images = json.loads(requests.get("https://api.digitalocean.com/v2/images?per_page=999&type=distribution", headers=headers).content)

print '----------------------------------------------'

for image in images["images"]:
	if image["distribution"] == "CentOS":
		print "\nCentOS Image"
		print "id: " + str(image["id"])
		print "name: " + image["name"]
		print "created_at: " + image["created_at"]

print '----------------------------------------------'

cloud_config = """
#cloud-config

runcmd:
 - yum -y install git
 - mkdir -p /opt/libjaypea
 - git clone https://github.ocm/bwackwat/libjaypea /opt/libjaypea
 - chmod +x /opt/libjaypea/setup-centos7.sh
 - /opt/libjaypea/bin/setup-centos7.sh > setup-centos7.log 2>&1
 - chmod +x /opt/libjaypea/build.sh
 - /opt/libjaypea/bin/build.sh > build.log 2>&1
 - /opt/libjaypea/bin/modern-web-monad
"""

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
