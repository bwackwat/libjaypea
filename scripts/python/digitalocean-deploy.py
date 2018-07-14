#!/bin/python

import requests, json, random, string, sys, os, subprocess, time

scriptdir = os.path.dirname(os.path.realpath(__file__))

headers = {
	"Content-Type":"application/json",
	"Authorization":"Bearer " + raw_input("Enter your DigitalOcean API access token: ")
}

hostname = ""
path = ""

def generate_password():
	chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
	return "".join(chars[ord(c) % len(chars)] for c in os.urandom(32))

def rprint(response):
	print '-----------------------------------------------------------------'
	print response.status_code
	for key, value in response.headers.iteritems():
		print key + ": " + value
	response_json = json.loads(response.content)
	print json.dumps(response_json, indent=4)
	return response_json

dbpassword = generate_password()
final_remark = ""

def deploy_monad_new_smallvm():
	newuser = raw_input("Enter a username for the application to run under: ")
	newname = raw_input("Enter a name for the droplet [" + newuser + "]: ") or newuser
	hostname = raw_input("Enter a hostname: ")
	application = raw_input("Enter a libjaypea application name[jph2]: ") or "jph2"

	key = ""
	print "If the file path below doesn't exist or is invalid, a key will be generated and provided."
	count = 0
	path = "artifacts/" + hostname + "_key" + str(count)
	while os.path.exists(path):
		count += 1
		path = "artifacts/" + hostname + "_key" + str(count)

	subprocess.call(["ssh-keygen", "-f", path, "-N", ""])

	with open(path + ".pub") as f:
		key = f.read()

	cloud_config = """
#cloud-config

runcmd:
 - useradd {0}
 - echo "{3}" >> ~/.ssh/authorized_keys
 - sed -i 's/^#Port .*/Port 10022/' /etc/ssh/sshd_config
 - sed -i 's/^PasswordAuthentication yes/PasswordAuthentication no/g' /etc/ssh/sshd_config
 - service sshd restart
 - yum -y install git
 - git clone https://github.com/bwackwat/libjaypea /opt/libjaypea
 - mkdir -p /opt/libjaypea/logs
 - chmod +x /opt/libjaypea/scripts/deploy.sh
 - /opt/libjaypea/scripts/deploy.sh /opt/libjaypea {0} {1} {2} > /opt/libjaypea/logs/deploy.log 2>&1
 - /opt/libjaypea/scripts/extra/pgsql-centos7.sh {4} > /opt/libjaypea/logs/deploy.log 2>&1
 - reboot
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

	if((raw_input("Do you want to deploy a minimal server? (downloads from build server) [y]: ") or "y") == "y"):
		cloud_config = minimal_config.format(newuser)
	else:
		cloud_config = cloud_config.format(newuser, application, hostname, key.strip(), dbpassword)

	print '-----------------------------------------------------------------'
	print cloud_config
	print '-----------------------------------------------------------------'

	if((raw_input("Cloud init good to go? [y]: ") or "y") != "y"):
		print "Exiting."
		sys.exit(0)

	print "Fetching images via API..."

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

	print "Creating droplet via API..."

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

	final_remark = "ssh -i " + path + " " + hostname + " -p 10022 \n AND \n Your new database password is " + dbpassword
	
	print '-----------------------------------------------------------------'


def switch_floating_ip():
	print "Fetching droplets via API..."
	droplets = rprint(requests.get("https://api.digitalocean.com/v2/droplets", headers=headers))

	print '-----------------------------------------------------------------'

	for droplet in droplets["droplets"]:
		print "\nDroplet:"
		print "Name: " + droplet["name"]
		print "Memory: " + droplet["size_slug"]
		print "Disk size (GB?): " + str(droplet["disk"])
		print "Id: " + str(droplet["id"])

	print '-----------------------------------------------------------------'
	
	dropletname = raw_input("Enter the name of the droplet which you want to assign a floating IP to: ");
	newid = 0
	for droplet in droplets["droplets"]:
		if droplet["name"] == dropletname:
			newid = droplet["id"]

	if newid == 0:
		print "Bad droplet name."
		sys.exit(0)

	print '-----------------------------------------------------------------'

	print "Fetching floating IPs via API..."
	fips = json.loads(requests.get("https://api.digitalocean.com/v2/floating_ips?page=1&per_page=20", headers=headers).content)
	for fip in fips["floating_ips"]:
		print "\nFloating IP:"
		print fip["ip"]
		print "Droplet: " + (fip["droplet"]["name"] if fip["droplet"] else "UNASSIGNED")

	print '-----------------------------------------------------------------'

	newfip = raw_input("If you want to assign a floating ip to the droplet, enter the ip [n]: ") or "n";
	if(newfip != "n"):
		print "Setting new floating IP via API..."
		rprint(requests.post("https://api.digitalocean.com/v2/floating_ips/" + newfip + " /actions", headers=headers, json={"type":"assign","droplet_id":newid}))

	print '-----------------------------------------------------------------'


if((raw_input("Would you like to deploy libjaypea on a DigitalOcean CentOS 7 VM? [y]: ") or "y") == "y"):
	deploy_monad_new_smallvm()

if((raw_input("Would you like to switch a DigitalOcean floating IP? [y]: ") or "y") == "y"):
	switch_floating_ip()

print '-----------------------------------------------------------------'
print final_remark
print '-----------------------------------------------------------------'

