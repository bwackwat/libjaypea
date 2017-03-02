#!/bin/python

import requests, json, random, string, sys, os

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

fips = json.loads(requests.get("https://api.digitalocean.com/v2/floating_ips?page=1&per_page=20", headers=headers).content)
for fip in fips["floating_ips"]:
	print "\nFloating IP:"
	print fip["ip"]
	print "Droplet: " + (fip["droplet"]["name"] if fip["droplet"] else "UNASSIGNED")

print '-----------------------------------------------------------------'

newfip = raw_input("If you want to assign a floating ip to the droplet, enter the ip [n]: ") or "n";
if(newfip != "n"):
	rprint(requests.post("https://api.digitalocean.com/v2/floating_ips/" + newfip + " /actions", headers=headers, json={"type":"assign","droplet_id":newid}))

print '-----------------------------------------------------------------'
