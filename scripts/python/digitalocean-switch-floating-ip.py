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

droplets = json.loads(requests.get("https://api.digitalocean.com/v2/droplets", headers=headers).content)
for droplet in droplets["droplets"]:
	print "\nDroplet:"
	print "Name: " + droplet["name"]
	print "Memory: " + droplet["size_slug"]
	print "Disk size (GB?): " + droplet["disk"]
	print "Id: " + droplet["id"]

print '-----------------------------------------------------------------'
	
newid = raw_input("Enter the id of the droplet which you want to assign a floating IP to: ");
if not newid:
	print "Bad id."
	sys.exit(0)

print '-----------------------------------------------------------------'

fips = json.loads(requests.get("https://api.digitalocean.com/v2/floating_ips?page=1&per_page=20", headers=headers).content)
for fip in fips["floating_ips"]:
	print "\nFloating IP:"
	print fip["ip"]
	print "Droplet: " + (fip["droplet"]["name"] if fip["droplet"] else "UNASSIGNED")

print '-----------------------------------------------------------------'

newfip = raw_input("If you want to assign the new droplet to a floating ip, enter the ip [n]: ") or "n";
if(newfip != "n"):
	rprint(requests.post("https://api.digitalocean.com/v2/floating_ips/" + newfip + " /actions", headers=headers, json={"type":"assign","droplet_id":newid}))

print '-----------------------------------------------------------------'
