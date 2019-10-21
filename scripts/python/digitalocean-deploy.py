#!/bin/python

import requests, json, random, string, sys, os, subprocess, time, getpass, random

scriptdir = os.path.dirname(os.path.realpath(__file__))

headers = {
	"Content-Type":"application/json",
	"Authorization":"Bearer " + getpass.getpass("Enter your DigitalOcean API access token: ")
}

hostname = ""
path = ""
final_remarks = []

def generate_password():
	chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
	return "".join(chars[ord(c) % len(chars)] for c in os.urandom(32))

def rprint(response):
	print '-----------------------------------------------------------------'
	print response.status_code
	for key, value in response.headers.iteritems():
		print key + ": " + value
	try:
		response_json = json.loads(response.content)
		print json.dumps(response_json, indent=4)
		return response_json
	except:
		pass
	return {}

dbpassword = generate_password()

def delete_droplet():
	print "Fetching droplets via API..."
	droplets = rprint(requests.get("https://api.digitalocean.com/v2/droplets", headers=headers))

	print '-----------------------------------------------------------------'

	for droplet in droplets["droplets"]:
		print "\nDroplet:"
		print "Name: " + droplet["name"]
		print "Memory: " + droplet["size_slug"]
		print "Disk size: " + str(droplet["disk"]) + "GB"
		print "Id: " + str(droplet["id"])

	print '-----------------------------------------------------------------'
	
	dropletname = raw_input("Enter the name of the droplet which you want to delete: ");
	
	theid = 0
	for droplet in droplets["droplets"]:
		if droplet["name"] == dropletname:
			theid = droplet["id"]
	
	print 'Deleting droplet...'
	rprint(requests.delete("https://api.digitalocean.com/v2/droplets/" + str(theid), headers=headers))
	print '-----------------------------------------------------------------'
	final_remarks.append("You deleted a droplet named " + dropletname)


def deploy_monad_new_smallvm():
	newuser = raw_input("Enter a username for the application to run under: ") or "bwackwat"
	newname = raw_input("Enter a name for the droplet: ") or "jph2"
	hostname = raw_input("Enter a hostname: ") or "jph2.net"
	application = raw_input("Enter a libjaypea application name[jph2]: ") or "jph2"
	email = raw_input("Enter your email address: ")
	sendgrid_key = raw_input("Enter a SendGrid key for sending emails: ")

	http_port = random.randrange(10000, 20000)

	https_port = random.randrange(10000, 20000)
	while https_port == http_port:
		https_port = random.randrange(10000, 20000)
		
	tanks_port = random.randrange(10000, 20000)
	while tanks_port == http_port or tanks_port == https_port:
		tanks_port = random.randrange(10000, 20000)

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

# - semanage port -a -t ssh_port_t -p tcp 10022
# - sed -i 's/^#Port .*/Port 10022/' /etc/ssh/sshd_config

	configuration_json = """
{
	\"hostname\": \"{0}\",
	\"public_directory\": \"../affable-escapade\",
	\"http_port\": \"{1}\",
	\"https_port\": \"{2}\",
	\"tanks_port\": \"{3}\",
	\"admin_email\": \"{4}\",
	\"sendgrid_key\": \"{5}\",
	\"postgresql_connection\": \"dbname=webservice user=postgres password={6}\",
	\"ssl_certificate\": \"artifacts/ssl.crt\",
	\"ssl_private_key\":\"artifacts/ssl.key\",
	\"keyfile\":\"extras/keyfile\"
}
"""

	cloud_config = """
#cloud-config

runcmd:
 - useradd -d /opt/libjaypea -s /bin/false -r {0}
 - echo "{3}" >> ~/.ssh/authorized_keys
 - sed -i 's/^PasswordAuthentication yes/PasswordAuthentication no/g' /etc/ssh/sshd_config
 - yum -y install git
 - git clone https://github.com/bwackwat/libjaypea /opt/libjaypea
 - echo "{8}" >> /opt/libjaypea/artifacts/configuration.json
 - git clone https://github.com/bwackwat/affable-escapade /opt/affable-escapade
 - chmod +x /opt/libjaypea/scripts/deploy.sh
 - /opt/libjaypea/scripts/deploy.sh /opt/libjaypea {0} {1} {2} {4} {5} {6} {7} > /opt/libjaypea/logs/deploy.log 2>&1
 - reboot
"""

	minimal_config = """
#cloud-config

runcmd:
 - yum -y install wget
 - cd /opt/libjaypea
 - wget https://raw.githubusercontent.com/bwackwat/libjaypea/master/scripts/deploy-minimal.sh
 - chmod +x deploy-minimal.sh
 - /opt/libjaypea/deploy-minimal.sh /opt/libjaypea {0} > /opt/libjaypea/logs/deploy-minimal.log 2>&1
	"""

	if((raw_input("Do you want to deploy a minimal server? (downloads from build server) [y]: ") or "y") == "y"):
		cloud_config = minimal_config.format(newuser)
	else:
		cloud_config = cloud_config.format(newuser, application, hostname, key.strip(), dbpassword, http_port, https_port,\
		tanks_port, configuration_json.format(hostname, http_port, https_port, tanks_port, email, sendgrid_key, dbpassword))

	print '-----------------------------------------------------------------'
	print cloud_config
	print '-----------------------------------------------------------------'

	if((raw_input("Cloud init good to go? [y]: ") or "y") != "y"):
		print "Exiting."
		sys.exit(0)

	print "Fetching images via API..."

	newestimageid = 0
	#images = rprint(requests.get("https://api.digitalocean.com/v2/images?per_page=999&type=distribution", headers=headers))
	images = json.loads(requests.get("https://api.digitalocean.com/v2/images?per_page=999&type=distribution", headers=headers).content)
	for image in images["images"]:
		if image["distribution"] == "CentOS" and "7" in image["name"]:
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

	print("ssh -i " + path + " " + hostname + " -p 10022 \n AND \n Your new database password is " + dbpassword)
	final_remarks.append("To connect use: ssh -i " + path + " " + hostname)
	final_remarks.append("Your new database password is " + dbpassword)
	
	print '-----------------------------------------------------------------'


def switch_floating_ip():
	print "Fetching droplets via API..."
	droplets = rprint(requests.get("https://api.digitalocean.com/v2/droplets", headers=headers))

	print '-----------------------------------------------------------------'

	for droplet in droplets["droplets"]:
		print "\nDroplet:"
		print "Name: " + droplet["name"]
		print "Memory: " + droplet["size_slug"]
		print "Disk size: " + str(droplet["disk"]) + "GB"
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
	final_remarks.append("You moved a droplet named " + dropletname + " to floating IP " + newfip)


if((raw_input("Would you like to delete a DigitalOcean droplet? [y]: ") or "y") == "y"):
	delete_droplet()

if((raw_input("Would you like to deploy libjaypea on a DigitalOcean CentOS 7 droplet? [y]: ") or "y") == "y"):
	deploy_monad_new_smallvm()

if((raw_input("Would you like to switch a DigitalOcean floating IP to a droplet? [y]: ") or "y") == "y"):
	switch_floating_ip()

for item in final_remarks:
	print item


