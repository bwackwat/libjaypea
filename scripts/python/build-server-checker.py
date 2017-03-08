#!/bin/python

import sys, requests, time, datetime

branch = sys.argv[1]
print "Checking commit for latest build for branch " + branch
commit_file = branch + ".latest.commit"
url = "https://build.bwackwat.com/build/" + branch + ".latest.commit"
last_commit = ""

while True:
	latest_commit = requests.get(url).content
	if latest_commit != last_commit:
		with open(commit_file, "w") as f:
			f.write(latest_commit)
		last_commit = latest_commit
		print "Wrote " + latest_commit + " to " + commit_file + " (" + datetime.datetime.now() + ")"
	time.sleep(60)
