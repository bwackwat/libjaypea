#!/usr/bin/python3

import requests, json, time, datetime, traceback, sys

if len(sys.argv) < 3:
	print("Usage: git-commit-checker.py <repository owner> <repository name>")
	sys.exit(1)

owner = sys.argv[1]
name = sys.argv[2]
branches = ["master"]
latest_commits = {}

for branch in branches:
	commit_file = "artifacts/" + name + "." + branch + ".latest.commit"
	try:
		with open(commit_file, "r") as f:
			latest_commits[branch] = f.read()
	except:
		pass

def get_latest_commit(branch):
	url = "https://api.github.com/repos/" + owner + "/" + name + "/git/refs/heads/" + branch
	try:
		return json.loads(requests.get(url, timeout=5).content)["object"]["sha"]
	except:
		print("-" * 60)
		print(str(datetime.datetime.now()))
		traceback.print_exc(file=sys.stdout)
		print("-" * 60)
		return 0

while True:
	for branch in branches:
		new_commit = get_latest_commit(branch)
		if new_commit == 0 or (branch in latest_commits and new_commit == latest_commits[branch]):
			continue
		commit_file = "artifacts/" + name + "." + branch + ".latest.commit"
		with open(commit_file, "w") as f:
			f.write(new_commit)
		print("Wrote " + new_commit + " to " + commit_file + " (" + str(datetime.datetime.now()) + ")")
		latest_commits[branch] = new_commit
	time.sleep(120)
