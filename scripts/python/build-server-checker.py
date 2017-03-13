#!/bin/python

import sys, requests, time, datetime, traceback

branch = sys.argv[1]
print "Checking commit for latest build for branch " + branch
commit_file = "libjaypea." + branch + ".latest.commit"
url = "https://build.bwackwat.com/build/libjaypea." + branch + ".latest.commit"
last_commit = ""

while True:
	try:
		latest_commit = requests.get(url).content
	except:
		print "-" * 60
		print str(datetime.datetime.now())
		traceback.print_exc(file=sys.stdout)
		print "-" * 60
		latest_commit = last_commit
	if latest_commit != last_commit:
		with open(commit_file, "w") as f:
			f.write(latest_commit)
		last_commit = latest_commit
		print "Wrote " + latest_commit + " to " + commit_file + " (" + str(datetime.datetime.now()) + ")"
	time.sleep(60)
