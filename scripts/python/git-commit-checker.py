#!/bin/python

import requests, json, time, datetime, traceback, sys

if len(sys.argv) < 3:
	print "Usage: git-commit-checker.py <repository owner> <repository name>"
	sys.exit(1)

owner = sys.argv[1]
name = sys.argv[2]
branches = ["master"]
latest_commits = {}

def get_commit(branch):
	url = "https://api.github.com/repos/" + owner + "/" + name + "/git/refs/heads/" + branch
	try:
		return json.loads(requests.get(url).content)["object"]["sha"]
	except:
		print "-" * 60
		print str(datetime.datetime.now())
		traceback.print_exc(file=sys.stdout)
		print "-" * 60
		return 0

while True:
	for branch in branches:
		latest_commit = get_commit(branch)
		if latest_commit == 0:
			continue
		if branch not in latest_commits or latest_commits[branch] != latest_commit:
			latest_commits[branch] = latest_commit
			commit_file = "artifacts/" + name + "." + branch + ".latest.commit"
			with open(commit_file, "w") as f:
				f.write(latest_commit)
			print "Wrote " + latest_commit + " to " + commit_file + " (" + str(datetime.datetime.now()) + ")"
	time.sleep(120)
