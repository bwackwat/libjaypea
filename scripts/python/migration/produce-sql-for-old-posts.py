#!/bin/python

import requests, json

thread_id = 1
owner_id = 0

posts = json.loads(requests.post("https://bwackwat.com/api/blog", json={"username":"bwackwat"}, verify=False).content)["result"]

with open("posts.sql", "w+") as f:
	for post in posts:
		f.write("INSERT INTO messages (thread_id, owner_id, title, content, created_on) VALUES (%s, %s, %s, %s, %s);\n\n"
		 % (thread_id, owner_id, json.dumps(post["title"]), json.dumps(post["content"]), json.dumps(post["created_on"])))
