#!/bin/python

import sys, requests, time, datetime, traceback

branch = sys.argv[1]
print "Checking commit for latest build for branch " + branch
commit_file = "libjaypea." + branch + ".latest.commit"
url = "https://build.bwackwat.com/build/libjaypea." + branch + ".latest.commit"
latest_commit = ""

try:
	with open(commit_file, "r") as f:
		latest_commit = f.read()
except:
	with open(commit_file, "w") as f:
		f.write(latest_commit)

while True:
	try:
		new_commit = requests.get(url).content
	except:
		print "-" * 60
		print str(datetime.datetime.now())
		traceback.print_exc(file=sys.stdout)
		print "-" * 60
		new_commit = latest_commit
	if new_commit != latest_commit:
		with open(commit_file, "w") as f:
			f.write(new_commit)
		print "Wrote " + new_commit + " to " + commit_file + " (" + str(datetime.datetime.now()) + ")"
		latest_commit = new_commit
	time.sleep(60)
