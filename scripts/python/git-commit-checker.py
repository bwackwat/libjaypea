#!/bin/python

import requests, json, time, datetime, traceback, sys

branches = ["master"]
latest_commits = {}

def get_commit(branch):
	url = "https://api.github.com/repos/bwackwat/libjaypea/git/refs/heads/" + branch
	try:
		return json.loads(requests.get(url).content)["object"]["sha"]
	except:
		print "-" * 60
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
			commit_file = "artifacts/" + branch + ".latest.commit"
			with open(commit_file, "w") as f:
				f.write(latest_commit)
			print "Wrote " + latest_commit + " to " + commit_file + " (" + str(datetime.datetime.now()) + ")"
	time.sleep(60)
